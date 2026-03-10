/*
 * ue3d_protocol.h - UE3D Shared Memory Protocol Definition
 *
 * VERSION: 4.1
 *
 * SINGLE SOURCE OF TRUTH for 3-party shared memory (UEVR + VRto3D + 3DGameBridge).
 * All projects should include this EXACT file to prevent struct misalignment.
 *
 * UEVR includes local copy:      #include "ue3d_protocol.h"
 * VRto3D includes canonical:     #include "vrto3dlib/ue3d_protocol.h"
 * 3DGameBridge includes local:   #include "ue3d_protocol.h"
 *
 * LICENSE: This file is dual-licensed to be compatible with both:
 *   - UEVR (MIT License)
 *   - VRto3D (LGPL v3)
 *
 * MONITOR MODE OWNERSHIP MODEL:
 *   UEVR owns all stereo rendering (eye separation, convergence, projection).
 *   VRto3D is a passive SBS display compositor.
 *   Shared memory carries: state for VRto3D display, commands for depth
 *   calibration, Leia eye tracking data, and diagnostic readback.
 *
 * CHANGELOG:
 *   v4.1 - Monitor mode cleanup: removed VR-only fields
 *        - Removed: game_fov, base_fov, convergence_multiplier,
 *          uevr_frametime, is_zooming, stereo_aim_correction, stereo_aim_base
 *        - Removed: UE3D_FLAG_AIM_CORRECTION
 *        - These fields replaced with _reserved padding (byte layout preserved)
 *        - Wire-compatible with v4.0: reserved bytes are zero-filled
 *   v4.0 - Leia integration (eye tracking, display info, UE3D_FLAG_LEIA_EYES)
 *   v3.3 - Added stereo_depth_hint field
 *   v3.2 - Added monitor_mode + is_monitor_display fields
 *   v3.0 - Multiplier mode (depth_multiplier instead of suggested_depth)
 */

#ifndef UE3D_PROTOCOL_H
#define UE3D_PROTOCOL_H

#include <cstdint>

/* ========================================================================== */
/* PROTOCOL CONSTANTS                                                         */
/* ========================================================================== */

#define UE3D_MAGIC          0x55455652   /* "UEVR" in ASCII (unchanged)        */
#define UE3D_VERSION        4            /* Protocol version (wire-compat 4.x) */
#define UE3D_STRUCT_SIZE    256          /* Total struct size in bytes          */
#define UE3D_SHMEM_NAME    "UE3D_SharedData"

/* Staleness threshold: data older than this (ms) is considered disconnected  */
#define UE3D_STALE_MS       1000

/* Feature flags (bitfield in SharedData.flags)                               */
#define UE3D_FLAG_MULTIPLIER_MODE  0x01   /* Uses multipliers, not absolutes  */
#define UE3D_FLAG_SCENE_AWARE      0x02   /* Sends scene_type field           */
#define UE3D_FLAG_FOV_COMP         0x04   /* Supports FOV compensation        */
#define UE3D_FLAG_LEIA_EYES        0x20   /* v4.0: Leia eye tracking + display */

/* ========================================================================== */
/* ENUMS                                                                       */
/* ========================================================================== */

/* Scene type - tells VRto3D what context UEVR is in */
enum UE3D_SceneType : uint8_t {
    UE3D_SCENE_NORMAL   = 0,
    UE3D_SCENE_CUTSCENE = 1,
    UE3D_SCENE_MENU     = 2,
    UE3D_SCENE_VEHICLE  = 3,
    UE3D_SCENE_LOADING  = 4
};

/* Zoom mode - tells VRto3D what kind of zoom UEVR detected */
enum UE3D_ZoomMode : uint8_t {
    UE3D_ZOOM_NONE           = 0,
    UE3D_ZOOM_AIM_DOWN_SIGHT = 1,
    UE3D_ZOOM_SCOPE          = 2
};

