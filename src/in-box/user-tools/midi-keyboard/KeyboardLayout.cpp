// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "KeyboardLayout.h"

namespace midikeyboard
{
    namespace
    {
        // semitone offset of each white key within an octave
        constexpr int32_t WhiteKeySemitones[7]{ 0, 2, 4, 5, 7, 9, 11 };

        // a black key sits on the boundary above these white keys: C, D, F, G, A
        constexpr int32_t BlackKeyAfterWhite[5]{ 0, 1, 3, 4, 5 };

        constexpr wchar_t const* NoteLetters[12]
        {
            L"C", L"C#", L"D", L"D#", L"E", L"F",
            L"F#", L"G", L"G#", L"A", L"A#", L"B"
        };

        constexpr double BlackKeyWidthFactor = 0.62;
        constexpr double BlackKeyHeightFactor = 0.62;

        struct ComputerKeyMapping
        {
            // set 1 make code, which is what PhysicalKeyStatus reports and what identifies a
            // key by where it sits rather than by what it types
            uint32_t ScanCode;

            int32_t Semitones;
        };

        // The layout trackers and DAWs have used for decades: the bottom two rows play one
        // octave from the lowest displayed C, the top two rows play the octave above it.
        // Positional, so it falls on the same keys on a QWERTZ or AZERTY keyboard as it does
        // on QWERTY; the letters shown on the keys are read from the layout in use. The
        // comments name each key by where a US keyboard puts it.
        constexpr ComputerKeyMapping ComputerKeys[]
        {
            { 0x2C,  0 },   // Z
            { 0x1F,  1 },   // S
            { 0x2D,  2 },   // X
            { 0x20,  3 },   // D
            { 0x2E,  4 },   // C
            { 0x2F,  5 },   // V
            { 0x22,  6 },   // G
            { 0x30,  7 },   // B
            { 0x23,  8 },   // H
            { 0x31,  9 },   // N
            { 0x24, 10 },   // J
            { 0x32, 11 },   // M
            { 0x33, 12 },   // ,
            { 0x26, 13 },   // L
            { 0x34, 14 },   // .
            { 0x27, 15 },   // ;
            { 0x35, 16 },   // /

            { 0x10, 12 },   // Q
            { 0x03, 13 },   // 2
            { 0x11, 14 },   // W
            { 0x04, 15 },   // 3
            { 0x12, 16 },   // E
            { 0x13, 17 },   // R
            { 0x06, 18 },   // 5
            { 0x14, 19 },   // T
            { 0x07, 20 },   // 6
            { 0x15, 21 },   // Y
            { 0x08, 22 },   // 7
            { 0x16, 23 },   // U
            { 0x17, 24 },   // I
            { 0x0A, 25 },   // 9
            { 0x18, 26 },   // O
            { 0x0B, 27 },   // 0
            { 0x19, 28 },   // P
        };

        // the first six keys of the top letter row, which is what gives a layout its name
        constexpr uint32_t SignatureScanCodes[]{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 };

        HKL ToKeyboardLayout(uint32_t layoutIdentifier) noexcept
        {
            return layoutIdentifier == 0
                ? GetKeyboardLayout(0)
                : reinterpret_cast<HKL>(static_cast<ULONG_PTR>(layoutIdentifier));
        }

        uint32_t FromKeyboardLayout(HKL layout) noexcept
        {
            return static_cast<uint32_t>(reinterpret_cast<ULONG_PTR>(layout));
        }

        std::wstring LabelForScanCode(uint32_t scanCode, HKL layout) noexcept
        {
            try
            {
                auto const virtualKey = MapVirtualKeyExW(scanCode, MAPVK_VSC_TO_VK_EX, layout);

                if (virtualKey == 0)
                {
                    return {};
                }

                // the top bit marks a dead key, but the character underneath is still what is
                // printed on the key cap
                auto const mapped = MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_CHAR, layout) & 0x7FFFFFFF;

                auto character = static_cast<wchar_t>(mapped);

                if (character == 0 || character == L' ')
                {
                    // a layout with no character here still has a usable name for the key
                    if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9'))
                    {
                        character = static_cast<wchar_t>(virtualKey);
                    }
                    else
                    {
                        return {};
                    }
                }

                std::wstring label(1, character);

                CharUpperBuffW(label.data(), 1);

                return label;
            }
            catch (...)
            {
                return {};
            }
        }

