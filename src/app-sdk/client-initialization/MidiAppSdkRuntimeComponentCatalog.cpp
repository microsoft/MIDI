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

HRESULT LoadMidiComponentTypes()
{
    auto types = GetMidiAppSdkManifestTypes();

    for (auto const& t : types)
    {
        auto component = std::make_shared<MidiAppSdkRuntimeComponent>();

        component->module_name = t.FileName;
        component->threading_model = t.ThreadingModel;
        
        g_types[t.ActivatableClassName] = component;
    }

    return S_OK;
}

HRESULT WinRTGetThreadingModel(HSTRING activatableClassId, ABI::Windows::Foundation::ThreadingType* threading_model)
{
    auto raw_class_name = WindowsGetStringRawBuffer(activatableClassId, nullptr);
    auto component_iter = g_types.find(raw_class_name);
    if (component_iter != g_types.end())
    {
        *threading_model = component_iter->second->threading_model;
        return S_OK;
    }

    return REGDB_E_CLASSNOTREG;
}

HRESULT WinRTGetActivationFactory(
    HSTRING activatableClassId,
    REFIID  iid,
    void** factory)
{
    auto raw_class_name = WindowsGetStringRawBuffer(activatableClassId, nullptr);
    auto component_iter = g_types.find(raw_class_name);
    if (component_iter != g_types.end())
    {
        return component_iter->second->GetActivationFactory(activatableClassId, iid, factory);
    }

    return REGDB_E_CLASSNOTREG;
}

// we assume by the point this has been called that the type
// has been validated to be a MIDI SDK type
HRESULT MidiSdkGetMetadataFile(
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

    auto metadataFile = GetMidiSdkMetadataFullFileName();

    // TODO: The original code says it uses the passed-in metadata dispenser, but it 
    // doesn't. It calls ResolveThirdPartyType while always passing in spMetaDataDispenser, 
    // which could be nullptr. This needs to use it if available.

    // all we need to do here is validate that the type exists in the winmd

    return FindTypeInMetaDataFile(
        metaDataDispenser,
        pszFullName,
        metadataFile.c_str(),
        TYPE_RESOLUTION_OPTIONS::TRO_RESOLVE_TYPE_AND_NAMESPACE,
        metaDataImport,
        typeDefToken
    );

}

