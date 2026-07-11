/*
 * This file is part of VRto3D.
 *
 * VRto3D is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VRto3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with VRto3D. If not, see <http://www.gnu.org/licenses/>.
 */

// Linux evdev backend for vrto3dlib/input_state.h.
//
// The driver runs inside vrserver while the game window has focus, so input
// must be read globally: one reader thread epolls every usable
// /dev/input/event* device (plus an inotify watch on /dev/input for
// hotplug) and folds events into a shared snapshot that the API functions
// read under a mutex. Requires membership in the `input` group.

#ifdef __linux__

#include "vrto3dlib/input_state.h"
#include "vrto3dlib/key_codes.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

// debug_log.hpp is Windows-only (windows.h, OutputDebugString); vrserver
// captures stderr into its log, so plain fprintf is the Linux fallback.
#define INPUT_LOG(...)                                    \
    do {                                                  \
        std::fprintf(stderr, "[vrto3d-input] " __VA_ARGS__); \
        std::fprintf(stderr, "\n");                       \
    } while (0)

namespace vrto3d::input {

namespace {

constexpr size_t kLongBits = 8 * sizeof(unsigned long);

constexpr size_t BitWords(size_t bit_count)
{
    return (bit_count + kLongBits - 1) / kLongBits;
}

bool TestBit(const unsigned long* bits, unsigned int bit)
{
    return (bits[bit / kLongBits] >> (bit % kLongBits)) & 1ul;
}

// Evdev letter-key codes indexed by (letter - 'A'). KEY_A..KEY_Z are laid
// out by physical QWERTY position, not alphabetically, so an explicit table
// is required for the VK <-> evdev translation and keyboard classification.
constexpr int kLetterKeys[26] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
};

//-----------------------------------------------------------------------------
// Purpose: VK <-> evdev translation tables, built once
//-----------------------------------------------------------------------------
struct VkTables {
    int vk_to_ev[256] = {};      // 0 = no single-code mapping
    int ev_to_vk[KEY_CNT] = {};  // 0 = no VK

    void Map(int vk, int ev)
    {
        if (vk >= 0 && vk < 256) {
            vk_to_ev[vk] = ev;
        }
        if (ev > 0 && ev < KEY_CNT && ev_to_vk[ev] == 0) {
            ev_to_vk[ev] = vk;
        }
    }

