#pragma once
#include "Animation.h"

#include "../src/application/vibes/gifs/kakashi.h"
#include "../src/application/vibes/gifs/shoyo.h"
#include "../src/application/vibes/gifs/beFree.h"
#include "../src/application/vibes/gifs/bloom.h"
#include "../src/application/vibes/gifs/nocturne.h"
#include "../src/application/vibes/gifs/monkey_walk.h"
#include "../src/application/vibes/gifs/specBoy.h"
#include "../src/application/vibes/gifs/kuro.h"
#include "../src/application/vibes/gifs/fuckYou.h"


// ============================================================
//  VibeRegistry  —  Master list of all Vibe animations
//
//  TO ADD A NEW VIBE:
//    1. Drop your .h file into src/vibes/gifs/
//    2. #include it below
//    3. Add ONE line to ALL_VIBES[]
//    Everything else (picker menu, NVS index, preview) updates automatically.
//
//  IMPORTANT: Keep ALL_VIBES[] order stable across releases.
//  NVS stores the selected index — if you reorder, saved prefs break.
//  Always ADD to the end. Never reorder or remove.
// ============================================================

struct VibeEntry {
    const char*         name;
    const Animation*    animation;
};

// ── Master list ──────────────────────────────────────────────
// ── NEVER reorder. Always append.─────────────────────────────
static const VibeEntry ALL_VIBES[] = {
    {"Spec_Boy", &specBoy},
    {"Kuro", &kuro},
    {"Monkey_Walk", &monkey_walk},
    {"Be_Free", &be_free},
    {"Bloom", &bloom},
    {"Nocturne", &nocturne},
    {"Fuck_You", &fuck_you},
    {"Kakashi", &kakashi},
    {"Shoyo", &shoyo}
};

static constexpr int VIBE_COUNT = sizeof(ALL_VIBES) / sizeof(ALL_VIBES[0]);
