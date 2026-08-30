// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ServiceControl.h"
#include "SystemInfo.h"

namespace miditroubleshooter
{
    namespace
    {
        std::wstring FormatSystemError(_In_ DWORD const error) noexcept
        {
            try
            {
                wil::unique_hlocal_string message;

                auto const length = ::FormatMessageW(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr,
                    error,
                    0,
                    reinterpret_cast<LPWSTR>(message.put()),
                    0,
                    nullptr);

                if (length > 0 && message)
                {
                    std::wstring text{ message.get() };

                    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n'))
                    {
                        text.pop_back();
                    }

                    return text;
                }
            }
            catch (...)
            {
            }

            return std::format(L"Error 0x{:08X}", error);
        }

        ServiceState StateFromWin32(_In_ DWORD const state) noexcept
        {
            switch (state)
            {
            case SERVICE_STOPPED:           return ServiceState::Stopped;
            case SERVICE_START_PENDING:     return ServiceState::StartPending;
            case SERVICE_STOP_PENDING:      return ServiceState::StopPending;
            case SERVICE_RUNNING:           return ServiceState::Running;
            case SERVICE_CONTINUE_PENDING:  return ServiceState::ContinuePending;
            case SERVICE_PAUSE_PENDING:     return ServiceState::PausePending;
            case SERVICE_PAUSED:            return ServiceState::Paused;
            default:                        return ServiceState::Unknown;
            }
        }

        DWORD Win32FromStartMode(_In_ ServiceStartMode const mode) noexcept
        {
            switch (mode)
            {
            case ServiceStartMode::Boot:                return SERVICE_BOOT_START;
            case ServiceStartMode::System:              return SERVICE_SYSTEM_START;
            case ServiceStartMode::Automatic:           return SERVICE_AUTO_START;
            case ServiceStartMode::AutomaticDelayed:    return SERVICE_AUTO_START;
            case ServiceStartMode::Disabled:            return SERVICE_DISABLED;
            default:                                    return SERVICE_DEMAND_START;
            }
        }

        // The image path in the service configuration can be quoted and can carry arguments,
        // and neither form opens as a file.
        std::wstring ExecutablePathFromImagePath(_In_ std::wstring const& imagePath) noexcept
        {
            try
            {
                std::wstring path{ imagePath };

                if (!path.empty() && path.front() == L'"')
                {
                    auto const closing = path.find(L'"', 1);

                    if (closing != std::wstring::npos)
                    {
                        return path.substr(1, closing - 1);
                    }
                }

                auto const space = path.find(L' ');

                if (space != std::wstring::npos)
                {
                    path = path.substr(0, space);
                }

                // service image paths are frequently written with environment variables
                auto const required = ::ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);

                if (required > 0)
                {
                    std::wstring expanded(required, L'\0');

                    if (::ExpandEnvironmentStringsW(path.c_str(), expanded.data(), required) > 0)
                    {
                        expanded.resize(required - 1);
                        return expanded;
                    }
                }

                return path;
            }
            catch (...)
            {
                return imagePath;
            }
        }

        bool WaitForState(
            _In_ SC_HANDLE const service,
            _In_ DWORD const desiredState,
            _In_ std::chrono::seconds const timeout) noexcept
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            for (;;)
            {
                SERVICE_STATUS_PROCESS status{};
                DWORD bytesNeeded{ 0 };

                if (!::QueryServiceStatusEx(
                    service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
                {
                    return false;
                }

                if (status.dwCurrentState == desiredState)
                {
                    return true;
                }

                if (std::chrono::steady_clock::now() >= deadline)
                {
                    return false;
                }

                ::Sleep(250);
            }
        }
    }

    ServiceStatus QueryMidiServiceStatus() noexcept
    {
        ServiceStatus status{};

        try
        {
            wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

            if (!manager)
            {
                status.ErrorMessage = FormatSystemError(::GetLastError());
                return status;
            }

            wil::unique_schandle service{
                ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG) };

            if (!service)
            {
                auto const error = ::GetLastError();

                if (error == ERROR_SERVICE_DOES_NOT_EXIST)
                {
                    status.QuerySucceeded = true;
                    status.State = ServiceState::NotInstalled;
                }
                else
                {
                    status.ErrorMessage = FormatSystemError(error);
                }

                return status;
            }

            status.Installed = true;

            SERVICE_STATUS_PROCESS statusProcess{};
            DWORD bytesNeeded{ 0 };

            if (::QueryServiceStatusEx(
                service.get(),
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&statusProcess),
                sizeof(statusProcess),
                &bytesNeeded))
            {
                status.State = StateFromWin32(statusProcess.dwCurrentState);
                status.ProcessId = statusProcess.dwProcessId;
                status.QuerySucceeded = true;
            }
            else
            {
                status.ErrorMessage = FormatSystemError(::GetLastError());
            }

            // two calls: the first only reports the buffer size needed
            DWORD configSize{ 0 };
            ::QueryServiceConfigW(service.get(), nullptr, 0, &configSize);

            if (configSize > 0)
            {
                std::vector<std::byte> buffer(configSize);

                auto* const config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());

