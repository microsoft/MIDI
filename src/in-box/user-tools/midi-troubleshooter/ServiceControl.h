// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    enum class ServiceState
    {
        Unknown = 0,
        NotInstalled,
        Stopped,
        StartPending,
        StopPending,
        Running,
        ContinuePending,
        PausePending,
        Paused
    };

    enum class ServiceStartMode
    {
        Unknown = 0,
        Boot,
        System,
        Automatic,
        AutomaticDelayed,
        Manual,
        Disabled
    };

    struct ServiceStatus
    {
        bool Installed{ false };
        bool QuerySucceeded{ false };

        ServiceState State{ ServiceState::Unknown };
        ServiceStartMode StartMode{ ServiceStartMode::Unknown };

        std::wstring DisplayName{};
        std::wstring AccountName{};
        std::wstring ImagePath{};
        std::wstring ImageVersion{};

        uint32_t ProcessId{ 0 };

        // set when the query itself failed, for example because the service control manager
        // could not be opened
        std::wstring ErrorMessage{};
    };

    struct ServiceOperationResult
    {
        bool Succeeded{ false };
        std::wstring ErrorMessage{};
    };

    constexpr wchar_t MidiServiceName[] = L"MidiSrv";

    // Blocking. Callers run these from a background thread.
    ServiceStatus QueryMidiServiceStatus() noexcept;

    ServiceOperationResult RestartMidiService() noexcept;

    // Nice to have rather than a repair: a service set to Manual still starts on demand, so
    // this only changes when it starts, not whether MIDI works.
    ServiceOperationResult SetMidiServiceStartMode(ServiceStartMode mode) noexcept;
}
