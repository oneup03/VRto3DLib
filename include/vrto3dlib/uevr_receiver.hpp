/*
 * uevr_receiver.hpp - UE3D Bridge Receiver for VRto3D (Monitor Mode)
 *
 * CANONICAL HOME: VRto3DLib (include/vrto3dlib/uevr_receiver.hpp)
 * Include as: #include "vrto3dlib/uevr_receiver.hpp"
 *
 * Reads UE3D shared memory written by UEVR. In monitor mode, VRto3D is a
 * passive SBS compositor — this receiver handles:
 *   - Connection lifecycle and staleness detection
 *   - Monitor mode flag propagation
 *   - Stereo depth hint for overlay IPD matching
 *   - Heartbeat (writes vrto3d_connected + timestamp back to UEVR)
 *
 * LICENSE: Dual-licensed (MIT for UEVR compatibility, LGPL v3 for VRto3D).
 */

#pragma once

#include <Windows.h>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include "vrto3dlib/ue3d_protocol.h"

namespace uevr {

constexpr uint32_t UEVR_MAGIC = UE3D_MAGIC;
constexpr const char* SHARED_MEM_NAME = UE3D_SHMEM_NAME;

using SharedData = UE3D_SharedData;
static_assert(sizeof(SharedData) == 256, "SharedData must be 256 bytes!");

class Receiver {
public:
    static Receiver& instance() {
        static Receiver inst;
        return inst;
    }

    // ----- Lifecycle -----

    bool init() {
        if (m_data) return true;

        m_mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
        if (!m_mapping) return false;

        m_data = static_cast<SharedData*>(MapViewOfFile(
            m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));

        if (!m_data) {
            cleanup();
            return false;
        }
        if (m_data->magic != UEVR_MAGIC) {
            m_last_magic_mismatch = m_data->magic;
            cleanup();
            return false;
        }

        return true;
    }

    void shutdown() {
        if (m_data) {
            m_data->vrto3d_connected = 0;
        }
        cleanup();
    }

    bool is_connected() const { return m_data != nullptr; }

    uint32_t get_last_magic_mismatch() const { return m_last_magic_mismatch; }

    // ----- Raw field accessors (for debug logging) -----
    uint32_t raw_magic() const { return m_data ? m_data->magic : 0; }
    uint32_t raw_version() const { return m_data ? m_data->version : 0; }
    uint8_t  raw_is_valid() const { return m_data ? m_data->is_valid : 0; }

    // ----- Heartbeat Update (call every frame) -----

    /**
     * Write VRto3D state back to UEVR and check connection.
     * In monitor mode, this is the primary per-frame call.
     */
    void update(float depth, float convergence, float fov, float fov_adj,
                uint8_t sbs_mode, bool profile_loaded = false) {
        if (!m_data && !init()) return;

        if (m_data->magic != UEVR_MAGIC) {
            cleanup();
            return;
        }

        m_data->vrto3d_depth = depth;
        m_data->vrto3d_convergence = convergence;
        m_data->vrto3d_fov = fov;
        m_data->vrto3d_fov_adjustment = fov_adj;
        m_data->vrto3d_sbs_mode = sbs_mode;
        m_data->vrto3d_connected = 1;
        m_data->vrto3d_profile_loaded = profile_loaded ? 1 : 0;
        m_data->is_monitor_display = 1;
        m_data->vrto3d_timestamp = GetTickCount64();
    }

    // ----- Data Validity -----

    bool has_valid_data() const {
        if (!m_data) return false;
        if (!m_data->is_valid) return false;
        uint64_t now = GetTickCount64();
        return (now - m_data->uevr_timestamp) < UE3D_STALE_MS;
    }

    // ----- Monitor Mode -----

    bool get_monitor_mode() const {
        return has_valid_data() && m_data->monitor_mode != 0;
    }

    // ----- Stereo Depth Hint (for overlay IPD matching) -----

    float get_stereo_depth_hint() const {
        if (!has_valid_data()) return 0.0f;
        float hint = m_data->stereo_depth_hint;
        return (std::isfinite(hint) && hint > 0.0f && hint < 2.0f) ? hint : 0.0f;
    }

private:
    Receiver() = default;
    ~Receiver() { shutdown(); }
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    void cleanup() {
        if (m_data) {
            UnmapViewOfFile(m_data);
            m_data = nullptr;
        }
        if (m_mapping) {
            CloseHandle(m_mapping);
            m_mapping = nullptr;
        }
    }

    HANDLE m_mapping = nullptr;
    SharedData* m_data = nullptr;
    uint32_t m_last_magic_mismatch = 0;
};

inline Receiver& receiver() {
    return Receiver::instance();
}

} // namespace uevr
