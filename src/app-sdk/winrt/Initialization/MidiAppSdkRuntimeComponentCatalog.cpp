// Copyright (c) Microsoft Corporation and Contributors
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================
// This file modified from
// https://github.com/microsoft/xlang/tree/master/src/UndockedRegFreeWinRT
// which retains its own copyrights as described in the xlang repo

#include "pch.h"

#include "MidiAppSdkRuntimeComponentCatalog.h"



static const UINT32 g_uiMaxTypeName = 512;

// created and initialized by the MidiClientInitializer
std::shared_ptr<MidiAppSdkRuntimeComponentCatalog> g_runtimeComponentCatalog{ nullptr };


// this exists so we don't need to ship a manifest or require apps to have a manifest.
// yes, it requires manually keeping in sync with the winmd and implementation types
// 
// 
// TODO: We could include args here for things like contract version and if we should 
// include experimental types, to allow multiple versions of the DLL to ship to Windows 
// customers.
//

std::vector<MidiAppSdkManifestEntry>
MidiAppSdkRuntimeComponentCatalog::GetMidiAppSdkManifestTypes()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::vector<MidiAppSdkManifestEntry> types{};

    //const std::wstring dllName{ MIDI_SDK_IMPL_DLL_NAME };
    const std::wstring rootNS{ MIDI_SDK_ROOT_NAMESPACE };
    const auto defaultThreading{ ABI::Windows::Foundation::ThreadingType::ThreadingType_BOTH };

 //   types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".IMidiClockStatics", defaultThreading });


    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiChannel", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiGroup", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiClock", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiUniversalSystemExclusiveChannel", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointDeviceInformation", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointAssociatedPortDeviceInformation", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointDeviceWatcher", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointDevicePropertyHelper", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointDeviceIdHelper", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiFunctionBlock", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage32", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage64", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage96", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiMessage128", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointConnectionBasicSettings", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiEndpointConnection", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".MidiSession", defaultThreading });

  

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".CapabilityInquiry.MidiUniqueId", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ClientPlugins.MidiChannelEndpointListener", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ClientPlugins.MidiGroupEndpointListener", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ClientPlugins.MidiMessageTypeEndpointListener", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Diagnostics.MidiDiagnostics", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Reporting.MidiReporting", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiMessageBuilder", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiMessageConverter", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiMessageHelper", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiStreamMessageBuilder", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Messages.MidiUniversalSystemExclusiveMessageBuilder", defaultThreading });


    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ServiceConfig.MidiServiceConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ServiceConfig.MidiServiceTransportPluginConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ServiceConfig.MidiServiceEndpointCustomizationConfig", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ServiceConfig.MidiServiceConfigEndpointMatchCriteria", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".ServiceConfig.IMidiServiceConfigEndpointMatchCriteriaStatics", defaultThreading });


    // Loopback MIDI
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointCreationConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointRemovalConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointManager", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointRemovalConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Loopback.MidiLoopbackEndpointManager", defaultThreading });

    // App-to-app MIDI (Virtual Device)
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Virtual.MidiVirtualDeviceCreationConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Virtual.MidiVirtualDeviceManager", defaultThreading });

    // Network MIDI 2.0
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Network.MidiNetworkTransportManager", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Network.MidiNetworkHostCreationConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Network.MidiNetworkHostRemovalConfig", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Network.MidiNetworkClientEndpointCreationConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Network.MidiNetworkClientEndpointRemovalConfig", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Endpoints.Network.MidiNetworkClientMatchCriteria", defaultThreading });


    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.Update.MidiRuntimeUpdateUtility", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.SysExTransfer.MidiSystemExclusiveMessageHelper", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.SysExTransfer.MidiSystemExclusiveSender", defaultThreading });

    //types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.RuntimeInformation.MidiRuntimeVersion", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.RuntimeInformation.MidiRuntimeInformation", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.Sequencer.MidiClockGenerator", defaultThreading });
    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.Sequencer.MidiClockDestination", defaultThreading });

    types.emplace_back(MidiAppSdkManifestEntry{ rootNS + L".Utilities.Metadata.MidiImageAssetHelper", defaultThreading });


    


    return types;
}