    VkTables()
    {
        constexpr int digit_keys[10] = {
            KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
            KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
        };
        constexpr int fn_keys[24] = {
            KEY_F1,  KEY_F2,  KEY_F3,  KEY_F4,  KEY_F5,  KEY_F6,
            KEY_F7,  KEY_F8,  KEY_F9,  KEY_F10, KEY_F11, KEY_F12,
            KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18,
            KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24,
        };
        constexpr int kp_keys[10] = {
            KEY_KP0, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4,
            KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9,
        };
        for (int i = 0; i < 26; ++i) Map('A' + i, kLetterKeys[i]);
        for (int i = 0; i < 10; ++i) Map('0' + i, digit_keys[i]);
        for (int i = 0; i < 24; ++i) Map(VK_F1 + i, fn_keys[i]);
        for (int i = 0; i < 10; ++i) Map(VK_NUMPAD0 + i, kp_keys[i]);
        Map(VK_MULTIPLY, KEY_KPASTERISK);
        Map(VK_ADD, KEY_KPPLUS);
        Map(VK_SUBTRACT, KEY_KPMINUS);
        Map(VK_DECIMAL, KEY_KPDOT);
        Map(VK_DIVIDE, KEY_KPSLASH);
        Map(VK_BACK, KEY_BACKSPACE);
        Map(VK_TAB, KEY_TAB);
        Map(VK_RETURN, KEY_ENTER);
        Map(VK_PAUSE, KEY_PAUSE);
        Map(VK_CAPITAL, KEY_CAPSLOCK);
        Map(VK_ESCAPE, KEY_ESC);
        Map(VK_SPACE, KEY_SPACE);
        Map(VK_PRIOR, KEY_PAGEUP);
        Map(VK_NEXT, KEY_PAGEDOWN);
        Map(VK_END, KEY_END);
        Map(VK_HOME, KEY_HOME);
        Map(VK_LEFT, KEY_LEFT);
        Map(VK_UP, KEY_UP);
        Map(VK_RIGHT, KEY_RIGHT);
        Map(VK_DOWN, KEY_DOWN);
        Map(VK_SNAPSHOT, KEY_SYSRQ);  // PrintScreen
        Map(VK_INSERT, KEY_INSERT);
        Map(VK_DELETE, KEY_DELETE);
        // Sided modifiers; generic VK_SHIFT/CONTROL/MENU check both sides
        // in IsKeyDown, and the reverse table reports the sided VK.
        Map(VK_LSHIFT, KEY_LEFTSHIFT);
        Map(VK_RSHIFT, KEY_RIGHTSHIFT);
        Map(VK_LCONTROL, KEY_LEFTCTRL);
        Map(VK_RCONTROL, KEY_RIGHTCTRL);
        Map(VK_LMENU, KEY_LEFTALT);
        Map(VK_RMENU, KEY_RIGHTALT);
        Map(VK_LWIN, KEY_LEFTMETA);
        Map(VK_RWIN, KEY_RIGHTMETA);
        Map(VK_OEM_MINUS, KEY_MINUS);
        Map(VK_OEM_PLUS, KEY_EQUAL);
        Map(VK_OEM_4, KEY_LEFTBRACE);
        Map(VK_OEM_6, KEY_RIGHTBRACE);
        Map(VK_OEM_1, KEY_SEMICOLON);
        Map(VK_OEM_2, KEY_SLASH);
        Map(VK_OEM_3, KEY_GRAVE);
        Map(VK_OEM_5, KEY_BACKSLASH);
        Map(VK_OEM_7, KEY_APOSTROPHE);
        Map(VK_OEM_COMMA, KEY_COMMA);
        Map(VK_OEM_PERIOD, KEY_DOT);
        // Mouse buttons are EV_KEY codes too.
        Map(VK_LBUTTON, BTN_LEFT);
        Map(VK_RBUTTON, BTN_RIGHT);
        Map(VK_MBUTTON, BTN_MIDDLE);
        Map(VK_XBUTTON1, BTN_SIDE);
        Map(VK_XBUTTON2, BTN_EXTRA);
    }
};

const VkTables& Tables()
{
    static const VkTables tables;
    return tables;
}

//-----------------------------------------------------------------------------
// Purpose: xpad-convention gamepad button -> XINPUT_GAMEPAD_* bit
//-----------------------------------------------------------------------------
uint16_t PadButtonBit(uint16_t code)
{
    switch (code) {
        case BTN_SOUTH:      return XINPUT_GAMEPAD_A;
        case BTN_EAST:       return XINPUT_GAMEPAD_B;
        case BTN_WEST:       return XINPUT_GAMEPAD_X;
        case BTN_NORTH:      return XINPUT_GAMEPAD_Y;
        case BTN_TL:         return XINPUT_GAMEPAD_LEFT_SHOULDER;
        case BTN_TR:         return XINPUT_GAMEPAD_RIGHT_SHOULDER;
        case BTN_SELECT:     return XINPUT_GAMEPAD_BACK;
        case BTN_START:      return XINPUT_GAMEPAD_START;
        case BTN_MODE:       return XINPUT_GAMEPAD_GUIDE;
        case BTN_THUMBL:     return XINPUT_GAMEPAD_LEFT_THUMB;
        case BTN_THUMBR:     return XINPUT_GAMEPAD_RIGHT_THUMB;
        // Some pads report the dpad as buttons instead of ABS_HAT0X/Y.
        case BTN_DPAD_UP:    return XINPUT_GAMEPAD_DPAD_UP;
        case BTN_DPAD_DOWN:  return XINPUT_GAMEPAD_DPAD_DOWN;
        case BTN_DPAD_LEFT:  return XINPUT_GAMEPAD_DPAD_LEFT;
        case BTN_DPAD_RIGHT: return XINPUT_GAMEPAD_DPAD_RIGHT;
        default:             return 0;
    }
}

struct AxisRange {
    int32_t min = 0;
    int32_t max = 0;
    bool valid = false;
};

int16_t ScaleStick(const AxisRange& range, int32_t value, bool invert)
{
    if (!range.valid || range.max <= range.min) {
        return 0;
    }
    const double t = double(value - range.min) / double(range.max - range.min);
    int32_t scaled = static_cast<int32_t>(t * 65535.0 + 0.5) - 32768;
    if (invert) {
        scaled = -scaled - 1;  // evdev Y grows downward; XInput up is positive
    }
    return static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
}

uint8_t ScaleTrigger(const AxisRange& range, int32_t value)
{
    if (!range.valid || range.max <= range.min) {
        return value ? 255 : 0;
    }
    const double t = double(value - range.min) / double(range.max - range.min);
    return static_cast<uint8_t>(std::clamp(static_cast<int32_t>(t * 255.0 + 0.5), 0, 255));
}

struct Device {
    int fd = -1;
    std::string path;
    bool is_keyboard = false;
    bool is_mouse = false;
    bool is_gamepad = false;
    std::bitset<KEY_CNT> keys;  // per-device, so removal releases held keys
    GamepadState pad;           // valid when is_gamepad
    std::array<AxisRange, ABS_CNT> abs{};
};

constexpr int kMaxKeyEvents = 256;

struct Shared {
    std::mutex mutex;

