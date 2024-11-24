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


#include <activationregistration.h>
#include <string>
#include <string_view>
#include <cor.h>
#include <xmllite.h>
#include <Shlwapi.h>

#include "wil/result.h"
#include "wil/resource.h"

HRESULT LoadManifestFromPath(std::wstring path);

HRESULT LoadFromSxSManifest(PCWSTR path);

HRESULT LoadFromEmbeddedManifest(PCWSTR path);

HRESULT WinRTLoadComponentFromFilePath(PCWSTR manifestPath);

HRESULT WinRTLoadComponentFromString(std::string_view xmlStringValue);

HRESULT ParseRootManifestFromXmlReaderInput(IUnknown* pInput);

HRESULT ParseFileTag(Microsoft::WRL::ComPtr<IXmlReader> xmlReader);

HRESULT ParseActivatableClassTag(Microsoft::WRL::ComPtr<IXmlReader> xmlReader, PCWSTR fileName);

HRESULT WinRTGetThreadingModel(
    HSTRING activatableClassId,
    ABI::Windows::Foundation::ThreadingType* threading_model);

HRESULT WinRTGetActivationFactory(
    HSTRING activatableClassId,
    REFIID  iid,
    void** factory);

HRESULT WinRTGetMetadataFile(
    const HSTRING        name,
    IMetaDataDispenserEx* metaDataDispenser,
    HSTRING* metaDataFilePath,
    IMetaDataImport2** metaDataImport,
    mdTypeDef* typeDefToken);