HRESULT
MidiAppSdkRuntimeComponentCatalog::Initialize()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    const std::optional<std::wstring> sdkLocation = wil::reg::try_get_value_string(HKEY_LOCAL_MACHINE, MIDI_ROOT_APP_SDK_REG_KEY, MIDI_APP_SDK_INSTALLED_REG_VALUE);

    if (!sdkLocation.has_value())
    {
        RETURN_IF_FAILED(E_FAIL);
    }

    m_sdkDirectory = sdkLocation.value();

    // TODO: Check to see if files we need exist in this location? May be overkill.


    // TODO: Do we harden and do any validation here? Check for path size or anything?
    m_sdkMetadataFullFilename = m_sdkDirectory + L"\\" + std::wstring{ MIDI_SDK_METADATA_NAME };
    m_sdkImplementationFullFilename = m_sdkDirectory + L"\\" + std::wstring{ MIDI_SDK_IMPL_DLL_NAME };

    m_types.clear();

    auto types = GetMidiAppSdkManifestTypes();

    for (auto const& t : types)
    {
        auto component = std::make_shared<MidiAppSdkRuntimeComponent>();

        component->module_name = GetSdkImplementationFullFilename();
        component->threading_model = t.ThreadingModel;

        m_types[t.ActivatableClassName] = component;
    }

    return S_OK;
}

HRESULT
MidiAppSdkRuntimeComponentCatalog::Shutdown()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
bool 
MidiAppSdkRuntimeComponentCatalog::TypeIsInScope(HSTRING const typeOrNamespace) const
{
    //TraceLoggingWrite(
    //    Midi2SdkTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingWideString(WindowsGetStringRawBuffer(typeOrNamespace, NULL), "type or namespace")
    //);

    if (typeOrNamespace != NULL)
    {
        UINT32 typeLength{ 0 };
        auto typeOrNamespaceBuffer = WindowsGetStringRawBuffer(typeOrNamespace, &typeLength);

        // fail fast if the type name is not at least as long as the root namespace
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
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Type is in scope", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(typeOrNamespaceBuffer, "type or namespace")
                );

                return true;
            }
        }
    }

    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Type is NOT in scope", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(typeOrNamespace, NULL), "type or namespace")
    );


    return false;
}


_Use_decl_annotations_
HRESULT 
MidiAppSdkRuntimeComponentCatalog::GetThreadingModel(
    HSTRING activatableClassId, 
    ABI::Windows::Foundation::ThreadingType* threading_model
) const
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(activatableClassId, nullptr), "class id")
    );

    auto raw_class_name = WindowsGetStringRawBuffer(activatableClassId, nullptr);
    auto component_iter = m_types.find(raw_class_name);

    if (component_iter != m_types.end())
    {
        *threading_model = component_iter->second->threading_model;
        return S_OK;
    }

    auto errorMessage = std::wstring{ L"MIDI App SDK: MIDI Component Catalog REGDB_E_CLASSNOTREG: " } + std::wstring{ raw_class_name } + L"\n";
    OutputDebugString(errorMessage.c_str());
    return REGDB_E_CLASSNOTREG;
}


_Use_decl_annotations_
HRESULT 
MidiAppSdkRuntimeComponentCatalog::GetActivationFactory(
    HSTRING activatableClassId,
    REFIID  iid,
    void** factory
) const
{
    auto raw_class_name = WindowsGetStringRawBuffer(activatableClassId, nullptr);

    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(raw_class_name, "class id")
    );

    auto component_iter = m_types.find(raw_class_name);
    if (component_iter != m_types.end())
    {
        return component_iter->second->GetActivationFactory(activatableClassId, iid, factory);
    }

    return REGDB_E_CLASSNOTREG;
}