    std::vector<Device> devices;

    // Keyboard/mouse buttons, OR-merged across devices. key_count tracks
    // how many devices hold each key so a release on one keyboard doesn't
    // clear a key still held on another.
    std::array<uint16_t, KEY_CNT> key_count{};
    std::bitset<KEY_CNT> pressed;

    // Virtual mouse cursor, clamped to the SetMouseRegion box.
    int32_t mouse_x = 0;
    int32_t mouse_y = 0;
    int32_t region_w = 0;
    int32_t region_h = 0;
    int32_t wheel = 0;

    // Edge-event ring buffer for the OSD input pump.
    KeyEvent events[kMaxKeyEvents];
    int ev_tail = 0;
    int ev_count = 0;

    std::string typed;  // pending UTF-8 text

    // xkbcommon (null when init failed: typed text silently disabled).
    xkb_context* xkb_ctx = nullptr;
    xkb_keymap* xkb_map = nullptr;
    xkb_state* xkb_st = nullptr;
};

Shared g;

std::mutex g_lifecycle_mutex;
bool g_started = false;
std::thread g_thread;
std::atomic<bool> g_running{false};
std::atomic<bool> g_have_keyboard{false};

int g_epoll_fd = -1;
int g_inotify_fd = -1;

//-----------------------------------------------------------------------------
// Purpose: Push an edge event, dropping the oldest when the ring is full
//-----------------------------------------------------------------------------
void PushEventLocked(uint16_t code, bool down)
{
    if (g.ev_count == kMaxKeyEvents) {
        g.ev_tail = (g.ev_tail + 1) % kMaxKeyEvents;
        --g.ev_count;
    }
    KeyEvent& e = g.events[(g.ev_tail + g.ev_count) % kMaxKeyEvents];
    e.vk = Tables().ev_to_vk[code];
    e.evdev_code = code;
    e.down = down;
    ++g.ev_count;
}

//-----------------------------------------------------------------------------
// Purpose: Translate a key press into UTF-8 via the current xkb state
//-----------------------------------------------------------------------------
void AppendTypedLocked(uint16_t code)
{
    if (!g.xkb_st) {
        return;
    }
    char buf[32];
    int n = xkb_state_key_get_utf8(g.xkb_st, code + 8, buf, sizeof(buf));
    if (n <= 0) {
        return;
    }
    n = std::min(n, static_cast<int>(sizeof(buf)) - 1);
    // Skip C0 control characters (Enter/Escape/Backspace map to those);
    // the OSD handles them as key events, not text.
    if (n == 1 && (static_cast<unsigned char>(buf[0]) < 0x20 || buf[0] == 0x7F)) {
        return;
    }
    if (g.typed.size() < 4096) {
        g.typed.append(buf, n);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Fold one EV_KEY event into shared state. emit=false replays a
//          device's initial EVIOCGKEY snapshot without edge events or text.
//-----------------------------------------------------------------------------
void HandleKeyLocked(Device& dev, uint16_t code, int32_t value, bool emit)
{
    if (code >= KEY_CNT) {
        return;
    }

    if (dev.is_gamepad) {
        const uint16_t bit = PadButtonBit(code);
        if (bit) {
            if (value == 1) {
                dev.pad.wButtons |= bit;
            } else if (value == 0) {
                dev.pad.wButtons &= ~bit;
            }
            return;  // gamepad buttons never feed the keyboard bitset
        }
    }
    if (!dev.is_keyboard && !dev.is_mouse) {
        return;
    }

    if (value == 2) {
        // Autorepeat: no state change, but repeats do generate characters.
        if (emit && dev.is_keyboard) {
            AppendTypedLocked(code);
        }
        return;
    }

    if (value == 1) {
        if (dev.keys[code]) {
            return;
        }
        dev.keys[code] = true;
        if (g.key_count[code]++ == 0) {
            g.pressed[code] = true;
            if (emit) {
                PushEventLocked(code, true);
            }
        }
        if (dev.is_keyboard && g.xkb_st) {
            if (emit) {
                AppendTypedLocked(code);  // query before the state update
            }
            xkb_state_update_key(g.xkb_st, code + 8, XKB_KEY_DOWN);
        }
    } else if (value == 0) {
        if (!dev.keys[code]) {
            return;
        }
        dev.keys[code] = false;
        if (g.key_count[code] > 0 && --g.key_count[code] == 0) {
            g.pressed[code] = false;
            if (emit) {
                PushEventLocked(code, false);
            }
        }
        if (dev.is_keyboard && g.xkb_st) {
            xkb_state_update_key(g.xkb_st, code + 8, XKB_KEY_UP);
        }
    }
}

void HandleRelLocked(uint16_t code, int32_t value)
{
    switch (code) {
        case REL_X:
            g.mouse_x = std::clamp(g.mouse_x + value, 0, std::max(0, g.region_w - 1));
            break;
        case REL_Y:
            g.mouse_y = std::clamp(g.mouse_y + value, 0, std::max(0, g.region_h - 1));
            break;
        case REL_WHEEL:
            g.wheel += value;
            break;
        default:
            break;
    }
}

void HandleAbsLocked(Device& dev, uint16_t code, int32_t value)
{
    if (code >= ABS_CNT) {
        return;
    }
    switch (code) {
        case ABS_X:
            dev.pad.sThumbLX = ScaleStick(dev.abs[ABS_X], value, false);
            break;
        case ABS_Y:
            dev.pad.sThumbLY = ScaleStick(dev.abs[ABS_Y], value, true);
            break;
        case ABS_RX:
            dev.pad.sThumbRX = ScaleStick(dev.abs[ABS_RX], value, false);
            break;
        case ABS_RY:
            dev.pad.sThumbRY = ScaleStick(dev.abs[ABS_RY], value, true);
            break;
        case ABS_Z:      // xpad convention
        case ABS_BRAKE:  // some HID pads
            dev.pad.bLeftTrigger = ScaleTrigger(dev.abs[code], value);
            break;
        case ABS_RZ:
        case ABS_GAS:
            dev.pad.bRightTrigger = ScaleTrigger(dev.abs[code], value);
            break;
        case ABS_HAT0X:
            dev.pad.wButtons &= ~(XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT);
            if (value < 0) {
                dev.pad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
            } else if (value > 0) {
                dev.pad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
            }
            break;
        case ABS_HAT0Y:  // -1 = up
            dev.pad.wButtons &= ~(XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN);
            if (value < 0) {
                dev.pad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
            } else if (value > 0) {
                dev.pad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
            }
            break;
        default:
            break;
    }
}

void ApplyEventLocked(Device& dev, const struct input_event& e, bool emit)
{
    switch (e.type) {
        case EV_KEY:
            HandleKeyLocked(dev, e.code, e.value, emit);
            break;
        case EV_REL:
            if (dev.is_mouse) {
                HandleRelLocked(e.code, e.value);
            }
            break;
        case EV_ABS:
            if (dev.is_gamepad) {
                HandleAbsLocked(dev, e.code, e.value);
            }
            break;
        default:
            break;
    }
}

//-----------------------------------------------------------------------------
// Purpose: Close a device, releasing any keys it still holds
//-----------------------------------------------------------------------------
void CloseDeviceLocked(Device& dev)
{
    for (int code = 0; code < KEY_CNT; ++code) {
        if (dev.keys[code]) {
            HandleKeyLocked(dev, static_cast<uint16_t>(code), 0, true);
        }
    }
    if (g_epoll_fd >= 0) {
        epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, dev.fd, nullptr);
    }
    close(dev.fd);
    dev.fd = -1;
}

void UpdateHaveKeyboardLocked()
{
    bool have = false;
    for (const Device& d : g.devices) {
        have = have || d.is_keyboard;
    }
    g_have_keyboard.store(have, std::memory_order_relaxed);
}

void RemoveDeviceLocked(int fd)
{
    for (size_t i = 0; i < g.devices.size(); ++i) {
        if (g.devices[i].fd == fd) {
            INPUT_LOG("removed %s", g.devices[i].path.c_str());
            CloseDeviceLocked(g.devices[i]);
            g.devices.erase(g.devices.begin() + i);
            break;
        }
    }
    UpdateHaveKeyboardLocked();
}

bool PathOpenLocked(const std::string& path)
{
    for (const Device& d : g.devices) {
        if (d.path == path) {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Purpose: Open + classify one /dev/input/event* node. A device may be
//          keyboard, mouse and gamepad at once; skip it if it is none.
//-----------------------------------------------------------------------------
void TryOpenDeviceLocked(const std::string& path)
{
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return;  // usually EACCES until udev applies the input-group ACL
    }

    unsigned long ev_bits[BitWords(EV_CNT)] = {};
    unsigned long key_bits[BitWords(KEY_CNT)] = {};
    unsigned long rel_bits[BitWords(REL_CNT)] = {};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        close(fd);
        return;
    }
    const bool has_key = TestBit(ev_bits, EV_KEY);
    const bool has_rel = TestBit(ev_bits, EV_REL);
    if (has_key) {
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
    }
    if (has_rel) {
        ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits);
    }

    Device dev;
    dev.fd = fd;
    dev.path = path;

    dev.is_keyboard = has_key;
    for (int i = 0; dev.is_keyboard && i < 26; ++i) {
        dev.is_keyboard = TestBit(key_bits, kLetterKeys[i]);
    }
    dev.is_mouse = has_rel && TestBit(rel_bits, REL_X) &&
                   has_key && TestBit(key_bits, BTN_LEFT);
    dev.is_gamepad = has_key && TestBit(key_bits, BTN_SOUTH);

    if (!dev.is_keyboard && !dev.is_mouse && !dev.is_gamepad) {
        close(fd);
        return;
    }

    if (dev.is_gamepad) {
        constexpr int pad_axes[] = {
            ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ,
            ABS_BRAKE, ABS_GAS, ABS_HAT0X, ABS_HAT0Y,
        };
        dev.pad.connected = true;
        for (const int axis : pad_axes) {
            struct input_absinfo info {};
            if (ioctl(fd, EVIOCGABS(axis), &info) == 0) {
                dev.abs[axis] = {info.minimum, info.maximum, true};
                HandleAbsLocked(dev, static_cast<uint16_t>(axis), info.value);
            }
        }
    }

    // Sync keys already held at open so state and xkb modifiers start right.
    unsigned long key_state[BitWords(KEY_CNT)] = {};
    if (ioctl(fd, EVIOCGKEY(sizeof(key_state)), key_state) >= 0) {
        for (int code = 0; code < KEY_CNT; ++code) {
            if (TestBit(key_state, code)) {
                HandleKeyLocked(dev, static_cast<uint16_t>(code), 1, false);
            }
        }
    }

    if (g_epoll_fd >= 0) {
        struct epoll_event ev {};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            close(fd);
            return;
        }
    }

    char name[128] = "?";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    INPUT_LOG("opened %s (%s)%s%s%s", path.c_str(), name,
              dev.is_keyboard ? " keyboard" : "",
              dev.is_mouse ? " mouse" : "",
              dev.is_gamepad ? " gamepad" : "");

    g.devices.push_back(std::move(dev));
}

//-----------------------------------------------------------------------------
// Purpose: Scan /dev/input for nodes not yet open. Removals are handled by
//          read errors / EPOLLHUP on the device fd, not here.
//-----------------------------------------------------------------------------
void RescanLocked()
{
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        return;
    }
    while (const struct dirent* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }
        const std::string path = std::string("/dev/input/") + entry->d_name;
        if (!PathOpenLocked(path)) {
            TryOpenDeviceLocked(path);
        }
    }
    closedir(dir);
    UpdateHaveKeyboardLocked();
}

