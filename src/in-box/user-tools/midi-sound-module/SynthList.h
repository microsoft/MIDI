// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What the synth list file holds, apart from reading and writing the file, so the tests can check
// it without one. The file is the customer's to edit, so it is read as untrusted input.

#pragma once

#include <sal.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace midisoundmodule
{
    // One synth as the customer set it up.
    struct SynthDefinition
    {
        // Stable for the life of the synth. Never shown.
        std::wstring Id{};

        // The endpoint name apps see.
        std::wstring Name{};

        std::wstring SoundFontPath{};

        // Printable ASCII. The service builds the endpoint id from it, so it never changes
        // after the synth is created, or every app would lose track of the endpoint.
        std::string ProductInstanceId{};

        double VolumeDb{ 0.0 };
        bool Enabled{ true };
    };

    constexpr size_t MaximumSynths = 64;

    // What fits in a UMP endpoint name and still works as a MIDI 1.0 port name.
    constexpr size_t MaximumSynthNameLength = 31;

    // The format this app writes. A file a newer app wrote is left alone.
    constexpr int32_t SynthListFileVersion = 1;

    namespace SynthListKeys
    {
        constexpr wchar_t Comment[] = L"_comment";
        constexpr wchar_t Version[] = L"version";
        constexpr wchar_t Synths[] = L"synths";
        constexpr wchar_t Id[] = L"id";
        constexpr wchar_t Name[] = L"name";
        constexpr wchar_t SoundFont[] = L"soundFont";
        constexpr wchar_t ProductInstanceId[] = L"productInstanceId";
        constexpr wchar_t VolumeDb[] = L"volumeDb";
        constexpr wchar_t Enabled[] = L"enabled";
    }

    // Why a synth list can't be used as it is. The app won't start on one of these, because
    // starting with no synths would save an empty list over the customer's own.
    enum class SynthListProblem
    {
        None,
        CannotOpen,
        TooLarge,
        NotJson,
        NoSynthList,
        NewerVersion,
        TooManySynths,
        EntryNotASynth,
        EntryId,
        EntrySoundBank,
        EntryDuplicateId,
    };

    struct SynthList
    {
        std::vector<SynthDefinition> Synths{};
        SynthListProblem Problem{ SynthListProblem::None };

        // The synth a problem is with, counted from 1 in the order of the file.
        size_t Entry{ 0 };

        // Why Windows couldn't open or read the file.
        uint32_t SystemError{ 0 };
    };

    // Reads the JSON in the file. A synth whose product instance id is missing, unusable or already
    // another synth's comes back without one, and the caller makes a new one. Names are tidied and
    // volumes brought into range rather than refused, because nothing is lost by that.
    SynthList ParseSynthList(_In_ std::string_view utf8) noexcept;
}
