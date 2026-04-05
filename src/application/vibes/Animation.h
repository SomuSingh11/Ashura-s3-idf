#pragma once

// ============================================================
//  Animation  —  A named frame sequence
//
//  Each frame is a 128×64 1-bit XBM bitmap = 1024 bytes.
//  All frame arrays should be declared const so the linker
//  places them in flash automatically on ESP32.
//
//  Example (in your gif header):
//    const unsigned char my_frame_001[] = { ... };
//    const unsigned char* const my_frames[] = { my_frame_001, ... };
//    const Animation my_anim = { my_frames, 27, 80, "My Anim" };
// ============================================================

struct Animation {
    const unsigned char* const* frames;
    int                         frameCount;
    int                         frameDelayMs;
    const char*                 name;
};