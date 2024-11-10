// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#define MIDI_SDK_ROOT_NAMESPACE     L"Microsoft.Windows.Devices.Midi2"
#define MIDI_SDK_ROOT_NAMESPACE_LENGTH 31

#define MIDI_SDK_IMPL_DLL_NAME      L"Microsoft.Windows.Devices.Midi2.dll"
#define MIDI_SDK_METADATA_NAME      L"Microsoft.Windows.Devices.Midi2.winmd"


struct MidiAppSdkManifestEntry
{
    std::wstring ActivatableClassName{};
    std::wstring FileName{};
    ABI::Windows::Foundation::ThreadingType ThreadingModel{ ABI::Windows::Foundation::ThreadingType::ThreadingType_BOTH };
};

inline std::optional<std::wstring> GetMidiSdkPath()
{
#if defined(_M_ARM64EC) || defined(_M_ARM64)
    // Arm64 and Arm64EC use same Arm64X binaries
    const std::optional<std::wstring> sdkLocation = wil::reg::try_get_value_string(HKEY_LOCAL_MACHINE, MIDI_ROOT_ENDPOINT_APP_SDK_REG_KEY, MIDI_APP_SDK_ARM64X_REG_VALUE);
#elif defined(_M_AMD64)
    // other 64 bit Intel/AMD
    const std::optional<std::wstring> sdkLocation = wil::reg::try_get_value_string(HKEY_LOCAL_MACHINE, MIDI_ROOT_ENDPOINT_APP_SDK_REG_KEY, MIDI_APP_SDK_X64_REG_VALUE);
#else
    // unsupported compile target architecture
#endif
    return sdkLocation;
}

inline bool ClassOrNamespaceIsInWindowsMidiServicesNamespace(HSTRING const typeOrNamespace)
{
    if (typeOrNamespace != NULL)
    {
        UINT32 typeLength{ 0 };
        auto typeOrNamespaceBuffer = WindowsGetStringRawBuffer(typeOrNamespace, &typeLength);

        if (typeLength >= MIDI_SDK_ROOT_NAMESPACE_LENGTH)
        {
            // we compare only up to the length of the namespace, so we use that
            // as the length for both strings (note we already checked to ensure
            // that the passed-in type name is as long or longer)
            if (CompareStringOrdinal(
                typeOrNamespaceBuffer, MIDI_SDK_ROOT_NAMESPACE_LENGTH, 
                MIDI_SDK_ROOT_NAMESPACE, MIDI_SDK_ROOT_NAMESPACE_LENGTH, 
                false) == CSTR_EQUAL)
            {
                return true;
            }
        }
    }

    return false;
}

inline bool IsSdkInstalled()
{
    return GetMidiSdkPath().has_value();
}

inline std::wstring GetMidiSdkMetadataFullFileName()
{
    auto path = GetMidiSdkPath();

    if (path.has_value())
    {
        return path.value() + L"\\" + MIDI_SDK_METADATA_NAME;
    }
    else
    {
        return L"";
    }
}

inline std::wstring GetMidiSdkImplementationFullFileName()
{
    auto path = GetMidiSdkPath();

    if (path.has_value())
    {
        return path.value() + L"\\" + MIDI_SDK_IMPL_DLL_NAME;
    }
    else
    {
        return L"";
    }
}


// this exists so we don't need to ship a manifest or require apps to have a manifest
// yes, it requires manually keeping in sync with the winmd and implementation types
// 
// TODO: We could include args here for things like contract and if we should include
// experimental, to allow multiple versions of the DLL to ship to Windows customers.
//
inline std::vector<MidiAppSdkManifestEntry> GetMidiAppSdkManifestTypes()
{
    std::vector<MidiAppSdkManifestEntry> types{};

    const std::wstring dllName{ MIDI_SDK_IMPL_DLL_NAME };
    const std::wstring rootNS{ MIDI_SDK_ROOT_NAMESPACE };
    const auto defaultThreading{ ABI::Windows::Foundation::ThreadingType::ThreadingType_BOTH };

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiChannel", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiGroup", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiClock", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointDeviceInformation", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointDeviceWatcher", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiFunctionBlock", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage32", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage64", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage96", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage128", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointConnection", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiSession", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".CapabilityInquiry.MidiUniqueId", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ClientPlugins.MidiChannelEndpointListener", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ClientPlugins.MidiGroupEndpointListener", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ClientPlugins.MidiMessageTypeEndpointListener", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Diagnostics.MidiDiagnostics", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Reporting.MidiReporting", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiMessageBuilder", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiMessageConverter", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiMessageHelper", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiStreamMessageBuilder", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ServiceConfig.MidiServiceConfig", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointCreationConfig", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointRemovalConfig", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointManager", dllName, defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Virtual.MidiVirtualDeviceCreationConfig", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Virtual.MidiVirtualDeviceManager", dllName, defaultThreading });

    // todo: add network

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.SysExTransfer.MidiSystemExclusiveMessageHelper", dllName, defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.SysExTransfer.MidiSystemExclusiveSender", dllName, defaultThreading });

    return types;
}
