// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "ServiceConfig.MidiServiceConfigSaveResponse.g.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceConfigSaveResponse : MidiServiceConfigSaveResponseT<MidiServiceConfigSaveResponse>
    {
        MidiServiceConfigSaveResponse() = default;

        svc::MidiServiceConfigSaveResult Result() const noexcept { return m_result; }
        bool Success() const noexcept { return m_result == svc::MidiServiceConfigSaveResult::Success; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        winrt::hstring ConfigFilePath() const noexcept { return m_configFilePath; }
        winrt::hstring BackupFilePath() const noexcept { return m_backupFilePath; }

        void InternalSetResult(_In_ svc::MidiServiceConfigSaveResult const result);
        void InternalSetSuccess() noexcept;
        void InternalSetConfigFilePath(_In_ winrt::hstring const& path) noexcept { m_configFilePath = path; }
        void InternalSetBackupFilePath(_In_ winrt::hstring const& path) noexcept { m_backupFilePath = path; }

    private:
        svc::MidiServiceConfigSaveResult m_result{ svc::MidiServiceConfigSaveResult::ErrorUnexpected };
        winrt::hstring m_errorMessage{};
        winrt::hstring m_configFilePath{};
        winrt::hstring m_backupFilePath{};
    };
}
