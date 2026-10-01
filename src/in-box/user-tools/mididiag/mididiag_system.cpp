// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// Facts about the PC that come from Windows itself. Nothing in this file calls the MIDI
// service, so all of it is still reported when the service is stuck.

#include "pch.h"
#include "mididiag_output.h"
#include "mididiag_sections.h"

#include <cfgmgr32.h>
#include <devpkey.h>
#include <wbemidl.h>
#include <netfw.h>
#include <netlistmgr.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "version.lib")

using namespace mididiag;

namespace
{
    constexpr wchar_t MidiServiceName[] = L"midisrv";
    constexpr wchar_t CurrentVersionKey[] = LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)";

    constexpr uint32_t HistoryDays{ 30 };
    constexpr uint32_t MaxHistoryEntries{ 20 };
    constexpr uint32_t MaxProblemDeviceFindings{ 5 };

    // Activation slowed to several seconds per endpoint at about 1,800 of these on a test PC
    constexpr uint32_t LeftoverEndpointNodeFindingThreshold{ 1000 };

    // GUID_DEVCLASS_MEDIA
    constexpr GUID MediaDeviceClass{ 0x4d36e96c, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

    std::wstring LowerCopy(_In_ std::wstring_view const text)
    {
        std::wstring lower{ text };

        for (auto& ch : lower)
        {
            ch = static_cast<wchar_t>(::towlower(ch));
        }

        return lower;
    }

    bool ContainsNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const part)
    {
        return LowerCopy(text).find(LowerCopy(part)) != std::wstring::npos;
    }

    bool StartsWithNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const prefix)
    {
        return text.size() >= prefix.size() && LowerCopy(text.substr(0, prefix.size())) == LowerCopy(prefix);
    }

    bool EndsWithNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const suffix)
    {
        return text.size() >= suffix.size() && LowerCopy(text.substr(text.size() - suffix.size())) == LowerCopy(suffix);
    }

    std::optional<std::wstring> TryReadRegistryString(_In_ HKEY const root, _In_ PCWSTR const subKey, _In_ PCWSTR const valueName)
    {
        try
        {
            return wil::reg::try_get_value_string(root, subKey, valueName);
        }
        catch (...)
        {
            // a value of the wrong type
            return std::nullopt;
        }
    }

    std::optional<DWORD> TryReadRegistryDword(_In_ HKEY const root, _In_ PCWSTR const subKey, _In_ PCWSTR const valueName)
    {
        try
        {
            return wil::reg::try_get_value_dword(root, subKey, valueName);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    // Service image paths can be quoted, carry arguments and use environment variables.
    std::wstring ExecutablePathFromImagePath(_In_ std::wstring const& imagePath)
    {
        std::wstring path{ imagePath };

        if (!path.empty() && path.front() == L'"')
        {
            auto const closing = path.find(L'"', 1);
            path = closing == std::wstring::npos ? path.substr(1) : path.substr(1, closing - 1);
        }
        else if (auto const space = path.find(L' '); space != std::wstring::npos)
        {
            path = path.substr(0, space);
        }

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

    // ------------------------------------------------------------------------------------------
    // Service control manager

    // A kernel driver's image path is \SystemRoot\..., System32\... or \??\C:\...
    std::wstring DriverPathFromImagePath(_In_ std::wstring const& imagePath)
    {
        constexpr std::wstring_view systemRootPrefix{ LR"(\SystemRoot\)" };
        constexpr std::wstring_view objectManagerPrefix{ LR"(\??\)" };

        wchar_t windowsFolder[MAX_PATH]{};
        ::GetWindowsDirectoryW(windowsFolder, ARRAYSIZE(windowsFolder));

        if (StartsWithNoCase(imagePath, systemRootPrefix))
        {
            // keep the backslash that ends the prefix
            return std::wstring{ windowsFolder } + imagePath.substr(systemRootPrefix.size() - 1);
        }

        if (StartsWithNoCase(imagePath, objectManagerPrefix))
        {
            return imagePath.substr(objectManagerPrefix.size());
        }

        if (StartsWithNoCase(imagePath, LR"(System32\)"))
        {
            return std::wstring{ windowsFolder } + L"\\" + imagePath;
        }

        return ExecutablePathFromImagePath(imagePath);
    }

    struct ServiceQuery
    {
        bool Queried{ false };
        bool Installed{ false };
        DWORD Error{ ERROR_SUCCESS };

        DWORD State{ 0 };
        DWORD ProcessId{ 0 };

        DWORD StartType{ SERVICE_DEMAND_START };
        bool DelayedStart{ false };
        uint32_t TriggerCount{ 0 };
        std::wstring Account{};
        std::wstring ImagePath{};
    };

    ServiceQuery g_serviceBeforeReport{};

    ServiceQuery QueryMidiService(_In_ bool const includeConfiguration)
    {
        ServiceQuery result{};

        wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

        if (!manager)
        {
            result.Error = ::GetLastError();
            return result;
        }

        DWORD const access = SERVICE_QUERY_STATUS | (includeConfiguration ? SERVICE_QUERY_CONFIG : 0);
        wil::unique_schandle service{ ::OpenServiceW(manager.get(), MidiServiceName, access) };

        if (!service)
        {
            result.Error = ::GetLastError();
            result.Queried = result.Error == ERROR_SERVICE_DOES_NOT_EXIST;

            return result;
        }

        result.Installed = true;

        SERVICE_STATUS_PROCESS status{};
        DWORD bytesNeeded{ 0 };

        if (::QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
        {
            result.Queried = true;
            result.State = status.dwCurrentState;
            result.ProcessId = status.dwProcessId;
        }
        else
        {
            result.Error = ::GetLastError();
        }

        if (!includeConfiguration)
        {
            return result;
        }

        DWORD configSize{ 0 };
        ::QueryServiceConfigW(service.get(), nullptr, 0, &configSize);

        if (configSize > 0)
        {
            std::vector<uint8_t> buffer(configSize);
            auto const config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());

            if (::QueryServiceConfigW(service.get(), config, configSize, &configSize))
            {
                result.StartType = config->dwStartType;
                result.Account = config->lpServiceStartName == nullptr ? L"" : config->lpServiceStartName;
                result.ImagePath = config->lpBinaryPathName == nullptr ? L"" : config->lpBinaryPathName;
            }
        }

        if (result.StartType == SERVICE_AUTO_START)
        {
            SERVICE_DELAYED_AUTO_START_INFO delayed{};
            DWORD delayedSize{ 0 };

            if (::QueryServiceConfig2W(service.get(), SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                reinterpret_cast<LPBYTE>(&delayed), sizeof(delayed), &delayedSize))
            {
                result.DelayedStart = delayed.fDelayedAutostart != FALSE;
            }
        }

        DWORD triggerSize{ 0 };
        ::QueryServiceConfig2W(service.get(), SERVICE_CONFIG_TRIGGER_INFO, nullptr, 0, &triggerSize);

        if (triggerSize >= sizeof(SERVICE_TRIGGER_INFO))
        {
            std::vector<uint8_t> buffer(triggerSize);

            if (::QueryServiceConfig2W(service.get(), SERVICE_CONFIG_TRIGGER_INFO, buffer.data(), triggerSize, &triggerSize))
            {
                result.TriggerCount = reinterpret_cast<SERVICE_TRIGGER_INFO const*>(buffer.data())->cTriggers;
            }
        }

        return result;
    }

    std::wstring ServiceStateName(_In_ DWORD const state)
    {
        switch (state)
        {
        case SERVICE_STOPPED:           return L"stopped";
        case SERVICE_START_PENDING:     return L"start_pending";
        case SERVICE_STOP_PENDING:      return L"stop_pending";
        case SERVICE_RUNNING:           return L"running";
        case SERVICE_CONTINUE_PENDING:  return L"continue_pending";
        case SERVICE_PAUSE_PENDING:     return L"pause_pending";
        case SERVICE_PAUSED:            return L"paused";
        default:                        return L"unknown";
        }
    }

    std::wstring ServiceStartTypeName(_In_ ServiceQuery const& service)
    {
        switch (service.StartType)
        {
        case SERVICE_BOOT_START:    return L"boot";
        case SERVICE_SYSTEM_START:  return L"system";
        case SERVICE_AUTO_START:    return service.DelayedStart ? L"automatic_delayed" : L"automatic";
        case SERVICE_DEMAND_START:  return L"manual";
        case SERVICE_DISABLED:      return L"disabled";
        default:                    return L"unknown";
        }
    }

    // ------------------------------------------------------------------------------------------
    // midisrv.exe processes, from WMI because opening a service process needs elevation

    struct ServiceProcess
    {
        uint32_t ProcessId{ 0 };
        uint32_t ThreadCount{ 0 };
        uint32_t HandleCount{ 0 };
        FILETIME StartTime{};
    };

    // WMI writes a creation date as local time plus its offset from UTC in minutes:
    // 20260930232309.123456-240
    bool ParseWmiDateTime(_In_ std::wstring_view const text, _Out_ FILETIME& utcTime)
    {
        utcTime = {};

        if (text.size() < 25)
        {
            return false;
        }

        auto const number = [&text](size_t const start, size_t const length) -> int
            {
                int value{ 0 };

                for (size_t i = start; i < start + length; i++)
                {
                    if (text[i] < L'0' || text[i] > L'9')
                    {
                        return -1;
                    }

                    value = value * 10 + (text[i] - L'0');
                }

                return value;
            };

        SYSTEMTIME local{};
        local.wYear = static_cast<WORD>(number(0, 4));
        local.wMonth = static_cast<WORD>(number(4, 2));
        local.wDay = static_cast<WORD>(number(6, 2));
        local.wHour = static_cast<WORD>(number(8, 2));
        local.wMinute = static_cast<WORD>(number(10, 2));
        local.wSecond = static_cast<WORD>(number(12, 2));

        auto const offsetMinutes = number(22, 3);

        if (offsetMinutes < 0 || (text[21] != L'+' && text[21] != L'-'))
        {
            return false;
        }

        FILETIME asIfUtc{};

        if (!::SystemTimeToFileTime(&local, &asIfUtc))
        {
            return false;
        }

        ULARGE_INTEGER value{};
        value.LowPart = asIfUtc.dwLowDateTime;
        value.HighPart = asIfUtc.dwHighDateTime;

        int64_t const offsetTicks = static_cast<int64_t>(offsetMinutes) * 60 * 10'000'000;
        value.QuadPart = text[21] == L'+' ? value.QuadPart - offsetTicks : value.QuadPart + offsetTicks;

        utcTime.dwLowDateTime = value.LowPart;
        utcTime.dwHighDateTime = value.HighPart;

        return true;
    }

    std::vector<ServiceProcess> FindMidiServiceProcesses()
    {
        std::vector<ServiceProcess> processes{};

        auto const locator = wil::CoCreateInstance<WbemLocator, IWbemLocator>(CLSCTX_INPROC_SERVER);

        wil::com_ptr<IWbemServices> services{};
        THROW_IF_FAILED(locator->ConnectServer(wil::make_bstr(L"ROOT\\CIMV2").get(),
            nullptr, nullptr, nullptr, 0, nullptr, nullptr, services.put()));

        THROW_IF_FAILED(::CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE));

        wil::com_ptr<IEnumWbemClassObject> results{};
        THROW_IF_FAILED(services->ExecQuery(
            wil::make_bstr(L"WQL").get(),
            wil::make_bstr(L"SELECT ProcessId, CreationDate, ThreadCount, HandleCount FROM Win32_Process WHERE Name = 'midisrv.exe'").get(),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            results.put()));

        for (;;)
        {
            wil::com_ptr<IWbemClassObject> item{};
            ULONG returned{ 0 };

            if (results->Next(10000, 1, item.put(), &returned) != WBEM_S_NO_ERROR || returned == 0)
            {
                break;
            }

            ServiceProcess process{};
            wil::unique_variant value{};

            if (SUCCEEDED(item->Get(L"ProcessId", 0, value.reset_and_addressof(), nullptr, nullptr)) && value.vt == VT_I4)
            {
                process.ProcessId = static_cast<uint32_t>(value.lVal);
            }

            if (SUCCEEDED(item->Get(L"ThreadCount", 0, value.reset_and_addressof(), nullptr, nullptr)) && value.vt == VT_I4)
            {
                process.ThreadCount = static_cast<uint32_t>(value.lVal);
            }

            if (SUCCEEDED(item->Get(L"HandleCount", 0, value.reset_and_addressof(), nullptr, nullptr)) && value.vt == VT_I4)
            {
                process.HandleCount = static_cast<uint32_t>(value.lVal);
            }

            if (SUCCEEDED(item->Get(L"CreationDate", 0, value.reset_and_addressof(), nullptr, nullptr)) && value.vt == VT_BSTR && value.bstrVal != nullptr)
            {
                ParseWmiDateTime(value.bstrVal, process.StartTime);
            }

            processes.push_back(process);
        }

        return processes;
    }

    std::chrono::seconds SecondsSince(_In_ FILETIME const& utcTime)
    {
        FILETIME now{};
        ::GetSystemTimeAsFileTime(&now);

        ULARGE_INTEGER nowValue{};
        nowValue.LowPart = now.dwLowDateTime;
        nowValue.HighPart = now.dwHighDateTime;

        ULARGE_INTEGER thenValue{};
        thenValue.LowPart = utcTime.dwLowDateTime;
        thenValue.HighPart = utcTime.dwHighDateTime;

        if (thenValue.QuadPart == 0 || thenValue.QuadPart > nowValue.QuadPart)
        {
            return std::chrono::seconds{ 0 };
        }

        return std::chrono::seconds{ static_cast<int64_t>((nowValue.QuadPart - thenValue.QuadPart) / 10'000'000) };
    }

    // ------------------------------------------------------------------------------------------
    // Event log

    class RenderedEvent
    {
    public:
        RenderedEvent(_In_ EVT_HANDLE const context, _In_ EVT_HANDLE const event)
        {
            DWORD used{ 0 };
            DWORD count{ 0 };

            ::EvtRender(context, event, EvtRenderEventValues, 0, nullptr, &used, &count);

            if (used == 0)
            {
                return;
            }

            // backed by 64-bit storage so the variants inside it are correctly aligned
            m_buffer.resize((used + sizeof(uint64_t) - 1) / sizeof(uint64_t));

            if (::EvtRender(context, event, EvtRenderEventValues, used, m_buffer.data(), &used, &count))
            {
                m_count = count;
            }
        }

        std::wstring String(_In_ size_t const index) const
        {
            auto const value = Value(index);

            if (value != nullptr && value->Type == EvtVarTypeString && value->StringVal != nullptr)
            {
                return value->StringVal;
            }

            return {};
        }

        // numbers come back as numbers or as text depending on how the provider declared them
        std::wstring Hex(_In_ size_t const index) const
        {
            auto const value = Value(index);

            if (value == nullptr)
            {
                return {};
            }

            switch (value->Type)
            {
            case EvtVarTypeUInt32:
            case EvtVarTypeHexInt32:
                return std::format(L"0x{:08x}", value->UInt32Val);
            case EvtVarTypeUInt64:
            case EvtVarTypeHexInt64:
                return std::format(L"0x{:x}", value->UInt64Val);
            case EvtVarTypeString:
                if (value->StringVal != nullptr)
                {
                    std::wstring text{ value->StringVal };
                    return StartsWithNoCase(text, L"0x") ? text : L"0x" + text;
                }
                return {};
            default:
                return {};
            }
        }

        uint64_t Number(_In_ size_t const index) const
        {
            auto const value = Value(index);

            if (value == nullptr)
            {
                return 0;
            }

            switch (value->Type)
            {
            case EvtVarTypeByte:        return value->ByteVal;
            case EvtVarTypeSByte:       return value->SByteVal < 0 ? 0 : static_cast<uint64_t>(value->SByteVal);
            case EvtVarTypeInt16:       return value->Int16Val < 0 ? 0 : static_cast<uint64_t>(value->Int16Val);
            case EvtVarTypeInt32:       return value->Int32Val < 0 ? 0 : static_cast<uint64_t>(value->Int32Val);
            case EvtVarTypeInt64:       return value->Int64Val < 0 ? 0 : static_cast<uint64_t>(value->Int64Val);
            case EvtVarTypeUInt16:      return value->UInt16Val;
            case EvtVarTypeUInt32:
            case EvtVarTypeHexInt32:    return value->UInt32Val;
            case EvtVarTypeUInt64:
            case EvtVarTypeHexInt64:    return value->UInt64Val;
            default:                    return 0;
            }
        }

        FILETIME Time(_In_ size_t const index) const
        {
            FILETIME time{};
            auto const value = Value(index);

            if (value != nullptr && value->Type == EvtVarTypeFileTime)
            {
                time.dwLowDateTime = static_cast<DWORD>(value->FileTimeVal & 0xFFFFFFFF);
                time.dwHighDateTime = static_cast<DWORD>(value->FileTimeVal >> 32);
            }

            return time;
        }

        // the service control manager puts the service's key name here as UTF-16
        std::wstring BinaryText(_In_ size_t const index) const
        {
            auto const value = Value(index);

            if (value == nullptr || value->Type != EvtVarTypeBinary || value->BinaryVal == nullptr)
            {
                return {};
            }

            std::wstring text(reinterpret_cast<wchar_t const*>(value->BinaryVal), value->Count / sizeof(wchar_t));

            if (auto const end = text.find(L'\0'); end != std::wstring::npos)
            {
                text.resize(end);
            }

            return text;
        }

    private:
        EVT_VARIANT const* Value(_In_ size_t const index) const
        {
            if (index >= m_count)
            {
                return nullptr;
            }

            return reinterpret_cast<EVT_VARIANT const*>(m_buffer.data()) + index;
        }

        std::vector<uint64_t> m_buffer{};
        DWORD m_count{ 0 };
    };

    // Calls back with each matching event, newest first. False when the log could not be read.
    bool ForEachEvent(
        _In_ PCWSTR const channel,
        _In_ std::wstring const& query,
        _In_ std::vector<PCWSTR> const& valuePaths,
        _In_ std::function<void(RenderedEvent const&)> const& callback)
    {
        wil::unique_evt_handle context{ ::EvtCreateRenderContext(static_cast<DWORD>(valuePaths.size()),
            const_cast<PCWSTR*>(valuePaths.data()), EvtRenderContextValues) };

        wil::unique_evt_handle results{ ::EvtQuery(nullptr, channel, query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection) };

        if (!context || !results)
        {
            return false;
        }

        for (;;)
        {
            EVT_HANDLE batch[16]{};
            DWORD returned{ 0 };

            if (!::EvtNext(results.get(), ARRAYSIZE(batch), batch, 5000, 0, &returned))
            {
                break;
            }

            std::vector<wil::unique_evt_handle> events{};

            for (DWORD i = 0; i < returned; i++)
            {
                events.emplace_back(batch[i]);
            }

            for (auto const& event : events)
            {
                callback(RenderedEvent{ context.get(), event.get() });
            }
        }

        return true;
    }

    std::wstring HistoryTimeFilter()
    {
        return std::format(L"TimeCreated[timediff(@SystemTime) <= {}]", static_cast<uint64_t>(HistoryDays) * 86'400'000);
    }

    std::wstring BootTypeName(_In_ uint64_t const bootType)
    {
        switch (bootType)
        {
        case 0:     return L"full";
        case 1:     return L"fast_startup";
        case 2:     return L"resume_from_hibernation";
        default:    return std::to_wstring(bootType);
        }
    }

    bool IsMidiModule(_In_ std::wstring const& moduleName)
    {
        return EndsWithNoCase(moduleName, L"wdmaud2.drv") ||
            EndsWithNoCase(moduleName, L"Devices.Midi2.dll") ||
            StartsWithNoCase(moduleName, L"Midi2.");
    }

    // ------------------------------------------------------------------------------------------
    // Device nodes

    std::vector<std::wstring> DeviceIdList(_In_opt_ PCWSTR const filter, _In_ ULONG const flags)
    {
        std::vector<std::wstring> ids{};
        ULONG length{ 0 };

        if (::CM_Get_Device_ID_List_SizeW(&length, filter, flags) != CR_SUCCESS || length == 0)
        {
            return ids;
        }

        std::vector<wchar_t> buffer(length);

        if (::CM_Get_Device_ID_ListW(filter, buffer.data(), length, flags) != CR_SUCCESS)
        {
            return ids;
        }

        for (auto id = buffer.data(); *id != L'\0'; id += wcslen(id) + 1)
        {
            ids.emplace_back(id);
        }

        return ids;
    }

    std::wstring DeviceStringProperty(_In_ DEVINST const device, _In_ DEVPROPKEY const& key)
    {
        DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };
        ULONG size{ 0 };

        if (::CM_Get_DevNode_PropertyW(device, &key, &type, nullptr, &size, 0) != CR_BUFFER_SMALL || size == 0)
        {
            return {};
        }

        std::vector<uint8_t> buffer(size);

        if (::CM_Get_DevNode_PropertyW(device, &key, &type, buffer.data(), &size, 0) != CR_SUCCESS ||
            (type != DEVPROP_TYPE_STRING && type != DEVPROP_TYPE_STRING_LIST))
        {
            return {};
        }

        // the first string of a list is enough here
        return std::wstring{ reinterpret_cast<wchar_t const*>(buffer.data()) };
    }

    bool DeviceClassIs(_In_ DEVINST const device, _In_ GUID const& expected)
    {
        DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };
        GUID classGuid{};
        ULONG size{ sizeof(classGuid) };

        return ::CM_Get_DevNode_PropertyW(device, &DEVPKEY_Device_ClassGuid, &type,
            reinterpret_cast<PBYTE>(&classGuid), &size, 0) == CR_SUCCESS &&
            type == DEVPROP_TYPE_GUID &&
            classGuid == expected;
    }

    FILETIME DeviceFileTimeProperty(_In_ DEVINST const device, _In_ DEVPROPKEY const& key)
    {
        FILETIME value{};
        DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };
        ULONG size{ sizeof(value) };

        if (::CM_Get_DevNode_PropertyW(device, &key, &type, reinterpret_cast<PBYTE>(&value), &size, 0) != CR_SUCCESS ||
            type != DEVPROP_TYPE_FILETIME)
        {
            return {};
        }

        return value;
    }

    // SWD\MIDISRV\MIDIU_KSA_123 -> KSA
    std::wstring TransportCodeFromInstanceId(_In_ std::wstring const& instanceId)
    {
        constexpr std::wstring_view prefix{ L"MIDIU_" };

        auto const lastSeparator = instanceId.rfind(L'\\');
        auto const leaf = lastSeparator == std::wstring::npos ? instanceId : instanceId.substr(lastSeparator + 1);

        if (!StartsWithNoCase(leaf, prefix))
        {
            return L"other";
        }

        auto const end = leaf.find(L'_', prefix.size());
        auto code = leaf.substr(prefix.size(), end == std::wstring::npos ? std::wstring::npos : end - prefix.size());

        for (auto& ch : code)
        {
            ch = static_cast<wchar_t>(::towupper(ch));
        }

        return code.empty() ? L"other" : code;
    }

    struct NodeCounts
    {
        uint32_t Present{ 0 };
        uint32_t NotPresent{ 0 };
    };

    bool IsPresent(_In_ std::wstring const& instanceId)
    {
        DEVINST device{ 0 };

        return ::CM_Locate_DevNodeW(&device, const_cast<DEVINSTID_W>(instanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS;
    }

    // ------------------------------------------------------------------------------------------
    // Firewall

    std::wstring FirewallProfileNames(_In_ long const profiles)
    {
        if ((profiles & NET_FW_PROFILE2_ALL) == NET_FW_PROFILE2_ALL)
        {
            return L"all";
        }

        std::wstring names{};

        for (auto const& [bit, name] : std::initializer_list<std::pair<long, PCWSTR>>{
            { NET_FW_PROFILE2_DOMAIN, L"domain" },
            { NET_FW_PROFILE2_PRIVATE, L"private" },
            { NET_FW_PROFILE2_PUBLIC, L"public" } })
        {
            if ((profiles & bit) != 0)
            {
                names += names.empty() ? L"" : L",";
                names += name;
            }
        }

        return names.empty() ? L"none" : names;
    }

    std::wstring FirewallProtocolName(_In_ long const protocol)
    {
        switch (protocol)
        {
        case NET_FW_IP_PROTOCOL_TCP:    return L"tcp";
        case NET_FW_IP_PROTOCOL_UDP:    return L"udp";
        case NET_FW_IP_PROTOCOL_ANY:    return L"any";
        default:                        return std::to_wstring(protocol);
        }
    }

    std::wstring BstrText(_In_ wil::unique_bstr const& text)
    {
        return text ? std::wstring{ text.get() } : std::wstring{};
    }
}

std::wstring FileVersionString(std::wstring const& path)
{
    DWORD handle{ 0 };
    auto const size = ::GetFileVersionInfoSizeW(path.c_str(), &handle);

    if (size == 0)
    {
        return {};
    }

    std::vector<uint8_t> buffer(size);

    if (!::GetFileVersionInfoW(path.c_str(), 0, size, buffer.data()))
    {
        return {};
    }

    VS_FIXEDFILEINFO* info{ nullptr };
    UINT infoSize{ 0 };

    if (!::VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize) ||
        info == nullptr || infoSize < sizeof(VS_FIXEDFILEINFO))
    {
        return {};
    }

    return std::format(L"{}.{}.{}.{}",
        HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
        HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
}

void CaptureServiceStateBeforeReport()
{
    try
    {
        g_serviceBeforeReport = QueryMidiService(true);

        auto& context = Context();

        context.ServiceInstalled = g_serviceBeforeReport.Installed;
        context.ServiceDisabled = g_serviceBeforeReport.Installed && g_serviceBeforeReport.StartType == SERVICE_DISABLED;
        context.ServiceRunningBeforeReport = g_serviceBeforeReport.Installed && g_serviceBeforeReport.State == SERVICE_RUNNING;
        context.ServiceProcessIdBeforeReport = g_serviceBeforeReport.ProcessId;
    }
    catch (...)
    {
    }
}

bool IsMidiServiceRunning()
{
    return MidiServiceState() == SERVICE_RUNNING;
}

DWORD MidiServiceState()
{
    try
    {
        auto const service = QueryMidiService(false);

        return service.Installed ? service.State : 0;
    }
    catch (...)
    {
        return 0;
    }
}

uint32_t MidiServiceProcessId()
{
    try
    {
        auto const service = QueryMidiService(false);

        return service.Installed && service.State == SERVICE_RUNNING ? service.ProcessId : 0;
    }
    catch (...)
    {
        return 0;
    }
}

ParentDeviceDetails GetParentDeviceDetails(std::wstring const& parentInstanceId, std::wstring const& driverInstanceId)
{
    ParentDeviceDetails details{};

    try
    {
        DEVINST parent{ 0 };

        if (::CM_Locate_DevNodeW(&parent, const_cast<DEVINSTID_W>(parentInstanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
        {
            details.LastArrival = FormatLocalTime(DeviceFileTimeProperty(parent, DEVPKEY_Device_LastArrivalDate));
            details.LastRemoval = FormatLocalTime(DeviceFileTimeProperty(parent, DEVPKEY_Device_LastRemovalDate));

            ULONG status{ 0 };
            ULONG problem{ 0 };

            if (::CM_Get_DevNode_Status(&status, &problem, parent, 0) == CR_SUCCESS && (status & DN_HAS_PROBLEM) != 0)
            {
                details.ProblemCode = problem;
            }

            // PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(3)#USB(2) is one hub down: each USB( is a hop below the root hub
            auto const location = DeviceStringProperty(parent, DEVPKEY_Device_LocationPaths);

            if (auto const root = location.find(L"#USBROOT("); root != std::wstring::npos)
            {
                uint32_t hops{ 0 };

                for (auto position = location.find(L"#USB(", root); position != std::wstring::npos; position = location.find(L"#USB(", position + 1))
                {
                    hops++;
                }

                details.UsbLocationPath = location;
                details.UsbHubCount = hops > 0 ? hops - 1 : 0;
            }
        }

        // the driver date belongs to the node the driver is installed on, which can be a child of the parent
        DEVINST driver{ 0 };
        auto const& driverId = driverInstanceId.empty() ? parentInstanceId : driverInstanceId;

        if (::CM_Locate_DevNodeW(&driver, const_cast<DEVINSTID_W>(driverId.c_str()), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
        {
            auto const date = DeviceFileTimeProperty(driver, DEVPKEY_Device_DriverDate);
            SYSTEMTIME day{};

            // a driver date is a calendar date, so no time zone is applied
            if ((date.dwHighDateTime != 0 || date.dwLowDateTime != 0) && ::FileTimeToSystemTime(&date, &day))
            {
                details.DriverDate = std::format(L"{:04}-{:02}-{:02}", day.wYear, day.wMonth, day.wDay);
            }
        }
    }
    catch (...)
    {
    }

    return details;
}

void OutputOperatingSystemFields()
{
    try
    {
        for (auto const& [label, valueName] : std::initializer_list<std::pair<PCWSTR, PCWSTR>>{
            { MIDIDIAG_FIELD_LABEL_OS_EDITION, L"EditionID" },
            { MIDIDIAG_FIELD_LABEL_OS_DISPLAY_VERSION, L"DisplayVersion" },
            { MIDIDIAG_FIELD_LABEL_OS_BUILD_LAB, L"BuildLabEx" } })
        {
            if (auto const value = TryReadRegistryString(HKEY_LOCAL_MACHINE, CurrentVersionKey, valueName); value.has_value())
            {
                WriteField(label, value.value());
            }
        }

        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};

        if (::LCIDToLocaleName(MAKELCID(::GetUserDefaultUILanguage(), SORT_DEFAULT), localeName, ARRAYSIZE(localeName), 0) > 0)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_OS_UI_LANGUAGE, localeName);
        }

        WriteField(MIDIDIAG_FIELD_LABEL_SYSTEM_UPTIME,
            FormatDuration(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds{ ::GetTickCount64() })));

        // A Fast Startup boot resumes the services and drivers that were running at shutdown
        FILETIME lastBootTime{};
        uint64_t lastBootType{ UINT64_MAX };

        ForEachEvent(L"System",
            L"*[System[Provider[@Name='Microsoft-Windows-Kernel-Boot'] and EventID=27]]",
            { L"Event/System/TimeCreated/@SystemTime", L"Event/EventData/Data[@Name='BootType']" },
            [&lastBootTime, &lastBootType](RenderedEvent const& event)
            {
                if (lastBootType == UINT64_MAX)
                {
                    lastBootTime = event.Time(0);
                    lastBootType = event.Number(1);
                }
            });

        if (lastBootType != UINT64_MAX)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_LAST_BOOT_TIME, FormatLocalTime(lastBootTime));
            WriteField(MIDIDIAG_FIELD_LABEL_LAST_BOOT_TYPE, BootTypeName(lastBootType));
        }
        else
        {
            WriteField(MIDIDIAG_FIELD_LABEL_LAST_BOOT_TYPE, L"Not reported");
        }

        auto const hiberboot = TryReadRegistryDword(HKEY_LOCAL_MACHINE,
            LR"(SYSTEM\CurrentControlSet\Control\Session Manager\Power)", L"HiberbootEnabled");

        WriteField(MIDIDIAG_FIELD_LABEL_FAST_STARTUP_ENABLED,
            hiberboot.has_value() ? (hiberboot.value() != 0 ? L"true" : L"false") : L"Not present");

        if (lastBootType == 1)
        {
            AddFinding(L"fast_startup_boot", internal::ResourceGetWString(IDS_FINDING_FAST_STARTUP_BOOT));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
    }
}

bool DoSectionApiMode()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_API_MODE);

    try
    {
        constexpr DWORD ApiModeFull{ 0 };
        constexpr DWORD ApiModeLegacy{ 1 };
        constexpr DWORD ApiModeHybrid{ 2 };

        auto const modeName = [](DWORD const mode)
            {
                switch (mode)
                {
                case ApiModeLegacy: return L"legacy";
                case ApiModeHybrid: return L"hybrid";
                default:            return L"full";
                }
            };

        // read the way the service and the MIDI 1.0 drivers read it, where anything unusable means full
        auto const registryValue = TryReadRegistryDword(HKEY_LOCAL_MACHINE, MIDI_DRIVERS32_REG_KEY, MIDI_USE_LEGACY_REG_KEY);
        auto const mode = registryValue.has_value() && registryValue.value() <= ApiModeHybrid ? registryValue.value() : ApiModeFull;

        WriteField(MIDIDIAG_FIELD_LABEL_API_MODE, modeName(mode));
        WriteField(MIDIDIAG_FIELD_LABEL_API_MODE_REGISTRY_VALUE,
            registryValue.has_value() ? std::to_wstring(registryValue.value()) : std::wstring{ L"Not present" });

        Context().LegacyApiMode = mode == ApiModeLegacy;

        if (mode == ApiModeLegacy)
        {
            AddFinding(L"api_mode_legacy", internal::ResourceGetWString(IDS_FINDING_API_MODE_LEGACY));
        }
        else if (mode == ApiModeHybrid)
        {
            AddFinding(L"api_mode_hybrid", internal::ResourceGetWString(IDS_FINDING_API_MODE_HYBRID));
        }

        // what an app using the SDK is told. This is the first SDK call, so it fails when the SDK is missing.
        try
        {
            DWORD sdkMode{ ApiModeFull };

            switch (midi2::MidiApi::GetCurrentlySelectedApiMode())
            {
            case midi2::MidiApiMode::LegacyMode:        sdkMode = ApiModeLegacy; break;
            case midi2::MidiApiMode::HybridLegacyMode:  sdkMode = ApiModeHybrid; break;
            default:                                    sdkMode = ApiModeFull; break;
            }

            WriteField(MIDIDIAG_FIELD_LABEL_API_MODE_SDK_REPORTED, modeName(sdkMode));
        }
        catch (...)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_API_MODE_SDK_REPORTED, L"Not available");
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionComponentVersions()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_COMPONENT_VERSIONS);

    try
    {
        auto const outputComponent = [](std::wstring const& name, std::wstring const& path)
            {
                KeyValueText values{};
                values.Add(L"name", name);

                if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
                {
                    values.AddBool(L"present", false);
                }
                else
                {
                    auto const version = FileVersionString(path);
                    values.Add(L"version", version.empty() ? std::wstring{ L"unknown" } : version);
                }

                values.Add(L"path", path);

                WriteField(MIDIDIAG_FIELD_LABEL_COMPONENT, values);
            };

        wchar_t systemFolder[MAX_PATH]{};
        ::GetSystemDirectoryW(systemFolder, ARRAYSIZE(systemFolder));

        auto servicePath = std::wstring{ systemFolder } + L"\\midisrv.exe";

        if (!g_serviceBeforeReport.ImagePath.empty())
        {
            servicePath = ExecutablePathFromImagePath(g_serviceBeforeReport.ImagePath);
        }

        outputComponent(L"midisrv.exe", servicePath);
        outputComponent(L"wdmaud2.drv", std::wstring{ systemFolder } + L"\\wdmaud2.drv");

        // what 32-bit apps load
        wchar_t wow64Folder[MAX_PATH]{};

        if (::GetSystemWow64DirectoryW(wow64Folder, ARRAYSIZE(wow64Folder)) > 0)
        {
            outputComponent(L"wdmaud2.drv", std::wstring{ wow64Folder } + L"\\wdmaud2.drv");
        }

        // Class drivers load from wherever their service says, which is usually the driver store
        for (auto const& [serviceName, fileName] : std::initializer_list<std::pair<PCWSTR, PCWSTR>>{
            { L"USBMidi2", L"USBMidi2.sys" },
            { L"usbaudio", L"usbaudio.sys" } })
        {
            auto const serviceKey = std::wstring{ LR"(SYSTEM\CurrentControlSet\Services\)" } + serviceName;
            auto const imagePath = TryReadRegistryString(HKEY_LOCAL_MACHINE, serviceKey.c_str(), L"ImagePath");

            outputComponent(fileName, imagePath.has_value() ?
                DriverPathFromImagePath(imagePath.value()) :
                std::wstring{ systemFolder } + LR"(\drivers\)" + fileName);
        }

        // the SDK this report loaded, which is not always the one apps load
        for (auto const moduleName : { L"Windows.Devices.Midi2.dll", L"Microsoft.Windows.Devices.Midi2.dll" })
        {
            if (auto const module = ::GetModuleHandleW(moduleName); module != nullptr)
            {
                wchar_t modulePath[MAX_PATH * 2]{};

                if (::GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath)) > 0)
                {
                    outputComponent(moduleName, modulePath);
                }
            }
        }

        wchar_t selfPath[MAX_PATH * 2]{};

        if (::GetModuleFileNameW(nullptr, selfPath, ARRAYSIZE(selfPath)) > 0)
        {
            outputComponent(L"mididiag.exe", selfPath);
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionServiceStatus()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_SERVICE_STATUS);

    try
    {
        auto const& service = g_serviceBeforeReport;
        auto& context = Context();

        if (!service.Queried)
        {
            WriteError(FormatResourceString(IDS_ERROR_CANNOT_QUERY_SERVICE, FormatHResult(HRESULT_FROM_WIN32(service.Error))));
            return false;
        }

        WriteBoolField(MIDIDIAG_FIELD_LABEL_SERVICE_INSTALLED, service.Installed);

        if (!service.Installed)
        {
            AddFinding(L"service_not_installed", internal::ResourceGetWString(IDS_FINDING_SERVICE_NOT_INSTALLED));
            return true;
        }

        WriteField(MIDIDIAG_FIELD_LABEL_SERVICE_STATE_BEFORE_REPORT, ServiceStateName(service.State));
        WriteField(MIDIDIAG_FIELD_LABEL_SERVICE_START_TYPE, ServiceStartTypeName(service));
        WriteNumberField(MIDIDIAG_FIELD_LABEL_SERVICE_TRIGGER_COUNT, service.TriggerCount);
        WriteField(MIDIDIAG_FIELD_LABEL_SERVICE_ACCOUNT, service.Account);
        WriteNumberField(MIDIDIAG_FIELD_LABEL_SERVICE_PROCESS_ID, service.ProcessId);

        if (service.StartType == SERVICE_DISABLED)
        {
            AddFinding(L"service_disabled", internal::ResourceGetWString(IDS_FINDING_SERVICE_DISABLED));
        }
        else if (service.StartType == SERVICE_DEMAND_START && service.TriggerCount == 0)
        {
            AddFinding(L"service_no_start_trigger", internal::ResourceGetWString(IDS_FINDING_SERVICE_NO_START_TRIGGER));
        }

        if (context.LegacyApiMode && service.State == SERVICE_RUNNING)
        {
            AddFinding(L"api_mode_not_applied", internal::ResourceGetWString(IDS_FINDING_API_MODE_NOT_APPLIED));
        }

        try
        {
            for (auto const& process : FindMidiServiceProcesses())
            {
                KeyValueText values{};
                values.AddNumber(L"pid", process.ProcessId);

                auto const startTime = FormatLocalTime(process.StartTime);

                if (!startTime.empty())
                {
                    values.Add(L"start", startTime);
                    values.Add(L"uptime", FormatDuration(SecondsSince(process.StartTime)));
                }

                values.AddNumber(L"threads", process.ThreadCount);
                values.AddNumber(L"handles", process.HandleCount);

                WriteField(MIDIDIAG_FIELD_LABEL_SERVICE_PROCESS, values);

                // a service that will not stop leaves its process behind, still holding whatever it
                // was stuck on. A service that is starting or stopping owns its process, so it is fine.
                if (service.State == SERVICE_STOPPED || (service.ProcessId != 0 && process.ProcessId != service.ProcessId))
                {
                    AddFinding(L"service_leftover_process",
                        FormatResourceString(IDS_FINDING_SERVICE_LEFTOVER_PROCESS, process.ProcessId));
                }
            }
        }
        catch (...)
        {
            WriteError(internal::ResourceGetWString(IDS_ERROR_CANNOT_LIST_SERVICE_PROCESSES));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionServiceHistory()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_SERVICE_HISTORY);

    try
    {
        WriteNumberField(MIDIDIAG_FIELD_LABEL_HISTORY_DAYS, HistoryDays);

        // services stopping on their own, or failing to start
        uint32_t serviceEventCount{ 0 };
        uint32_t midiServiceExitCount{ 0 };
        std::vector<KeyValueText> serviceEvents{};

        auto const serviceQuery = std::format(
            L"*[System[Provider[@Name='Service Control Manager'] and (EventID=7000 or EventID=7009 or EventID=7011 or "
            L"EventID=7022 or EventID=7023 or EventID=7024 or EventID=7031 or EventID=7034) and {}]]",
            HistoryTimeFilter());

        bool const serviceLogRead = ForEachEvent(L"System", serviceQuery,
            { L"Event/System/TimeCreated/@SystemTime", L"Event/System/EventID", L"Event/EventData/Data[@Name='param1']", L"Event/EventData/Binary" },
            [&](RenderedEvent const& event)
            {
                // the key name is the same in every language, the display name in param1 is not
                auto serviceName = event.BinaryText(3);
                bool const isMidiService = serviceName.empty() ?
                    ContainsNoCase(event.String(2), L"midi") :
                    ContainsNoCase(serviceName, L"midi");

                if (!isMidiService)
                {
                    return;
                }

                if (serviceName.empty())
                {
                    serviceName = event.String(2);
                }

                auto const eventId = event.Number(1);

                if ((eventId == 7031 || eventId == 7034) && LowerCopy(serviceName) == MidiServiceName)
                {
                    midiServiceExitCount++;
                }

                serviceEventCount++;

                if (serviceEvents.size() < MaxHistoryEntries)
                {
                    serviceEvents.push_back(KeyValueText{}
                        .Add(L"time", FormatLocalTime(event.Time(0)))
                        .AddNumber(L"id", eventId)
                        .Add(L"service", serviceName));
                }
            });

        if (!serviceLogRead)
        {
            WriteError(FormatResourceString(IDS_ERROR_CANNOT_READ_EVENT_LOG, std::wstring{ L"System" }));
        }

        WriteNumberField(MIDIDIAG_FIELD_LABEL_SERVICE_EVENT_COUNT, serviceEventCount);

        for (auto const& values : serviceEvents)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_SERVICE_EVENT, values);
        }

        if (midiServiceExitCount > 0)
        {
            AddFinding(L"service_crashes", FormatResourceString(IDS_FINDING_SERVICE_CRASHES, midiServiceExitCount, HistoryDays));
        }

        // programs that crashed in the service or in a MIDI component
        uint32_t crashCount{ 0 };
        std::vector<KeyValueText> crashes{};

        auto const crashQuery = std::format(
            L"*[System[Provider[@Name='Application Error'] and EventID=1000 and {}]]", HistoryTimeFilter());

        bool const applicationLogRead = ForEachEvent(L"Application", crashQuery,
            {
                L"Event/System/TimeCreated/@SystemTime",
                L"Event/EventData/Data[@Name='AppName']",
                L"Event/EventData/Data[@Name='AppVersion']",
                L"Event/EventData/Data[@Name='ModuleName']",
                L"Event/EventData/Data[@Name='ModuleVersion']",
                L"Event/EventData/Data[@Name='ExceptionCode']",
                L"Event/EventData/Data[@Name='FaultingOffset']"
            },
            [&](RenderedEvent const& event)
            {
                auto const appName = event.String(1);
                auto const moduleName = event.String(3);

                if (LowerCopy(appName) != L"midisrv.exe" && !IsMidiModule(moduleName))
                {
                    return;
                }

                crashCount++;

                if (crashes.size() < MaxHistoryEntries)
                {
                    crashes.push_back(KeyValueText{}
                        .Add(L"time", FormatLocalTime(event.Time(0)))
                        .Add(L"app", appName)
                        .Add(L"app_version", event.String(2))
                        .Add(L"module", moduleName)
                        .Add(L"module_version", event.String(4))
                        .Add(L"exception", event.Hex(5))
                        .Add(L"offset", event.Hex(6)));
                }
            });

        if (!applicationLogRead)
        {
            WriteError(FormatResourceString(IDS_ERROR_CANNOT_READ_EVENT_LOG, std::wstring{ L"Application" }));
        }

        WriteNumberField(MIDIDIAG_FIELD_LABEL_MIDI_APP_CRASH_COUNT, crashCount);

        for (auto const& values : crashes)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_MIDI_APP_CRASH, values);
        }

        if (crashCount > 0)
        {
            AddFinding(L"midi_app_crashes", FormatResourceString(IDS_FINDING_MIDI_APP_CRASHES, crashCount, HistoryDays));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionDeviceNodes()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_DEVICE_NODES);

    try
    {
        // Devices Windows found but is not using. A USB device whose descriptor could not be
        // read, or that has no driver, never becomes a MIDI endpoint, so nothing else shows it.
        uint32_t problemCount{ 0 };

        for (auto const& instanceId : DeviceIdList(nullptr, CM_GETIDLIST_FILTER_PRESENT))
        {
            DEVINST device{ 0 };

            if (::CM_Locate_DevNodeW(&device, const_cast<DEVINSTID_W>(instanceId.c_str()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
            {
                continue;
            }

            ULONG status{ 0 };
            ULONG problem{ 0 };

            if (::CM_Get_DevNode_Status(&status, &problem, device, 0) != CR_SUCCESS ||
                ((status & DN_HAS_PROBLEM) == 0 && problem == 0))
            {
                continue;
            }

            bool const isMidiSoftwareDevice = StartsWithNoCase(instanceId, L"SWD\\MIDISRV\\") ||
                (StartsWithNoCase(instanceId, L"SWD\\MMDEVAPI\\") && ContainsNoCase(instanceId, L"\\MIDIU_"));

            if (!StartsWithNoCase(instanceId, L"USB\\") && !isMidiSoftwareDevice && !DeviceClassIs(device, MediaDeviceClass))
            {
                continue;
            }

            auto name = DeviceStringProperty(device, DEVPKEY_Device_FriendlyName);

            if (name.empty())
            {
                name = DeviceStringProperty(device, DEVPKEY_Device_DeviceDesc);
            }

            KeyValueText values{};
            values.AddNumber(L"problem", problem);

            DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };
            uint32_t problemStatus{ 0 };
            ULONG size{ sizeof(problemStatus) };

            if (::CM_Get_DevNode_PropertyW(device, &DEVPKEY_Device_ProblemStatus, &type,
                reinterpret_cast<PBYTE>(&problemStatus), &size, 0) == CR_SUCCESS && type == DEVPROP_TYPE_NTSTATUS)
            {
                values.AddHex(L"status", problemStatus, 8);
            }

            values.Add(L"id", instanceId);
            values.Add(L"name", name);

            WriteField(MIDIDIAG_FIELD_LABEL_PROBLEM_DEVICE, values);

            if (++problemCount <= MaxProblemDeviceFindings)
            {
                // a device with no driver often has no name either, so use its hardware id
                auto const shownName = name.empty() ? instanceId.substr(0, instanceId.rfind(L'\\')) : name;

                AddFinding(L"problem_device",
                    FormatResourceString(IDS_FINDING_PROBLEM_DEVICE, shownName, instanceId, problem));
            }
        }

        WriteNumberField(MIDIDIAG_FIELD_LABEL_PROBLEM_DEVICE_COUNT, problemCount);

        // Endpoints the service created before and that are gone now. The service keeps them on
        // purpose, but a large pile of them makes adding a device slow.
        std::map<std::wstring, NodeCounts> endpointsByTransport{};
        NodeCounts endpointTotal{};

        for (auto const& instanceId : DeviceIdList(L"SWD\\MIDISRV", CM_GETIDLIST_FILTER_ENUMERATOR))
        {
            auto& counts = endpointsByTransport[TransportCodeFromInstanceId(instanceId)];
            bool const present = IsPresent(instanceId);

            (present ? counts.Present : counts.NotPresent)++;
            (present ? endpointTotal.Present : endpointTotal.NotPresent)++;
        }

        for (auto const& [transportCode, counts] : endpointsByTransport)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_ENDPOINT_NODES, KeyValueText{}
                .Add(L"transport", transportCode)
                .AddNumber(L"present", counts.Present)
                .AddNumber(L"not_present", counts.NotPresent));
        }

        WriteField(MIDIDIAG_FIELD_LABEL_ENDPOINT_NODES_TOTAL, KeyValueText{}
            .AddNumber(L"present", endpointTotal.Present)
            .AddNumber(L"not_present", endpointTotal.NotPresent));

        NodeCounts portTotal{};

        for (auto const& instanceId : DeviceIdList(L"SWD\\MMDEVAPI", CM_GETIDLIST_FILTER_ENUMERATOR))
        {
            if (ContainsNoCase(instanceId, L"\\MIDIU_"))
            {
                (IsPresent(instanceId) ? portTotal.Present : portTotal.NotPresent)++;
            }
        }

        WriteField(MIDIDIAG_FIELD_LABEL_MIDI1_PORT_NODES_TOTAL, KeyValueText{}
            .AddNumber(L"present", portTotal.Present)
            .AddNumber(L"not_present", portTotal.NotPresent));

        if (endpointTotal.NotPresent > LeftoverEndpointNodeFindingThreshold)
        {
            AddFinding(L"leftover_endpoint_nodes", FormatResourceString(IDS_FINDING_LEFTOVER_NODES, endpointTotal.NotPresent));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionNetwork()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_NETWORK);

    // firewall profile bits for the networks this PC is connected to
    long connectedProfiles{ 0 };

    try
    {
        auto const networkList = wil::CoCreateInstance<NetworkListManager, INetworkListManager>(CLSCTX_ALL);

        wil::com_ptr<IEnumNetworks> networks{};
        THROW_IF_FAILED(networkList->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, networks.put()));

        for (;;)
        {
            wil::com_ptr<INetwork> network{};
            ULONG fetched{ 0 };

            if (networks->Next(1, network.put(), &fetched) != S_OK || fetched == 0)
            {
                break;
            }

            NLM_NETWORK_CATEGORY category{ NLM_NETWORK_CATEGORY_PUBLIC };

            if (FAILED(network->GetCategory(&category)))
            {
                continue;
            }

            std::wstring categoryName{};

            switch (category)
            {
            case NLM_NETWORK_CATEGORY_PRIVATE:
                categoryName = L"private";
                connectedProfiles |= NET_FW_PROFILE2_PRIVATE;
                break;
            case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED:
                categoryName = L"domain";
                connectedProfiles |= NET_FW_PROFILE2_DOMAIN;
                break;
            default:
                categoryName = L"public";
                connectedProfiles |= NET_FW_PROFILE2_PUBLIC;
                break;
            }

            WriteField(MIDIDIAG_FIELD_LABEL_CONNECTED_NETWORK, KeyValueText{}.Add(L"category", categoryName));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_CANNOT_READ_NETWORKS));
    }

    try
    {
        auto const policy = wil::CoCreateInstance<NetFwPolicy2, INetFwPolicy2>(CLSCTX_INPROC_SERVER);

        long enabledProfiles{ 0 };

        for (auto const profile : { NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC })
        {
            VARIANT_BOOL enabled{ VARIANT_FALSE };

            if (SUCCEEDED(policy->get_FirewallEnabled(profile, &enabled)))
            {
                if (enabled != VARIANT_FALSE)
                {
                    enabledProfiles |= profile;
                }

                WriteField(MIDIDIAG_FIELD_LABEL_FIREWALL_PROFILE, KeyValueText{}
                    .Add(L"profile", FirewallProfileNames(profile))
                    .AddBool(L"enabled", enabled != VARIANT_FALSE));
            }
        }

        // profiles where a rule lets midisrv accept incoming UDP
        long allowedProfiles{ 0 };

        wil::com_ptr<INetFwRules> rules{};
        THROW_IF_FAILED(policy->get_Rules(rules.put()));

        wil::com_ptr<IUnknown> enumerator{};
        THROW_IF_FAILED(rules->get__NewEnum(enumerator.put()));

        auto const ruleList = enumerator.query<IEnumVARIANT>();

        for (;;)
        {
            wil::unique_variant item{};
            ULONG fetched{ 0 };

            if (ruleList->Next(1, item.addressof(), &fetched) != S_OK || fetched == 0)
            {
                break;
            }

            if (item.vt != VT_DISPATCH || item.pdispVal == nullptr)
            {
                continue;
            }

            auto const rule = wil::try_com_query<INetFwRule>(item.pdispVal);

            if (!rule)
            {
                continue;
            }

            wil::unique_bstr applicationName{};
            wil::unique_bstr serviceName{};
            rule->get_ApplicationName(applicationName.put());
            rule->get_ServiceName(serviceName.put());

            auto const application = BstrText(applicationName);

            if (!EndsWithNoCase(application, L"\\midisrv.exe") && LowerCopy(application) != L"midisrv.exe" &&
                LowerCopy(BstrText(serviceName)) != MidiServiceName)
            {
                continue;
            }

            NET_FW_RULE_DIRECTION direction{ NET_FW_RULE_DIR_IN };
            NET_FW_ACTION action{ NET_FW_ACTION_BLOCK };
            VARIANT_BOOL enabled{ VARIANT_FALSE };
            long profiles{ 0 };
            long protocol{ 0 };
            wil::unique_bstr localPorts{};
            wil::unique_bstr name{};

            rule->get_Direction(&direction);
            rule->get_Action(&action);
            rule->get_Enabled(&enabled);
            rule->get_Profiles(&profiles);
            rule->get_Protocol(&protocol);
            rule->get_LocalPorts(localPorts.put());
            rule->get_Name(name.put());

            WriteField(MIDIDIAG_FIELD_LABEL_FIREWALL_RULE, KeyValueText{}
                .Add(L"direction", direction == NET_FW_RULE_DIR_IN ? L"in" : L"out")
                .Add(L"action", action == NET_FW_ACTION_ALLOW ? L"allow" : L"block")
                .AddBool(L"enabled", enabled != VARIANT_FALSE)
                .Add(L"profiles", FirewallProfileNames(profiles))
                .Add(L"protocol", FirewallProtocolName(protocol))
                .Add(L"local_ports", BstrText(localPorts))
                .Add(L"name", BstrText(name)));

            if (direction == NET_FW_RULE_DIR_IN && action == NET_FW_ACTION_ALLOW && enabled != VARIANT_FALSE &&
                (protocol == NET_FW_IP_PROTOCOL_UDP || protocol == NET_FW_IP_PROTOCOL_ANY))
            {
                allowedProfiles |= profiles;
            }
        }

        // a host is only blocked where the firewall is on and nothing lets the service through
        auto& context = Context();
        context.FirewallStateKnown = connectedProfiles != 0;
        context.AnyConnectedNetworkBlocksMidiService = (connectedProfiles & enabledProfiles & ~allowedProfiles) != 0;
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_CANNOT_READ_FIREWALL));
    }

    return true;
}