Device* FindDeviceLocked(int fd)
{
    for (Device& d : g.devices) {
        if (d.fd == fd) {
            return &d;
        }
    }
    return nullptr;
}

void HandleDeviceReadable(int fd, uint32_t epoll_flags)
{
    std::lock_guard<std::mutex> lock(g.mutex);
    Device* dev = FindDeviceLocked(fd);
    if (!dev) {
        return;
    }

    bool dead = (epoll_flags & (EPOLLHUP | EPOLLERR)) != 0;
    struct input_event buf[64];
    while (!dead) {
        const ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                dead = true;  // ENODEV on unplug
            }
            break;
        }
        if (n == 0) {
            dead = true;
            break;
        }
        const int count = static_cast<int>(n / sizeof(struct input_event));
        for (int i = 0; i < count; ++i) {
            ApplyEventLocked(*dev, buf[i], true);
        }
    }

    if (dead) {
        RemoveDeviceLocked(fd);
    }
}

void DrainInotify()
{
    // Contents don't matter — any activity in /dev/input triggers a rescan.
    char buf[4096];
    while (read(g_inotify_fd, buf, sizeof(buf)) > 0) {
    }
}

//-----------------------------------------------------------------------------
// Purpose: Reader thread: epoll over device fds + inotify, with a debounced
//          rescan (IN_ATTRIB fires when udev grants group access post-CREATE)
//-----------------------------------------------------------------------------
void ReaderThread()
{
    using Clock = std::chrono::steady_clock;
    constexpr auto kRescanDebounce = std::chrono::milliseconds(500);

    bool rescan_pending = false;
    Clock::time_point rescan_at{};

    while (g_running.load(std::memory_order_relaxed)) {
        // Cap the wait so Stop() is honored promptly without a wakeup fd.
        int timeout_ms = 250;
        if (rescan_pending) {
            const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
                rescan_at - Clock::now()).count();
            timeout_ms = remain <= 0 ? 0 : std::min<int>(250, static_cast<int>(remain));
        }

        struct epoll_event events[16];
        const int n = epoll_wait(g_epoll_fd, events, 16, timeout_ms);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            INPUT_LOG("epoll_wait failed: %s", std::strerror(errno));
            break;
        }
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == g_inotify_fd) {
                DrainInotify();
                rescan_pending = true;
                rescan_at = Clock::now() + kRescanDebounce;
            } else {
                HandleDeviceReadable(events[i].data.fd, events[i].events);
            }
        }
        if (rescan_pending && Clock::now() >= rescan_at) {
            std::lock_guard<std::mutex> lock(g.mutex);
            RescanLocked();
            rescan_pending = false;
        }
    }
}

