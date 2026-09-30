// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    // One kind of key: a gradient from top to bottom, the color of the letters and note names
    // drawn on it, and the lines around it. All 0xAARRGGBB.
    struct KeyPalette
    {
        uint32_t TopArgb{ 0 };
        uint32_t BottomArgb{ 0 };
        uint32_t TextArgb{ 0 };
        uint32_t LineArgb{ 0 };
    };

    // Shades a key color like the built-in keys, with text and lines that show on every part of
    // the key whatever the color. Alpha is ignored; the result is opaque.
    KeyPalette MakeKeyPalette(_In_ uint32_t keyColorArgb) noexcept;

    // The lines around a key with this gradient and text: the key color shifted the way the text
    // goes, just far enough to show on every part of the key. Opaque.
    uint32_t MakeKeyLineArgb(_In_ uint32_t keyColorArgb, _In_ KeyPalette const& palette) noexcept;
}