                if (::QueryServiceConfigW(service.get(), config, configSize, &configSize))
                {
                    if (config->lpDisplayName != nullptr)
                    {
                        status.DisplayName = config->lpDisplayName;
                    }

                    if (config->lpServiceStartName != nullptr)
                    {
                        status.AccountName = config->lpServiceStartName;
                    }

                    if (config->lpBinaryPathName != nullptr)
                    {
                        status.ImagePath = config->lpBinaryPathName;
                        status.ImageVersion = GetFileVersionString(ExecutablePathFromImagePath(status.ImagePath));
                    }

                    switch (config->dwStartType)
                    {
                    case SERVICE_BOOT_START:    status.StartMode = ServiceStartMode::Boot; break;
                    case SERVICE_SYSTEM_START:  status.StartMode = ServiceStartMode::System; break;
                    case SERVICE_AUTO_START:    status.StartMode = ServiceStartMode::Automatic; break;
                    case SERVICE_DEMAND_START:  status.StartMode = ServiceStartMode::Manual; break;
                    case SERVICE_DISABLED:      status.StartMode = ServiceStartMode::Disabled; break;
                    default:                    status.StartMode = ServiceStartMode::Unknown; break;
                    }
                }
            }

            if (status.StartMode == ServiceStartMode::Automatic)
            {
                DWORD delayedSize{ 0 };
                SERVICE_DELAYED_AUTO_START_INFO delayed{};

                if (::QueryServiceConfig2W(
                        service.get(),
                        SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                        reinterpret_cast<LPBYTE>(&delayed),
                        sizeof(delayed),
                        &delayedSize) &&
                    delayed.fDelayedAutostart)
                {
                    status.StartMode = ServiceStartMode::AutomaticDelayed;
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            status.ErrorMessage = ex.message();
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to query the MIDI service.");
        }
        catch (...)
        {
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to query the MIDI service.");
        }

        return status;
    }

    ServiceOperationResult RestartMidiService() noexcept
    {
        ServiceOperationResult result{};

        try
        {
            wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

            if (!manager)
            {
                result.ErrorMessage = FormatSystemError(::GetLastError());
                return result;
            }

            wil::unique_schandle service{
                ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP) };

            if (!service)
            {
                result.ErrorMessage = FormatSystemError(::GetLastError());
                return result;
            }

            SERVICE_STATUS_PROCESS statusProcess{};
            DWORD bytesNeeded{ 0 };

            if (::QueryServiceStatusEx(
                service.get(),
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&statusProcess),
                sizeof(statusProcess),
                &bytesNeeded) &&
                statusProcess.dwCurrentState != SERVICE_STOPPED)
            {
                SERVICE_STATUS stopStatus{};

                if (!::ControlService(service.get(), SERVICE_CONTROL_STOP, &stopStatus))
                {
                    auto const error = ::GetLastError();

                    // already on its way down, which is fine
                    if (error != ERROR_SERVICE_NOT_ACTIVE)
                    {
                        result.ErrorMessage = FormatSystemError(error);
                        return result;
                    }
                }

                if (!WaitForState(service.get(), SERVICE_STOPPED, std::chrono::seconds{ 30 }))
                {
                    result.ErrorMessage = L"The service did not stop in time.";
                    return result;
                }
            }

            if (!::StartServiceW(service.get(), 0, nullptr))
            {
                auto const error = ::GetLastError();

                if (error != ERROR_SERVICE_ALREADY_RUNNING)
                {
                    result.ErrorMessage = FormatSystemError(error);
                    return result;
                }
            }

            if (!WaitForState(service.get(), SERVICE_RUNNING, std::chrono::seconds{ 30 }))
            {
                result.ErrorMessage = L"The service did not start in time.";
                return result;
            }

            result.Succeeded = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            result.ErrorMessage = ex.message();
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to restart the MIDI service.");
        }
        catch (...)
        {
            result.ErrorMessage = L"An unexpected error occurred.";
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to restart the MIDI service.");
        }

        return result;
    }

    _Use_decl_annotations_
    ServiceOperationResult SetMidiServiceStartMode(ServiceStartMode mode) noexcept
    {
        ServiceOperationResult result{};

        try
        {
            wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

            if (!manager)
            {
                result.ErrorMessage = FormatSystemError(::GetLastError());
                return result;
            }

            wil::unique_schandle service{
                ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG) };

            if (!service)
            {
                result.ErrorMessage = FormatSystemError(::GetLastError());
                return result;
            }

            if (!::ChangeServiceConfigW(
                service.get(),
                SERVICE_NO_CHANGE,
                Win32FromStartMode(mode),
                SERVICE_NO_CHANGE,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
            {
                result.ErrorMessage = FormatSystemError(::GetLastError());
                return result;
            }

            // Only meaningful for an automatic start, but writing it for the others as well
            // clears a stale delayed flag left behind by an earlier change.
            SERVICE_DELAYED_AUTO_START_INFO delayed{};
            delayed.fDelayedAutostart = mode == ServiceStartMode::AutomaticDelayed ? TRUE : FALSE;

            ::ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);

            result.Succeeded = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            result.ErrorMessage = ex.message();
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to change the MIDI service start mode.");
        }
        catch (...)
        {
            result.ErrorMessage = L"An unexpected error occurred.";
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to change the MIDI service start mode.");
        }

        return result;
    }
}
