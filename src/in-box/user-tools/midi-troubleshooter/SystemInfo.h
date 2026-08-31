// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // What a support engineer asks for first. Gathered once at startup, because none of it
    // changes while the app is open.
    struct SystemInformation
    {
        std::wstring WindowsVersion{};
        std::wstring WindowsEdition{};
        std::wstring WindowsArchitecture{};
        std::wstring CultureName{};
        std::wstring WindowsAppSdkVersion{};
        std::wstring MidiSdkVersion{};
        std::wstring ToolVersion{};
        std::wstring ComputerName{};

        // true when the customer has turned on Developer Mode, which is what lets an unsigned
        // service transport load. See the service plugin knowledge base article.
        bool DeveloperModeEnabled{ false };
    };

    SystemInformation GatherSystemInformation() noexcept;

    // "10.0.26100.1234" style string from a file's version resource. Empty when the file has
    // no version resource or cannot be read.
    std::wstring GetFileVersionString(std::wstring const& filePath) noexcept;
}
