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


static const UINT32 g_uiMaxTypeName = 512;


// this exists so we don't need to ship a manifest or require apps to have a manifest
// yes, it requires manually keeping in sync with the winmd and implementation types
// 
// TODO: We could include args here for things like contract and if we should include
// experimental, to allow multiple versions of the DLL to ship to Windows customers.
//

std::vector<MidiAppSdkManifestEntry>
MidiSdkComponentCatalog::GetMidiAppSdkManifestTypes()
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



HRESULT
MidiSdkComponentCatalog::Initialize()
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

    if (!sdkLocation.has_value())
    {
        return E_FAIL;
    }

    m_sdkDirectory = sdkLocation.value();

    m_sdkMetadataFullFilename = m_sdkDirectory + L"\\" + std::wstring{ MIDI_SDK_METADATA_NAME };
    m_sdkImplementationFullFilename = m_sdkDirectory + L"\\" + std::wstring{ MIDI_SDK_IMPL_DLL_NAME };


    auto types = GetMidiAppSdkManifestTypes();

    for (auto const& t : types)
    {
        auto component = std::make_shared<MidiAppSdkRuntimeComponent>();

        component->module_name = t.FileName;
        component->threading_model = t.ThreadingModel;

        m_types[t.ActivatableClassName] = component;
    }

    return S_OK;
}

HRESULT
MidiSdkComponentCatalog::Shutdown()
{

}


_Use_decl_annotations_
bool 
MidiSdkComponentCatalog::TypeIsInScope(HSTRING const typeOrNamespace)
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


_Use_decl_annotations_
HRESULT 
MidiSdkComponentCatalog::GetThreadingModel(
    HSTRING activatableClassId, 
    ABI::Windows::Foundation::ThreadingType* threading_model
)
{
    auto raw_class_name = WindowsGetStringRawBuffer(activatableClassId, nullptr);
    auto component_iter = m_types.find(raw_class_name);

    if (component_iter != m_types.end())
    {
        *threading_model = component_iter->second->threading_model;
        return S_OK;
    }

    return REGDB_E_CLASSNOTREG;
}


_Use_decl_annotations_
HRESULT 
MidiSdkComponentCatalog::GetActivationFactory(
    HSTRING activatableClassId,
    REFIID  iid,
    void** factory
)
{
    auto raw_class_name = WindowsGetStringRawBuffer(activatableClassId, nullptr);
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
MidiSdkComponentCatalog::GetMetadataFile(
    const HSTRING name,
    IMetaDataDispenserEx* metaDataDispenser,
    HSTRING* metaDataFilePath,
    IMetaDataImport2** metaDataImport,
    mdTypeDef* typeDefToken)
{
    if (metaDataFilePath != nullptr) *metaDataFilePath = nullptr;
    if (metaDataImport != nullptr) *metaDataImport = nullptr;
    if (typeDefToken != nullptr) *typeDefToken = mdTypeDefNil;

    if (((metaDataImport == nullptr) && (typeDefToken != nullptr)) ||
        ((metaDataImport != nullptr) && (typeDefToken == nullptr)))
    {
        return E_INVALIDARG;
    }

    PCWSTR pszFullName = WindowsGetStringRawBuffer(name, nullptr);

    //wil::com_ptr<IMetaDataDispenserEx> spMetaDataDispenser;
    // The API uses the caller's passed-in metadata dispenser. If null, it
    // will create an instance of the metadata reader to dispense metadata files.
    //if (metaDataDispenser == nullptr)
    //{
    //    RETURN_IF_FAILED(MetaDataGetDispenser(CLSID_CorMetaDataDispenser,
    //        IID_IMetaDataDispenser,
    //        (LPVOID*)&spMetaDataDispenser));
    //    {
    //        variant_t version{ L"WindowsRuntime 1.4" };
    //        RETURN_IF_FAILED(spMetaDataDispenser->SetOption(MetaDataRuntimeVersion, &version.GetVARIANT()));
    //    }
    //}

    // TODO: The original code says it uses the passed-in metadata dispenser, but it 
    // doesn't. It calls ResolveThirdPartyType while always passing in spMetaDataDispenser, 
    // which could be nullptr. This needs to use it if available.

    // all we need to do here is validate that the type exists in the winmd

    return FindTypeInMetaDataFile(
        metaDataDispenser,
        pszFullName,
        m_sdkMetadataFullFilename.c_str(),
        TYPE_RESOLUTION_OPTIONS::TRO_RESOLVE_TYPE_AND_NAMESPACE,
        metaDataImport,
        typeDefToken
    );

}

_Use_decl_annotations_
HRESULT 
MidiSdkComponentCatalog::FindTypeInMetaDataFile(
    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
    _In_ PCWSTR pszFullName,
    _In_ PCWSTR pszCandidateFilePath,
    _In_ TYPE_RESOLUTION_OPTIONS resolutionOptions,
    _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
    _Out_opt_ mdTypeDef* pmdTypeDef)
{
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<IMetaDataImport2> spMetaDataImport;
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
                        *ppMetaDataImport = spMetaDataImport.Detach();
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


