// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Checks the rtpMIDI SDK against the transport without the MIDI service.
//
// The SDK's configuration objects are activated straight from its DLL, since nothing registers
// it, and the JSON they produce is handed to the transport the way the service would.
// ============================================================================

#include "spike_common.h"

#include <roapi.h>
#include <winstring.h>

#undef GetObject
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.Rtp.h>

#pragma comment(lib, "runtimeobject.lib")

namespace rtp = winrt::Windows::Devices::Midi2::Transports::Rtp;
namespace json = winrt::Windows::Data::Json;

namespace
{
    using GetActivationFactory = HRESULT(STDAPICALLTYPE*)(HSTRING, void**);

    GetActivationFactory g_getActivationFactory{ nullptr };
    GetActivationFactory g_getMidi2ActivationFactory{ nullptr };

    bool StartsWith(wchar_t const* text, std::wstring_view const prefix)
    {
        return text != nullptr && std::wstring_view{ text }.substr(0, prefix.size()) == prefix;
    }

    HRESULT FromLibrary(GetActivationFactory const getFactory, void* classId, winrt::guid const& iid, void** factory)
    {
        IUnknown* activationFactory{ nullptr };

        auto const hr = getFactory(static_cast<HSTRING>(classId), reinterpret_cast<void**>(&activationFactory));
        if (FAILED(hr) || activationFactory == nullptr) return FAILED(hr) ? hr : E_NOINTERFACE;

        auto const result = activationFactory->QueryInterface(reinterpret_cast<GUID const&>(iid), factory);
        activationFactory->Release();

        return result;
    }

    // C++/WinRT calls this instead of RoGetActivationFactory once it is set. The rtpMIDI SDK is
    // never registered, and the Windows MIDI Services SDK may be an app-local copy, which is how
    // the installed tools use it.
    int32_t __stdcall ActivationHandler(void* classId, winrt::guid const& iid, void** factory) noexcept
    {
        *factory = nullptr;

        auto const className = WindowsGetStringRawBuffer(static_cast<HSTRING>(classId), nullptr);

        if (g_getActivationFactory != nullptr && StartsWith(className, L"Windows.Devices.Midi2.Transports.Rtp."))
        {
            return FromLibrary(g_getActivationFactory, classId, iid, factory);
        }

        auto const hr = RoGetActivationFactory(static_cast<HSTRING>(classId), reinterpret_cast<GUID const&>(iid), factory);

        if (FAILED(hr) && g_getMidi2ActivationFactory != nullptr && StartsWith(className, L"Windows.Devices.Midi2."))
        {
            return FromLibrary(g_getMidi2ActivationFactory, classId, iid, factory);
        }

        return hr;
    }

    // The service hands a transport only its own section, keyed by transport id in the wrapper
    std::wstring TransportSection(json::JsonObject const& wrapped)
    {
        if (wrapped == nullptr || !wrapped.HasKey(L"endpointTransportPluginSettings")) return {};

        auto const transports = wrapped.GetNamedObject(L"endpointTransportPluginSettings");

        for (auto const& pair : transports)
        {
            if (_wcsicmp(pair.Key().c_str(), L"{54C9B2F6-C235-4000-A675-9F6958A1A4FA}") == 0 && pair.Value().ValueType() == json::JsonValueType::Object)
            {
                return std::wstring{ pair.Value().GetObject().Stringify() };
            }
        }

        return {};
    }
}

bool SdkCheckStart(std::wstring const& dllPath)
{
    auto const module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr) return false;

    g_getActivationFactory = reinterpret_cast<GetActivationFactory>(GetProcAddress(module, "DllGetActivationFactory"));
    if (g_getActivationFactory == nullptr) return false;

    winrt_activation_handler = ActivationHandler;
    return true;
}

// Loads Windows.Devices.Midi2.dll by full path. Once it is loaded, C++/WinRT's own fallback in
// the rtpMIDI SDK finds it by name as well. A copy next to this exe wins, then the one the
// installed tools use, which matches the installed service, then the local SDK build.
std::wstring SdkLoadMidi2Runtime()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, ARRAYSIZE(exePath));

    std::wstring const exeDirectory = std::wstring{ exePath }.substr(0, std::wstring{ exePath }.find_last_of(L'\\') + 1);

    wchar_t programFiles[MAX_PATH]{};
    GetEnvironmentVariableW(L"ProgramFiles", programFiles, ARRAYSIZE(programFiles));

    std::vector<std::wstring> const candidates
    {
        exeDirectory + L"Windows.Devices.Midi2.dll",
        std::wstring{ programFiles } + L"\\Windows MIDI Services\\Tools\\Console\\Windows.Devices.Midi2.dll",
        exeDirectory + L"..\\..\\..\\..\\..\\in-box\\vsfiles-sdk\\out\\Windows.Devices.Midi2\\x64\\Release\\Windows.Devices.Midi2.dll",
    };

    for (auto const& candidate : candidates)
    {
        if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) continue;

        auto const module = LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (module == nullptr) continue;

        g_getMidi2ActivationFactory = reinterpret_cast<GetActivationFactory>(GetProcAddress(module, "DllGetActivationFactory"));
        if (g_getMidi2ActivationFactory != nullptr) return candidate;
    }

    return {};
}

bool SdkStaticsAreRight()
{
    return rtp::MidiRtpTransportManager::TransportId() == winrt::guid{ L"54c9b2f6-c235-4000-a675-9f6958a1a4fa" } &&
        rtp::MidiRtpTransportManager::DnsSdServiceType() == L"_apple-midi._udp" &&
        rtp::MidiRtpTransportManager::DefaultHostPort() == 5004;
}

// A host config as an app would fill it in, as the transport section the service would pass on
std::wstring SdkHostSection(std::wstring const& name, std::wstring& hostId)
{
    rtp::MidiRtpHostConfig config;
    config.Name(name);
    config.UseAutomaticPort(true);
    config.Advertise(false);

    hostId = winrt::to_hstring(config.HostId());
    return TransportSection(config.ConfigJson());
}

std::wstring SdkClientSection(std::wstring const& name, std::wstring const& address, uint16_t port, std::wstring const& customEndpointName, std::wstring& clientId)
{
    rtp::MidiRtpClientConfig config;
    config.Name(name);
    config.RemoteAddress(address);
    config.RemotePort(port);
    config.CustomEndpointName(customEndpointName);

    clientId = winrt::to_hstring(config.ClientId());
    return TransportSection(config.ConfigJson());
}

std::wstring SdkRemovalSection(std::wstring const& entryId, bool isHost)
{
    rtp::MidiRtpEntryRemovalConfig config(winrt::guid{ entryId }, isHost);
    return TransportSection(config.ConfigJson());
}
