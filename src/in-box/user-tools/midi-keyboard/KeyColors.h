// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    // One kind of key: a gradient from top to bottom, and the color of the letters and note
    // names drawn on it. All 0xAARRGGBB, and always opaque.
    struct KeyPalette
    {
        uint32_t TopArgb{ 0 };
        uint32_t BottomArgb{ 0 };
        uint32_t TextArgb{ 0 };
    };

    // Shades a key color the way the built-in keys are shaded, and picks text that stays
    // readable on every part of the key, whatever the color. Alpha is ignored.
    KeyPalette MakeKeyPalette(_In_ uint32_t keyColorArgb) noexcept;
}
