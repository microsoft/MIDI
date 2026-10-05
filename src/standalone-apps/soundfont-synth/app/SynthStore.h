// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisoundfontsynth
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

    // The synths, kept in a JSON file in the customer's local app data. The file is the
    // customer's to edit, so it is read as untrusted input.
    class SynthStore
    {
    public:
        static std::vector<SynthDefinition> Load() noexcept;
        static bool Save(_In_ std::vector<SynthDefinition> const& synths) noexcept;

        static std::wstring FilePath() noexcept;

        static constexpr size_t MaximumSynths = 64;

        // What fits in a UMP endpoint name and still works as a MIDI 1.0 port name.
        static constexpr size_t MaximumNameLength = 31;

        static std::wstring NewId();
        static std::string NewProductInstanceId();
    };
}