// we assume by the point this has been called that the type
// has been validated to be a MIDI SDK type
_Use_decl_annotations_
HRESULT
MidiAppSdkRuntimeComponentCatalog::GetMetadataFile(
    const HSTRING name,
    IMetaDataDispenserEx* metaDataDispenser,
    HSTRING* metaDataFilePath,
    IMetaDataImport2** metaDataImport,
    mdTypeDef* typeDefToken) const
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(name, nullptr), "name")
    );

    wchar_t folderPrefix[9];
    PCWSTR pszFullName = WindowsGetStringRawBuffer(name, nullptr);
    HRESULT hr = StringCchCopyW(folderPrefix, _countof(folderPrefix), pszFullName);
    if (hr != S_OK && hr != STRSAFE_E_INSUFFICIENT_BUFFER)
    {
        return hr;
    }
    if (CompareStringOrdinal(folderPrefix, -1, L"Windows.", -1, false) == CSTR_EQUAL)
    {
        return RO_E_METADATA_NAME_NOT_FOUND;
    }

    if (metaDataFilePath != nullptr) *metaDataFilePath = nullptr;
    if (metaDataImport != nullptr) *metaDataImport = nullptr;
    if (typeDefToken != nullptr) *typeDefToken = mdTypeDefNil;

    if (((metaDataImport == nullptr) && (typeDefToken != nullptr)) ||
        ((metaDataImport != nullptr) && (typeDefToken == nullptr)))
    {
        return E_INVALIDARG;
    }

//    wil::com_ptr<IMetaDataDispenserEx> spMetaDataDispenser;
    // The API uses the caller's passed-in metadata dispenser. If null, it
    // will create an instance of the metadata reader to dispense metadata files.

    if (metaDataDispenser == nullptr)
    {
        //RETURN_IF_FAILED(MetaDataGetDispenser(
        //    CLSID_CorMetaDataDispenser,
        //    IID_IMetaDataDispenser,
        //    (LPVOID*)&spMetaDataDispenser));
        //{
        //    variant_t version{ L"WindowsRuntime 1.4" };
        //    RETURN_IF_FAILED(spMetaDataDispenser->SetOption(MetaDataRuntimeVersion, &version.GetVARIANT()));
        //}
    }

    // TODO: Seems this *always* uses the newly created metadata dispenser instead of using it only when the existing is nullptr

    return FindTypeInMetaDataFile(
        metaDataDispenser /*spMetaDataDispenser.get()*/,
        pszFullName,
        m_sdkMetadataFullFilename.c_str(),
        TYPE_RESOLUTION_OPTIONS::TRO_RESOLVE_TYPE_AND_NAMESPACE,
        metaDataImport,
        typeDefToken
    );

}