/* ========================================================================== */
/* SHARED MEMORY STRUCTURE - 256 BYTES, PACKED                                 */
/*                                                                             */
/* ALL PARTIES MUST USE THIS EXACT LAYOUT.                                     */
/*                                                                             */
/* Offset map:                                                                 */
/*   0-15    HEADER             (16 bytes)                                     */
/*   16-39   FOV / ZOOM         (24 bytes)  UEVR -> VRto3D                    */
/*   40-55   DEPTH CONTROL      (16 bytes)  UEVR -> VRto3D                    */
/*   56-71   TIMING             (16 bytes)  UEVR -> VRto3D                    */
/*   72-103  VRTO3D STATE       (32 bytes)  VRto3D -> UEVR  (diagnostic)      */
/*   104-119 VRTO3D STATUS      (16 bytes)  VRto3D -> UEVR                    */
/*   120-183 PROFILE INFO       (64 bytes)  UEVR -> VRto3D                    */
/*   184-215 LEIA EYE TRACKING  (32 bytes)  3DGameBridge -> UEVR  [v4.0]     */
/*   216-223 LEIA DISPLAY INFO  (8 bytes)   3DGameBridge -> UEVR  [v4.0]     */
/*   224-231 LEIA RESERVED      (8 bytes)   Future Leia fields                */
/*   232-235 COMMANDS           (4 bytes)   UEVR -> VRto3D                    */
/*   236-243 RESERVED           (8 bytes)   (was aim correction, v4.1)        */
/*   244-245 MONITOR MODE       (2 bytes)   Bidirectional                     */
/*   246-249 STEREO DEPTH HINT  (4 bytes)   UEVR -> VRto3D                    */
/*   250-255 RESERVED           (6 bytes)   Future use                         */
/* ========================================================================== */

#pragma pack(push, 1)
typedef struct UE3D_SharedData {

    /* ----- HEADER (16 bytes) -------------------------------------------- */
    uint32_t magic;                  /* Must be UE3D_MAGIC                   */
    uint32_t version;                /* UE3D_VERSION                         */
    uint32_t struct_size;            /* sizeof(UE3D_SharedData) = 256        */
    uint32_t flags;                  /* Bitfield: UE3D_FLAG_*                */

    /* ----- UEVR -> VRTO3D: FOV / ZOOM (24 bytes) ----------------------- */
    uint8_t  _reserved_fov1[4];      /* (was game_fov, UEVR-internal now)   */
    uint8_t  _reserved_fov2[4];      /* (was base_fov, UEVR-internal now)   */
    float    fov_scale;              /* Projection scale (0.5 = 2x zoom)    */
    float    zoom_factor;            /* Magnification (2.0 = 2x zoom)       */
    uint8_t  _reserved_zoom1;        /* (was is_zooming, derive fov_scale)  */
    uint8_t  is_valid;               /* 1 if FOV reading is trustworthy      */
    uint8_t  zoom_mode;              /* UE3D_ZoomMode enum                   */
    uint8_t  _pad1[5];              /* Align to 24 bytes for this section   */

    /* ----- UEVR -> VRTO3D: DEPTH CONTROL (16 bytes) -------------------- */
    float    depth_multiplier;       /* 0.05 - 1.0 (1.0 = no change)        */
    uint8_t  _reserved_depth1[4];    /* (was convergence_multiplier)         */
    uint8_t  scene_type;             /* UE3D_SceneType enum                  */
    uint8_t  auto_depth_request;     /* 1=auto, 2=calibrate, 3-9=depth cmds */
    uint8_t  _pad2[2];              /* Padding                              */
    float    world_scale;            /* UEVR world scale                     */

    /* ----- UEVR -> VRTO3D: TIMING (16 bytes) --------------------------- */
    uint64_t uevr_timestamp;         /* GetTickCount64() from UEVR           */
    uint32_t uevr_frame_count;       /* UEVR frame counter                   */
    uint8_t  _reserved_timing1[4];   /* (was uevr_frametime)                 */

    /* ----- VRTO3D -> UEVR: CURRENT STATE (32 bytes) -------------------- */
    /* Diagnostic: UEVR reads for UI display, no logic depends on these     */
    float    vrto3d_depth;           /* VRto3D's current depth               */
    float    vrto3d_convergence;     /* VRto3D's current convergence         */
    float    vrto3d_fov;             /* VRto3D's current FOV                 */
    float    vrto3d_fov_adjustment;  /* FOV delta from convergence           */
    float    vrto3d_aspect_ratio;    /* Display aspect ratio                 */
    float    vrto3d_ipd;             /* IPD setting                          */
    float    vrto3d_hmd_height;      /* HMD height                           */
    uint8_t  vrto3d_sbs_mode;       /* 0 = TaB, 1 = SbS                    */
    uint8_t  _pad3[3];              /* Padding                              */

    /* ----- VRTO3D -> UEVR: STATUS (16 bytes) --------------------------- */
    uint8_t  vrto3d_connected;       /* 1 if VRto3D is reading this memory   */
    uint8_t  vrto3d_auto_depth_active; /* 1 if applying depth multiplier     */
    uint8_t  vrto3d_profile_loaded;  /* 1 if VRto3D has a game profile       */
    uint8_t  vrto3d_listener_enabled;/* 1 if Ctrl+F11 auto-depth is ON       */
    uint8_t  _pad4[4];              /* Padding                              */
    uint64_t vrto3d_timestamp;       /* GetTickCount64() from VRto3D         */

    /* ----- PROFILE INFO (64 bytes) -------------------------------------- */
    char     uevr_profile_name[32];  /* Current UEVR profile name            */
    char     game_exe_name[32];      /* Game executable name                 */

    /* ----- v4.0: LEIA EYE TRACKING (32 bytes) 3DGameBridge -> UEVR ------ */
    uint8_t  leia_tracking_active;   /* 1 if face tracked, 0 if not         */
    uint8_t  _leia_pad1[3];          /* Alignment padding                   */
    float    leia_left_eye_x;        /* Left eye X (mm, right)              */
    float    leia_left_eye_y;        /* Left eye Y (mm, up)                 */
    float    leia_left_eye_z;        /* Left eye Z (mm, backward)           */
    float    leia_right_eye_x;       /* Right eye X (mm, right)             */
    float    leia_right_eye_y;       /* Right eye Y (mm, up)                */
    float    leia_right_eye_z;       /* Right eye Z (mm, backward)          */
    uint32_t leia_frame_counter;     /* Writer increments each update       */

    /* ----- v4.0: LEIA DISPLAY INFO (8 bytes) 3DGameBridge -> UEVR ------- */
    float    leia_display_width_cm;  /* Physical display width (cm)         */
    float    leia_display_height_cm; /* Physical display height (cm)        */

    /* ----- v4.0: RESERVED FOR LEIA (8 bytes) ---------------------------- */
    uint8_t  _leia_reserved[8];      /* Zero-filled, future Leia fields     */

    /* ----- COMMANDS (4 bytes) UEVR -> VRto3D ---------------------------- */
    uint32_t command_seq;            /* Sequence number for depth commands    */

    /* ----- RESERVED (8 bytes, was aim correction in v3.2) --------------- */
    uint8_t  _reserved_aim[8];       /* Zero-filled                         */

    /* ----- MONITOR MODE (2 bytes) Bidirectional ------------------------- */
    uint8_t  monitor_mode;           /* 1 if UEVR monitor mode is active     */
    uint8_t  is_monitor_display;     /* 1 if VRto3D is outputting to monitor  */

    /* ----- STEREO DEPTH HINT (4 bytes) UEVR -> VRto3D ------------------ */
    float    stereo_depth_hint;      /* UEVR stereo depth for overlay IPD    */

    /* ----- RESERVED (6 bytes) ------------------------------------------- */
    uint8_t  reserved[6];            /* Zero-filled, for future fields       */

} UE3D_SharedData;
#pragma pack(pop)