void InitXkbLocked()
{
    // Default rules honor XKB_DEFAULT_LAYOUT & friends from the environment.
    g.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (g.xkb_ctx) {
        g.xkb_map = xkb_keymap_new_from_names(g.xkb_ctx, nullptr,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    if (g.xkb_map) {
        g.xkb_st = xkb_state_new(g.xkb_map);
    }
    if (!g.xkb_st) {
        INPUT_LOG("xkbcommon init failed: typed text disabled");
        if (g.xkb_map) {
            xkb_keymap_unref(g.xkb_map);
            g.xkb_map = nullptr;
        }
        if (g.xkb_ctx) {
            xkb_context_unref(g.xkb_ctx);
            g.xkb_ctx = nullptr;
        }
    }
}

void ShutdownXkbLocked()
{
    if (g.xkb_st) {
        xkb_state_unref(g.xkb_st);
        g.xkb_st = nullptr;
    }
    if (g.xkb_map) {
        xkb_keymap_unref(g.xkb_map);
        g.xkb_map = nullptr;
    }
    if (g.xkb_ctx) {
        xkb_context_unref(g.xkb_ctx);
        g.xkb_ctx = nullptr;
    }
}

}  // namespace

bool Start()
{
    std::lock_guard<std::mutex> lifecycle(g_lifecycle_mutex);
    if (g_started) {
        return PermissionOk();
    }

    {
        std::lock_guard<std::mutex> lock(g.mutex);

        g_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (g_epoll_fd < 0) {
            INPUT_LOG("epoll_create1 failed: %s", std::strerror(errno));
            return false;
        }

        g_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (g_inotify_fd >= 0) {
            // IN_ATTRIB matters: udev applies group permissions after CREATE.
            if (inotify_add_watch(g_inotify_fd, "/dev/input",
                                  IN_CREATE | IN_DELETE | IN_ATTRIB) >= 0) {
                struct epoll_event ev {};
                ev.events = EPOLLIN;
                ev.data.fd = g_inotify_fd;
                epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_inotify_fd, &ev);
            } else {
                close(g_inotify_fd);
                g_inotify_fd = -1;
            }
        }
        if (g_inotify_fd < 0) {
            INPUT_LOG("inotify unavailable: device hotplug disabled");
        }

        InitXkbLocked();
        RescanLocked();
    }

    g_running.store(true, std::memory_order_relaxed);
    g_thread = std::thread(ReaderThread);
    g_started = true;

    if (!PermissionOk()) {
        INPUT_LOG("no keyboard device could be opened - is the user in the "
                  "`input` group? Hotkeys will not work.");
    }
    return PermissionOk();
}

