// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceStatus.h"

namespace midiapp
{
    namespace
    {
        constexpr wchar_t MidiServiceName[] = L"MidiSrv";
    }

    bool IsMidiServiceRunning() noexcept
    {
        try
        {
            wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

            if (!manager)
            {
                return false;
            }

            wil::unique_schandle service{
                ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_QUERY_STATUS) };

            if (!service)
            {
                return false;
            }

            SERVICE_STATUS_PROCESS status{};
            DWORD bytesNeeded{ 0 };

            if (!::QueryServiceStatusEx(
                service.get(),
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status),
                sizeof(status),
                &bytesNeeded))
            {
                return false;
            }

            // Starting counts as not yet running. A tool that said "running" while the service
            // was still coming up would be reporting something a customer cannot use.
            return status.dwCurrentState == SERVICE_RUNNING;
        }
        catch (...)
        {
            return false;
        }
    }
}