        std::wstring LanguageNameForLayout(HKL layout) noexcept
        {
            try
            {
                wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};

                auto const languageId = LOWORD(FromKeyboardLayout(layout));

                if (LCIDToLocaleName(
                    MAKELCID(languageId, SORT_DEFAULT),
                    localeName,
                    ARRAYSIZE(localeName),
                    0) == 0)
                {
                    return {};
                }

                wchar_t displayName[LOCALE_NAME_MAX_LENGTH * 2]{};

                if (GetLocaleInfoEx(
                    localeName,
                    LOCALE_SLOCALIZEDDISPLAYNAME,
                    displayName,
                    ARRAYSIZE(displayName)) == 0)
                {
                    return {};
                }

                return displayName;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    _Use_decl_annotations_
    uint32_t WhiteKeyCount(uint32_t octaveCount) noexcept
    {
        return (octaveCount * 7) + 1;
    }

    _Use_decl_annotations_
    bool IsBlackKeyNote(int32_t noteNumber) noexcept
    {
        auto const semitone = ((noteNumber % 12) + 12) % 12;

        return semitone == 1 || semitone == 3 || semitone == 6 || semitone == 8 || semitone == 10;
    }

    _Use_decl_annotations_
    std::vector<KeyGeometry> BuildKeyboard(
        int32_t firstNoteNumber,
        uint32_t octaveCount,
        double width,
        double height) noexcept
    {
        std::vector<KeyGeometry> keys{};

        if (width <= 0.0 || height <= 0.0 || octaveCount == 0)
        {
            return keys;
        }

        auto const whiteCount = WhiteKeyCount(octaveCount);
        auto const whiteWidth = width / static_cast<double>(whiteCount);
        auto const blackWidth = whiteWidth * BlackKeyWidthFactor;
        auto const blackHeight = height * BlackKeyHeightFactor;

        keys.reserve(static_cast<size_t>(whiteCount) + (octaveCount * 5));

        for (uint32_t i = 0; i < whiteCount; i++)
        {
            auto const octave = static_cast<int32_t>(i / 7);
            auto const within = static_cast<int32_t>(i % 7);

            KeyGeometry key{};
            key.NoteNumber = firstNoteNumber + (octave * 12) + WhiteKeySemitones[within];
            key.IsBlack = false;
            key.Left = static_cast<double>(i) * whiteWidth;
            key.Top = 0.0;
            key.Width = whiteWidth;
            key.Height = height;

            keys.push_back(key);
        }

        for (uint32_t octave = 0; octave < octaveCount; octave++)
        {
            for (auto const after : BlackKeyAfterWhite)
            {
                auto const whiteIndex = static_cast<double>((octave * 7) + after);

                KeyGeometry key{};
                key.NoteNumber = firstNoteNumber + static_cast<int32_t>(octave * 12) +
                    WhiteKeySemitones[after] + 1;
                key.IsBlack = true;
                key.Left = ((whiteIndex + 1.0) * whiteWidth) - (blackWidth / 2.0);
                key.Top = 0.0;
                key.Width = blackWidth;
                key.Height = blackHeight;

                keys.push_back(key);
            }
        }

        return keys;
    }

    _Use_decl_annotations_
    int32_t HitTestKey(std::vector<KeyGeometry> const& keys, double x, double y) noexcept
    {
        int32_t whiteHit{ -1 };

        for (size_t i = 0; i < keys.size(); i++)
        {
            auto const& key = keys[i];

            if (x < key.Left || x >= key.Left + key.Width)
            {
                continue;
            }

            if (y < key.Top || y >= key.Top + key.Height)
            {
                continue;
            }

            if (key.IsBlack)
            {
                return static_cast<int32_t>(i);
            }

            if (whiteHit < 0)
            {
                whiteHit = static_cast<int32_t>(i);
            }
        }

        return whiteHit;
    }

    _Use_decl_annotations_
    std::wstring NoteName(int32_t noteNumber) noexcept
    {
        try
        {
            auto const semitone = ((noteNumber % 12) + 12) % 12;
            auto const octave = static_cast<int32_t>(std::floor(noteNumber / 12.0)) - 2;

            return std::format(L"{}{}", NoteLetters[semitone], octave);
        }
        catch (...)
        {
            return {};
        }
    }

    std::vector<InstalledKeyboardLayout> InstalledKeyboardLayouts() noexcept
    {
        std::vector<InstalledKeyboardLayout> layouts{};

        try
        {
            auto const count = GetKeyboardLayoutList(0, nullptr);

            if (count <= 0)
            {
                return layouts;
            }

            std::vector<HKL> handles(static_cast<size_t>(count), nullptr);

            auto const returned = GetKeyboardLayoutList(count, handles.data());

            if (returned <= 0)
            {
                return layouts;
            }

            handles.resize(static_cast<size_t>(std::min(returned, count)));

            layouts.reserve(handles.size());

            for (auto const handle : handles)
            {
                InstalledKeyboardLayout entry{};

                entry.Identifier = FromKeyboardLayout(handle);
                entry.LanguageName = LanguageNameForLayout(handle);

                for (auto const scanCode : SignatureScanCodes)
                {
                    entry.KeySignature += LabelForScanCode(scanCode, handle);
                }

                layouts.push_back(std::move(entry));
            }
        }
        catch (...)
        {
            layouts.clear();
        }

        return layouts;
    }

    uint32_t ActiveKeyboardLayoutIdentifier() noexcept
    {
        return FromKeyboardLayout(GetKeyboardLayout(0));
    }

    _Use_decl_annotations_
    bool IsKeyboardLayoutInstalled(uint32_t layoutIdentifier) noexcept
    {
        if (layoutIdentifier == 0)
        {
            return false;
        }

        for (auto const& layout : InstalledKeyboardLayouts())
        {
            if (layout.Identifier == layoutIdentifier)
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    std::wstring ComputerKeyLabel(int32_t semitonesFromBottom, uint32_t layoutIdentifier) noexcept
    {
        for (auto const& mapping : ComputerKeys)
        {
            if (mapping.Semitones == semitonesFromBottom)
            {
                return LabelForScanCode(mapping.ScanCode, ToKeyboardLayout(layoutIdentifier));
            }
        }

        return {};
    }

    _Use_decl_annotations_
    int32_t ComputerKeyToSemitones(uint32_t scanCode) noexcept
    {
        for (auto const& mapping : ComputerKeys)
        {
            if (mapping.ScanCode == scanCode)
            {
                return mapping.Semitones;
            }
        }

        return -1;
    }

    _Use_decl_annotations_
    uint32_t ScaleUpValue(uint32_t value, uint32_t sourceBits, uint32_t destinationBits) noexcept
    {
        if (sourceBits == 0 || sourceBits >= destinationBits || destinationBits > 32)
        {
            return value;
        }

        auto const scaleBits = destinationBits - sourceBits;
        auto scaled = value << scaleBits;

        auto const sourceCenter = 1u << (sourceBits - 1);

        if (value <= sourceCenter)
        {
            return scaled;
        }

        // above center the low bits repeat the source's own low bits, which is what keeps the
        // maximum at the maximum instead of leaving the top of the range unreachable
        auto const repeatBits = sourceBits - 1;
        auto const repeatMask = (1u << repeatBits) - 1u;
        auto repeatValue = value & repeatMask;

        repeatValue = (scaleBits > repeatBits)
            ? repeatValue << (scaleBits - repeatBits)
            : repeatValue >> (repeatBits - scaleBits);

        while (repeatValue != 0)
        {
            scaled |= repeatValue;
            repeatValue >>= repeatBits;
        }

        return scaled;
    }
}
