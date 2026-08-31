// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midisysextool
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // messages per transfer, to throttle for older hardware
        uint32_t SingleTransferMessageCount() const noexcept { return m_singleTransferMessageCount; }
        void SingleTransferMessageCount(uint32_t value) noexcept;

        uint32_t TransferSpacingMilliseconds() const noexcept { return m_transferSpacingMilliseconds; }
        void TransferSpacingMilliseconds(uint32_t value) noexcept;

        // how much room to reserve for an incoming dump before it has to grow
        uint32_t InitialReceiveBufferKb() const noexcept { return m_initialReceiveBufferKb; }
        void InitialReceiveBufferKb(uint32_t value) noexcept;

        std::wstring LibraryFolder() const noexcept { return m_libraryFolder; }
        void LibraryFolder(std::wstring const& value) noexcept;

        // The file pickers silently fall back to Documents when the folder is missing, so it
        // has to exist before it can act as the default.
        void EnsureLibraryFolderExists() const noexcept;

        // Documents\SysEx Library
        static std::wstring DefaultLibraryFolder() noexcept;

        static constexpr uint32_t MinimumSingleTransferMessageCount = 1;
        static constexpr uint32_t MaximumSingleTransferMessageCount = 4096;
        static constexpr uint32_t DefaultSingleTransferMessageCount = 64;

        static constexpr uint32_t MinimumTransferSpacingMilliseconds = 0;
        static constexpr uint32_t MaximumTransferSpacingMilliseconds = 5000;
        static constexpr uint32_t DefaultTransferSpacingMilliseconds = 5;

        static constexpr uint32_t MinimumInitialReceiveBufferKb = 16;
        static constexpr uint32_t MaximumInitialReceiveBufferKb = 65536;
        static constexpr uint32_t DefaultInitialReceiveBufferKb = 256;

    private:
        AppSettings() noexcept;

        uint32_t m_singleTransferMessageCount{ DefaultSingleTransferMessageCount };
        uint32_t m_transferSpacingMilliseconds{ DefaultTransferSpacingMilliseconds };
        uint32_t m_initialReceiveBufferKb{ DefaultInitialReceiveBufferKb };
        std::wstring m_libraryFolder{};
    };
}
