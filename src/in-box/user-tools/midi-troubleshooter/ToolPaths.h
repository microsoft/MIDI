// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // The console tools and support files this app drives. Each is resolved from the SDK
    // registration first, then from the install convention, then from the folder this
    // executable lives in, so a developer build works without an installer.
    struct ToolLocations
    {
        std::wstring MidiDiag{};
        std::wstring MidiKsInfo{};

        // providers.wprp, which is the one external file the repro capture still needs
        std::wstring ReproProfile{};

        std::wstring WindowsPerformanceRecorder{};
        std::wstring TimeTravelTracer{};
        std::wstring PnpUtil{};
        std::wstring DdoDiag{};
        std::wstring DxDiag{};
    };

    ToolLocations GetToolLocations() noexcept;

    // Re-resolves everything. Called when a page is shown, so installing the SDK while the
    // app is open does not require a restart.
    void RefreshToolLocations() noexcept;

    // %windir%\System32, or Sysnative when this is a 32 bit process on 64 bit Windows.
    std::wstring GetNativeSystem32Folder() noexcept;

    std::wstring GetExecutableFolder() noexcept;

    bool FileExists(std::wstring const& path) noexcept;
}