void Stop()
{
    std::lock_guard<std::mutex> lifecycle(g_lifecycle_mutex);
    if (!g_started) {
        return;
    }

    g_running.store(false, std::memory_order_relaxed);
    if (g_thread.joinable()) {
        g_thread.join();
    }

    std::lock_guard<std::mutex> lock(g.mutex);
    for (Device& dev : g.devices) {
        if (dev.fd >= 0) {
            close(dev.fd);
        }
    }
    g.devices.clear();
    g.key_count.fill(0);
    g.pressed.reset();
    g.ev_tail = 0;
    g.ev_count = 0;
    g.typed.clear();
    g.wheel = 0;

    if (g_inotify_fd >= 0) {
        close(g_inotify_fd);
        g_inotify_fd = -1;
    }
    if (g_epoll_fd >= 0) {
        close(g_epoll_fd);
        g_epoll_fd = -1;
    }
    ShutdownXkbLocked();

    g_have_keyboard.store(false, std::memory_order_relaxed);
    g_started = false;
}

bool PermissionOk()
{
    return g_have_keyboard.load(std::memory_order_relaxed);
}

bool IsKeyDown(int vk)
{
    std::lock_guard<std::mutex> lock(g.mutex);
    switch (vk) {
        case VK_SHIFT:
            return g.pressed[KEY_LEFTSHIFT] || g.pressed[KEY_RIGHTSHIFT];
        case VK_CONTROL:
            return g.pressed[KEY_LEFTCTRL] || g.pressed[KEY_RIGHTCTRL];
        case VK_MENU:
            return g.pressed[KEY_LEFTALT] || g.pressed[KEY_RIGHTALT];
        default: {
            if (vk < 0 || vk >= 256) {
                return false;
            }
            const int code = Tables().vk_to_ev[vk];
            return code != 0 && g.pressed[code];
        }
    }
}

