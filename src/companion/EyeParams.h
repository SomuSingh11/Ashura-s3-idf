#pragma once

// ================================================================
//  EyeParams  —  everything needed to render one pair of eyes
//
//  All values normalised 0.0–1.0 except squish (1.0 = normal).
//
//  lidTop    0.0 = fully open   →   1.0  = closed from top   (sleepy)
//  lidBottom 0.0 = fully open   →   1.0  = closed from below (squint)
//  pupilSize 0.0 = tiny dot     →   1.0  = fills eye
//  pupilX   -1.0 = hard left    →  +1.0  = hard right
//  pupilY   -1.0 = hard up      →  +1.0  = hard down
//  squishX   1.0 = normal       →  >1.0  = wider (surprise)
//  squishY   1.0 = normal       →  <1.0  = flatter (squint shape)
//
//  MoodEngine holds a TARGET.
//  CompanionRenderer holds CURRENT and lerps toward TARGET.
//  This makes every mood transition smooth automatically.
// ================================================================

struct EyeParams {
    float lidTop    = 0.05f;
    float lidBottom = 0.00f;
    float pupilSize = 0.45f;
    float pupilX    = 0.00f;
    float pupilY    = 0.00f;
    float squishX   = 1.00f;
    float squishY   = 1.00f;
};

namespace Mood {

    // Reseting - alive but neutral
    inline EyeParams IDLE() {
        EyeParams p;
        p.lidTop    = 0.05f;
        p.lidBottom = 0.00f;
        p.pupilSize = 0.45f;
        p.pupilX    = 0.00f;
        p.pupilY    = 0.00f;
        p.squishX   = 1.00f;
        p.squishY   = 1.00f;
      return p;
    }

    // Content — slight smile squint, pupils down a touch
    inline EyeParams HAPPY() {
        EyeParams p;
        p.lidTop    = 0.10f;
        p.lidBottom = 0.18f;
        p.pupilSize = 0.50f;
        p.pupilX    = 0.00f;
        p.pupilY    = 0.08f;
        p.squishX   = 1.05f;
        p.squishY   = 0.88f;
        return p;
    }

    // Bored — heavy top lid, gaze drifted right
    inline EyeParams BORED() {
        EyeParams p;
        p.lidTop    = 0.48f;
        p.lidBottom = 0.00f;
        p.pupilSize = 0.38f;
        p.pupilX    = 0.20f;
        p.pupilY    = 0.22f;
        p.squishX   = 1.00f;
        p.squishY   = 1.00f;
        return p;
    }

    // Sleepy — very heavy lids, pupils low
    inline EyeParams SLEEPY() {
        EyeParams p;
        p.lidTop    = 0.68f;
        p.lidBottom = 0.00f;
        p.pupilSize = 0.30f;
        p.pupilX    = 0.00f;
        p.pupilY    = 0.28f;
        p.squishX   = 1.00f;
        p.squishY   = 1.00f;
        return p;
    }

    // Focused — determined, slight bottom squint
    inline EyeParams FOCUSED() {
        EyeParams p;
        p.lidTop    = 0.05f;
        p.lidBottom = 0.22f;
        p.pupilSize = 0.55f;
        p.pupilX    = 0.00f;
        p.pupilY    = -0.05f;
        p.squishX   = 1.00f;
        p.squishY   = 0.82f;
        return p;
    }

    // Surprised — wide open, pupils up, bigger
    inline EyeParams SURPRISED() {
        EyeParams p;
        p.lidTop    = 0.00f;
        p.lidBottom = 0.00f;
        p.pupilSize = 0.65f;
        p.pupilX    = 0.00f;
        p.pupilY    = -0.12f;
        p.squishX   = 1.22f;
        p.squishY   = 1.22f;
        return p;
    }

    // Annoyed — both lids partially closed, furrowed
    inline EyeParams ANNOYED() {
        EyeParams p;
        p.lidTop    = 0.28f;
        p.lidBottom = 0.28f;
        p.pupilSize = 0.40f;
        p.pupilX    = 0.00f;
        p.pupilY    = -0.15f;
        p.squishX   = 0.95f;
        p.squishY   = 0.82f;
        return p;
    }

    // Excited (Spotify) — wide pupils, slight upward gaze
    inline EyeParams EXCITED() {
        EyeParams p;

        p.lidTop    = 0.00f;
        p.lidBottom = 0.00f;
        p.pupilSize = 0.62f;
        p.pupilX    = 0.00f;
        p.pupilY    = -0.08f;
        p.squishX   = 1.12f;
        p.squishY   = 1.12f;

        return p;
    }
}; 
