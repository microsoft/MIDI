// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

namespace gspike
{
    struct SpikeOptions
    {
        std::wstring Mode{ L"xaml" };
        uint32_t ControlCount{ 200 };
        uint32_t AnimatedCount{ 12 };
        double Seconds{ 20.0 };
        std::wstring EndpointDeviceId;
        std::wstring OutputPath;
        bool AutoRun{ false };
        bool SkipMidi{ false };
        std::wstring ParseError;

        static SpikeOptions ParseProcessCommandLine();
    };
}