bool IsCtrlDown()
{
    return IsKeyDown(VK_CONTROL);
}

GamepadState GetGamepadState()
{
    std::lock_guard<std::mutex> lock(g.mutex);
    GamepadState merged;
    const auto max_magnitude = [](int16_t& dst, int16_t v) {
        if (std::abs(static_cast<int>(v)) > std::abs(static_cast<int>(dst))) {
            dst = v;
        }
    };
    for (const Device& dev : g.devices) {
        if (!dev.is_gamepad) {
            continue;
        }
        merged.connected = true;
        merged.wButtons |= dev.pad.wButtons;
        merged.bLeftTrigger = std::max(merged.bLeftTrigger, dev.pad.bLeftTrigger);
        merged.bRightTrigger = std::max(merged.bRightTrigger, dev.pad.bRightTrigger);
        max_magnitude(merged.sThumbLX, dev.pad.sThumbLX);
        max_magnitude(merged.sThumbLY, dev.pad.sThumbLY);
        max_magnitude(merged.sThumbRX, dev.pad.sThumbRX);
        max_magnitude(merged.sThumbRY, dev.pad.sThumbRY);
    }
    return merged;
}

MouseState GetMouseState()
{
    std::lock_guard<std::mutex> lock(g.mutex);
    MouseState ms;
    ms.x = g.mouse_x;
    ms.y = g.mouse_y;
    ms.wheel = g.wheel;
    g.wheel = 0;  // accumulated detents since last call
    ms.left = g.pressed[BTN_LEFT];
    ms.right = g.pressed[BTN_RIGHT];
    ms.middle = g.pressed[BTN_MIDDLE];
    ms.x1 = g.pressed[BTN_SIDE];
    ms.x2 = g.pressed[BTN_EXTRA];
    return ms;
}