/* Compile-time size check - if this fires, the struct layout is wrong */
#ifndef __cplusplus
_Static_assert(sizeof(UE3D_SharedData) == 256,
    "UE3D_SharedData must be exactly 256 bytes");
#else
static_assert(sizeof(UE3D_SharedData) == 256,
    "UE3D_SharedData must be exactly 256 bytes");
#endif

/* ========================================================================== */
/* HELPERS                                                                     */
/* ========================================================================== */

/* Check if UEVR data is fresh */
static inline int ue3d_is_uevr_fresh(const UE3D_SharedData* d, uint64_t now_ms) {
    if (!d || d->magic != UE3D_MAGIC) return 0;
    if (!d->is_valid) return 0;
    return (now_ms - d->uevr_timestamp) < UE3D_STALE_MS;
}

/* Check if VRto3D data is fresh */
static inline int ue3d_is_vrto3d_fresh(const UE3D_SharedData* d, uint64_t now_ms) {
    if (!d || d->magic != UE3D_MAGIC) return 0;
    if (!d->vrto3d_connected) return 0;
    return (now_ms - d->vrto3d_timestamp) < UE3D_STALE_MS;
}

/* Check if Leia eye tracking data is present (v4.0) */
static inline int ue3d_has_leia_eyes(const UE3D_SharedData* d) {
    if (!d) return 0;
    return (d->flags & UE3D_FLAG_LEIA_EYES) && d->leia_tracking_active;
}

#endif /* UE3D_PROTOCOL_H */
