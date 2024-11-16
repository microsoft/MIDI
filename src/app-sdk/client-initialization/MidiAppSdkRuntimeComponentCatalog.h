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


#pragma once

typedef enum
{
    TRO_NONE = 0x00,
    TRO_RESOLVE_TYPE = 0x01,
    TRO_RESOLVE_NAMESPACE = 0x02,
    TRO_RESOLVE_TYPE_AND_NAMESPACE = TRO_RESOLVE_TYPE | TRO_RESOLVE_NAMESPACE
} TYPE_RESOLUTION_OPTIONS;

struct MidiAppSdkManifestEntry
{
    std::wstring ActivatableClassName{};
    std::wstring FileName{};
    ABI::Windows::Foundation::ThreadingType ThreadingModel{ ABI::Windows::Foundation::ThreadingType::ThreadingType_BOTH };
};

class MidiAppSdkRuntimeComponentCatalog
{
public:

    STDMETHOD(Initialize)();

    STDMETHOD(GetActivationFactory)(
        _In_ HSTRING activatableClassId,
        _In_ REFIID  iid,
        _COM_Outptr_ void** factory
        );

    STDMETHOD(GetThreadingModel)(
        _In_ HSTRING activatableClassId,
        _In_ ABI::Windows::Foundation::ThreadingType* threading_model
        );

    STDMETHOD(GetMetadataFile)(
        _In_ const HSTRING name,
        _In_ IMetaDataDispenserEx* metaDataDispenser,
        _In_ HSTRING* metaDataFilePath,
        _In_ IMetaDataImport2** metaDataImport,
        _In_ mdTypeDef* typeDefToken
        );

    STDMETHOD(FindTypeInMetaDataFile)(
        _In_ IMetaDataDispenserEx* pMetaDataDispenser,
        _In_ PCWSTR pszFullName,
        _In_ PCWSTR pszCandidateFilePath,
        _In_ TYPE_RESOLUTION_OPTIONS resolutionOptions,
        _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
        _Out_opt_ mdTypeDef* pmdTypeDef
        );

    bool TypeIsInScope(_In_ HSTRING const typeOrNamespace);

    STDMETHOD(Shutdown)();

    std::wstring GetSdkDirectory() { return m_sdkDirectory; }
    std::wstring GetSdkImplementationFullFilename() { return m_sdkImplementationFullFilename; }
    std::wstring GetSdkMetadataFullFilename() { return m_sdkMetadataFullFilename; }


private:
    std::unordered_map<std::wstring, std::shared_ptr<MidiAppSdkRuntimeComponent>> m_types;

    std::vector<MidiAppSdkManifestEntry> GetMidiAppSdkManifestTypes();

    std::wstring m_sdkDirectory{};
    std::wstring m_sdkImplementationFullFilename{};
    std::wstring m_sdkMetadataFullFilename{};

};

//HRESULT LoadManifestFromPath(std::wstring path);

//HRESULT LoadFromSxSManifest(PCWSTR path);
//HRESULT LoadFromEmbeddedManifest(PCWSTR path);

//HRESULT WinRTLoadComponentFromFilePath(PCWSTR manifestPath);
//HRESULT WinRTLoadComponentFromString(std::string_view xmlStringValue);

//HRESULT ParseRootManifestFromXmlReaderInput(IUnknown* input);

//HRESULT ParseFileTag(wil::com_ptr<IXmlReader> xmlReader);

//HRESULT ParseActivatableClassTag(wil::com_ptr<IXmlReader> xmlReader, PCWSTR fileName);

//HRESULT WinRTGetThreadingModel(HSTRING activatableClassId, ABI::Windows::Foundation::ThreadingType* threading_model);

//HRESULT WinRTGetMetadataFile(
//    const HSTRING        name,
//    IMetaDataDispenserEx* metaDataDispenser,
//    HSTRING* metaDataFilePath,
//    IMetaDataImport2** metaDataImport,
//    mdTypeDef* typeDefToken);


