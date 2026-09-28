// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Loads the Windows MIDI Services SDK for the service commands.
//
// The SDK is not registered on a development PC, so its classes are activated straight from its
// DLL. The RTP-MIDI classes are only in a local SDK build until the SDK ships with them.
// ============================================================================

#include "spike_common.h"

#include <roapi.h>
#include <winstring.h>

#undef GetObject
#include <winrt/Windows.Foundation.h>

#pragma comment(lib, "runtimeobject.lib")

namespace
{
    using GetActivationFactory = HRESULT(STDAPICALLTYPE*)(HSTRING, void**);

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

    // C++/WinRT calls this instead of RoGetActivationFactory once it is set. A registered copy of
    // the SDK wins, and anything it does not have comes from the copy loaded here.
    int32_t __stdcall ActivationHandler(void* classId, winrt::guid const& iid, void** factory) noexcept
    {
        *factory = nullptr;

        auto const className = WindowsGetStringRawBuffer(static_cast<HSTRING>(classId), nullptr);
        auto const hr = RoGetActivationFactory(static_cast<HSTRING>(classId), reinterpret_cast<GUID const&>(iid), factory);

        if (FAILED(hr) && g_getMidi2ActivationFactory != nullptr && StartsWith(className, L"Windows.Devices.Midi2."))
        {
            return FromLibrary(g_getMidi2ActivationFactory, classId, iid, factory);
        }

        return hr;
    }
}

// Loads Windows.Devices.Midi2.dll by full path. A copy next to this exe wins, then the local SDK
// build, which has the RTP-MIDI classes, then the copy the installed tools use.
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
        exeDirectory + L"..\\..\\..\\..\\..\\in-box\\vsfiles-sdk\\out\\Windows.Devices.Midi2\\x64\\Release\\Windows.Devices.Midi2.dll",
        std::wstring{ programFiles } + L"\\Windows MIDI Services\\Tools\\Console\\Windows.Devices.Midi2.dll",
    };

    for (auto const& candidate : candidates)
    {
        if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) continue;

        auto const module = LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (module == nullptr) continue;

        g_getMidi2ActivationFactory = reinterpret_cast<GetActivationFactory>(GetProcAddress(module, "DllGetActivationFactory"));

        if (g_getMidi2ActivationFactory != nullptr)
        {
            winrt_activation_handler = ActivationHandler;
            return candidate;
        }
    }

    return {};
}
