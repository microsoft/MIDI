// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// This type is not projected, so unlike the generated implementation headers it has to bring in
// the projection itself to see the save result enum.
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiConfigFileSaveOutcome
    {
        svc::MidiServiceConfigSaveResult Result{ svc::MidiServiceConfigSaveResult::ErrorUnexpected };
        winrt::hstring ConfigFilePath{};
        winrt::hstring BackupFilePath{};
    };

    struct MidiConfigFile
    {
        // Folds one transport's section into the configuration file. The read, merge and write all
        // happen under a single file handle, so no other writer can act on data this call has
        // already read, and the service is never blocked from reading.
        static MidiConfigFileSaveOutcome SaveTransportSection(
            _In_ winrt::guid const& transportId,
            _In_ json::JsonObject const& transportSection) noexcept;

        // Empty when no configuration file is registered on this PC.
        static std::wstring ResolvePath() noexcept;

#ifdef _DEBUG
        // Debug builds only, see MidiServiceTransportPluginConfigManager.idl for why.
        static void SetPathOverride(_In_ std::wstring const& path) noexcept;
        static std::wstring GetPathOverride() noexcept;
#endif
    };
}