void SetMouseRegion(int32_t width, int32_t height)
{
    std::lock_guard<std::mutex> lock(g.mutex);
    width = std::max<int32_t>(width, 1);
    height = std::max<int32_t>(height, 1);
    if (width == g.region_w && height == g.region_h) {
        return;
    }
    const bool first = (g.region_w == 0);
    g.region_w = width;
    g.region_h = height;
    if (first) {
        g.mouse_x = width / 2;  // start centered
        g.mouse_y = height / 2;
    } else {
        g.mouse_x = std::clamp<int32_t>(g.mouse_x, 0, width - 1);
        g.mouse_y = std::clamp<int32_t>(g.mouse_y, 0, height - 1);
    }
}

void WarpMouse(int32_t x, int32_t y)
{
    std::lock_guard<std::mutex> lock(g.mutex);
    g.mouse_x = std::clamp<int32_t>(x, 0, std::max<int32_t>(0, g.region_w - 1));
    g.mouse_y = std::clamp<int32_t>(y, 0, std::max<int32_t>(0, g.region_h - 1));
}

int DrainKeyEvents(KeyEvent* out, int max_events)
{
    if (!out || max_events <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g.mutex);
    const int n = std::min(max_events, g.ev_count);
    for (int i = 0; i < n; ++i) {
        out[i] = g.events[g.ev_tail];
        g.ev_tail = (g.ev_tail + 1) % kMaxKeyEvents;
    }
    g.ev_count -= n;
    return n;
}

int DrainTypedUtf8(char* out, int out_size)
{
    if (!out || out_size <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g.mutex);
    size_t n = std::min(g.typed.size(), static_cast<size_t>(out_size - 1));
    // Never split a UTF-8 sequence: back up past continuation bytes.
    while (n > 0 && n < g.typed.size() &&
           (static_cast<unsigned char>(g.typed[n]) & 0xC0) == 0x80) {
        --n;
    }
    std::memcpy(out, g.typed.data(), n);
    out[n] = '\0';
    g.typed.erase(0, n);
    return static_cast<int>(n);
}

}  // namespace vrto3d::input

#endif  // __linux__