_Use_decl_annotations_
HRESULT 
MidiAppSdkRuntimeComponentCatalog::FindTypeInMetaDataFile(
    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
    _In_ PCWSTR pszFullName,
    _In_ PCWSTR pszCandidateFilePath,
    _In_ TYPE_RESOLUTION_OPTIONS resolutionOptions,
    _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
    _Out_opt_ mdTypeDef* pmdTypeDef) const
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(pszFullName, "full name"),
        TraceLoggingWideString(pszCandidateFilePath, "candidate file path")
        );

    HRESULT hr = S_OK;
    wil::com_ptr<IMetaDataImport2> spMetaDataImport;
    MetaDataImportersLRUCache* pMetaDataImporterCache = MetaDataImportersLRUCache::GetMetaDataImportersLRUCacheInstance();
    if (pMetaDataImporterCache != nullptr)
    {
        hr = pMetaDataImporterCache->GetMetaDataImporter(
            pMetaDataDispenser,
            pszCandidateFilePath,
            &spMetaDataImport);
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        const size_t cFullName = wcslen(pszFullName);
        wchar_t pszRetrievedName[g_uiMaxTypeName];
        HCORENUM hEnum = nullptr;
        mdTypeDef rgTypeDefs[32];
        ULONG cTypeDefs;
        DWORD dwTypeDefProps;
        hr = RO_E_METADATA_NAME_NOT_FOUND;

        if (TRO_RESOLVE_TYPE & resolutionOptions)
        {
            hr = spMetaDataImport->FindTypeDefByName(pszFullName, mdTokenNil, &rgTypeDefs[0]);
            if (SUCCEEDED(hr))
            {
                //  Check to confirm that the type we just found is a
                //  winrt type.  If it is, we're good, otherwise we
                //  want to fail with RO_E_INVALID_METADATA_FILE.
                hr = spMetaDataImport->GetTypeDefProps(rgTypeDefs[0], nullptr, 0, nullptr, &dwTypeDefProps, nullptr);
                if (SUCCEEDED(hr))
                {
                    //  If we found the type but it's not a winrt type,
                    //  it's an error.
                    //
                    //  If the type is public, than the metadata file
                    //  is corrupt (all public types in a winrt file
                    //  must be tdWindowsRuntime).  If the type is
                    //  private, then we just want to report that the
                    //  type wasn't found.
                    if (!IsTdWindowsRuntime(dwTypeDefProps))
                    {
                        if (IsTdPublic(dwTypeDefProps))
                        {
                            hr = RO_E_INVALID_METADATA_FILE;
                        }
                        else
                        {
                            hr = RO_E_METADATA_NAME_NOT_FOUND;
                        }
                    }
                }
                else
                {
                    hr = RO_E_INVALID_METADATA_FILE;
                }
                if (SUCCEEDED(hr))
                {
                    if (pmdTypeDef != nullptr)
                    {
                        *pmdTypeDef = rgTypeDefs[0];
                    }
                    if (ppMetaDataImport != nullptr)
                    {
                        *ppMetaDataImport = spMetaDataImport.detach();
                    }
                }
            }
            else if (hr == CLDB_E_RECORD_NOTFOUND)
            {
                hr = RO_E_METADATA_NAME_NOT_FOUND;
            }
        }

        if ((hr == RO_E_METADATA_NAME_NOT_FOUND) &&
            (TRO_RESOLVE_NAMESPACE & resolutionOptions))
        {
            // Check whether the name is a namespace rather than a type.
            do
            {
                hr = spMetaDataImport->EnumTypeDefs(
                    &hEnum,
                    rgTypeDefs,
                    ARRAYSIZE(rgTypeDefs),
                    &cTypeDefs);

                if (hr == S_OK)
                {
                    for (UINT32 iTokenIndex = 0; iTokenIndex < cTypeDefs; ++iTokenIndex)
                    {
                        hr = spMetaDataImport->GetTypeDefProps(
                            rgTypeDefs[iTokenIndex],
                            pszRetrievedName,
                            ARRAYSIZE(pszRetrievedName),
                            nullptr,
                            &dwTypeDefProps,
                            nullptr);

                        if (FAILED(hr))
                        {
                            break;
                        }

                        hr = RO_E_METADATA_NAME_NOT_FOUND;

                        // Only consider windows runtime types when
                        // trying to determine if the name is a
                        // namespace.
                        if (IsTdWindowsRuntime(dwTypeDefProps) && (wcslen(pszRetrievedName) > cFullName))
                        {
                            if ((wcsncmp(pszRetrievedName, pszFullName, cFullName) == 0) &&
                                (pszRetrievedName[cFullName] == L'.'))
                            {
                                hr = RO_E_METADATA_NAME_IS_NAMESPACE;
                                break;
                            }
                        }
                    }
                }
            } while (hr == RO_E_METADATA_NAME_NOT_FOUND);

            // There were no more tokens to enumerate, but the type was still not found.
            if (hr == S_FALSE)
            {
                hr = RO_E_METADATA_NAME_NOT_FOUND;
            }

            if (hEnum != nullptr)
            {
                spMetaDataImport->CloseEnum(hEnum);
                hEnum = nullptr;
            }
        }
    }
    return hr;
}


