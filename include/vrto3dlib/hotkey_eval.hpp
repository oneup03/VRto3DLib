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
#pragma once

// Platform-neutral user-hotkey preset evaluator, shared by win32_helper.hpp
// and linux_helper.hpp (previously an identical ~120-line copy in each). The
// only per-platform difference was the "is this key down?" query, so it's a
// template parameter (`is_down(int vk) -> bool`); the gamepad button state is
// passed in as `xstate`.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "vrto3dlib/key_codes.h"     // HOLD / TOGGLE / SWITCH key-type constants
#include "vrto3dlib/stereo_config.h"

namespace vrto3d {

// Backend the evaluator calls to read/apply the live depth/convergence/FoV.
struct DepthConvBackend {
    float (*getDepth)(void* ctx);
    float (*getConv)(void* ctx);
    void  (*setDepth)(void* ctx, float v);
    void  (*setConv)(void* ctx, float v);
    float (*getFov)(void* ctx);
    void  (*setFov)(void* ctx, float v);
    void  (*onApplied)(void* ctx) = nullptr;  // optional (e.g. mark dirty)
    void* ctx = nullptr;
};

inline bool HotkeyNearlyEqual(float a, float b, float maxDelta = 0.001f)
{
    return std::fabs(a - b) <= maxDelta;
}

// Evaluate the user_settings[] preset hotkeys. `is_down(vk)` reports keyboard
// key state; `got_xinput`/`xstate` carry the merged gamepad button mask.
// Line-for-line the former per-platform body — keep behavior identical.
template <typename IsDownFn>
inline std::string ApplyUserSettingsHotkeysImpl(
    StereoDisplayDriverConfiguration& cfg, bool got_xinput, uint32_t xstate,
    const DepthConvBackend& b, IsDownFn is_down, float maxDelta = 0.001f)
{
    std::string storeMsg;

    auto applied = [&]() {
        if (b.onApplied) b.onApplied(b.ctx);
    };

    for (size_t i = 0; i < cfg.num_user_settings; ++i) {
        if (cfg.sleep_count[i] > 0)
            cfg.sleep_count[i]--;

        if (cfg.user_depth[i].empty()) continue;  // malformed row, skip

        if (i >= cfg.user_preset_index.size()) cfg.user_preset_index.resize(i + 1, 0);
        if (cfg.user_preset_index[i] >= cfg.user_depth[i].size())
            cfg.user_preset_index[i] = 0;

        const size_t presets = cfg.user_depth[i].size();
        size_t idx = cfg.user_preset_index[i];
        const size_t hold_idx = 0;

        auto getD = [&](size_t k) { return cfg.user_depth[i][k]; };
        auto getC = [&](size_t k) {
            return k < cfg.user_convergence[i].size()
                 ? cfg.user_convergence[i][k]
                 : (cfg.user_convergence[i].empty() ? 1.0f : cfg.user_convergence[i].back());
        };
        auto getF = [&](size_t k) {
            float f = k < cfg.user_fov[i].size()
                    ? cfg.user_fov[i][k]
                    : (cfg.user_fov[i].empty() ? 0.0f : cfg.user_fov[i].back());
            if (f <= 0.0f) f = cfg.fov;  // 0 = "keep active FoV" sentinel
            return f;
        };
        auto applyPreset = [&](size_t k) {
            b.setDepth(b.ctx, getD(k));
            b.setConv(b.ctx,  getC(k));
            b.setFov(b.ctx,   getF(k));
            applied();
        };

        const bool loadPressed =
            (cfg.load_xinput[i] && got_xinput &&
                ((xstate & static_cast<uint32_t>(cfg.user_load_key[i])) ==
                    static_cast<uint32_t>(cfg.user_load_key[i])))
            || (!cfg.load_xinput[i] && is_down(cfg.user_load_key[i]));

        if (loadPressed) {
            const int32_t kt = cfg.user_key_type[i];

            if (kt == HOLD && !cfg.was_held[i]) {
                cfg.prev_depth[i] = b.getDepth(b.ctx);
                cfg.prev_convergence[i] = b.getConv(b.ctx);
                cfg.prev_fov[i] = b.getFov(b.ctx);
                cfg.was_held[i] = true;
                applyPreset(hold_idx);
            } else if (kt == TOGGLE && cfg.sleep_count[i] < 1) {
                cfg.sleep_count[i] = cfg.sleep_count_max;

                const float curD = b.getDepth(b.ctx);
                const float curC = b.getConv(b.ctx);
                const float curF = b.getFov(b.ctx);

                const bool matches =
                    HotkeyNearlyEqual(curD, getD(idx), maxDelta) &&
                    HotkeyNearlyEqual(curC, getC(idx), maxDelta) &&
                    HotkeyNearlyEqual(curF, getF(idx), maxDelta);

                if (matches && presets > 1) {
                    idx = (idx + 1) % presets;
                    cfg.user_preset_index[i] = idx;
                    applyPreset(idx);
                } else if (matches) {
                    b.setDepth(b.ctx, cfg.prev_depth[i]);
                    b.setConv(b.ctx,  cfg.prev_convergence[i]);
                    b.setFov(b.ctx,   cfg.prev_fov[i]);
                    applied();
                } else {
                    cfg.prev_depth[i] = curD;
                    cfg.prev_convergence[i] = curC;
                    cfg.prev_fov[i] = curF;
                    applyPreset(idx);
                }
            } else if (kt == SWITCH && cfg.sleep_count[i] < 1) {
                cfg.sleep_count[i] = cfg.sleep_count_max;
                applyPreset(idx);
                cfg.user_preset_index[i] = (idx + 1) % presets;
            }
        } else if (cfg.user_key_type[i] == HOLD && cfg.was_held[i]) {
            cfg.was_held[i] = false;
            b.setDepth(b.ctx, cfg.prev_depth[i]);
            b.setConv(b.ctx, cfg.prev_convergence[i]);
            b.setFov(b.ctx, cfg.prev_fov[i]);
            applied();
        }
    }

    return storeMsg;
}

}  // namespace vrto3d
