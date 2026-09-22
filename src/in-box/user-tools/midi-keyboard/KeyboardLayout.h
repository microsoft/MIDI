// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    struct KeyGeometry
    {
        // the note this key plays before transposition is applied
        int32_t NoteNumber{ 0 };

        bool IsBlack{ false };

        double Left{ 0.0 };
        double Top{ 0.0 };
        double Width{ 0.0 };
        double Height{ 0.0 };
    };

    // Keys in paint order: every white key first, then the black keys that overlap them.
    // The keyboard starts and ends on a C, the way a real one does, so a four octave
    // keyboard has twenty-nine white keys rather than twenty-eight.
    std::vector<KeyGeometry> BuildKeyboard(
        int32_t firstNoteNumber,
        uint32_t octaveCount,
        double width,
        double height) noexcept;

    // Number of white keys a keyboard of this many octaves draws, including the closing C.
    uint32_t WhiteKeyCount(uint32_t octaveCount) noexcept;

    // Index into the keys vector, or -1. Black keys win where the two overlap.
    int32_t HitTestKey(std::vector<KeyGeometry> const& keys, double x, double y) noexcept;

    bool IsBlackKeyNote(int32_t noteNumber) noexcept;

    // "C#3" style. Middle C, note 60, is C3, matching the rest of the Windows MIDI Services tools.
    std::wstring NoteName(int32_t noteNumber) noexcept;

    // One of the keyboard layouts Windows currently has loaded for this session.
    struct InstalledKeyboardLayout
    {
        // the HKL as a 32 bit value, which is what the setting stores
        uint32_t Identifier{ 0 };

        // "German (Germany)", in the display language
        std::wstring LanguageName{};

        // the letters the layout puts on the first six keys of the top row, for example
        // "QWERTY" or "QWERTZ", which is how a player recognizes their own keyboard
        std::wstring KeySignature{};
    };

    std::vector<InstalledKeyboardLayout> InstalledKeyboardLayouts() noexcept;

    // The layout the keyboard is set to right now. Zero means "whatever Windows is using",
    // which is also what the setting stores for automatic.
    uint32_t ActiveKeyboardLayoutIdentifier() noexcept;

    bool IsKeyboardLayoutInstalled(uint32_t layoutIdentifier) noexcept;

    // The character the layout prints on the key that plays this many semitones above the
    // lowest displayed C, for example "Z" on a US keyboard and "Y" on a German one. Empty
    // when the note is out of reach of the computer keyboard mapping.
    std::wstring ComputerKeyLabel(int32_t semitonesFromBottom, uint32_t layoutIdentifier) noexcept;

    // Semitones above the lowest displayed C, or -1 when the key plays nothing. Takes the
    // scan code rather than the virtual key, so the notes land on the same physical keys on
    // every keyboard layout.
    int32_t ComputerKeyToSemitones(uint32_t scanCode) noexcept;

    // MIDI 2.0 min-center-max scaling, so 0 stays 0, the center stays centered and the
    // maximum stays the maximum. Anything less makes a MIDI 1.0 value drift once a
    // receiver scales it back down.
    uint32_t ScaleUpValue(uint32_t value, uint32_t sourceBits, uint32_t destinationBits) noexcept;
}
