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
            uint32_t VirtualKeyCode;
            int32_t Semitones;
            wchar_t const* Label;
        };

        // The layout trackers and DAWs have used for decades: the bottom two rows play one
        // octave from the lowest displayed C, the top two rows play the octave above it.
        constexpr ComputerKeyMapping ComputerKeys[]
        {
            { 'Z',  0, L"Z" },  { 'S',  1, L"S" },  { 'X',  2, L"X" },  { 'D',  3, L"D" },
            { 'C',  4, L"C" },  { 'V',  5, L"V" },  { 'G',  6, L"G" },  { 'B',  7, L"B" },
            { 'H',  8, L"H" },  { 'N',  9, L"N" },  { 'J', 10, L"J" },  { 'M', 11, L"M" },
            { VK_OEM_COMMA, 12, L"," }, { 'L', 13, L"L" }, { VK_OEM_PERIOD, 14, L"." },
            { VK_OEM_1, 15, L";" }, { VK_OEM_2, 16, L"/" },

            { 'Q', 12, L"Q" },  { '2', 13, L"2" },  { 'W', 14, L"W" },  { '3', 15, L"3" },
            { 'E', 16, L"E" },  { 'R', 17, L"R" },  { '5', 18, L"5" },  { 'T', 19, L"T" },
            { '6', 20, L"6" },  { 'Y', 21, L"Y" },  { '7', 22, L"7" },  { 'U', 23, L"U" },
            { 'I', 24, L"I" },  { '9', 25, L"9" },  { 'O', 26, L"O" },  { '0', 27, L"0" },
            { 'P', 28, L"P" },
        };
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

    _Use_decl_annotations_
    std::wstring ComputerKeyLabel(int32_t semitonesFromBottom) noexcept
    {
        for (auto const& mapping : ComputerKeys)
        {
            if (mapping.Semitones == semitonesFromBottom)
            {
                return mapping.Label;
            }
        }

        return {};
    }

    _Use_decl_annotations_
    int32_t ComputerKeyToSemitones(uint32_t virtualKeyCode) noexcept
    {
        for (auto const& mapping : ComputerKeys)
        {
            if (mapping.VirtualKeyCode == virtualKeyCode)
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
