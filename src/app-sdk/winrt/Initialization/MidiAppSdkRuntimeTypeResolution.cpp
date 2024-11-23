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

#include "MidiAppSdkRuntimeTypeResolution.h"



//
// MetaDataImportersLRUCache implementation
//
INIT_ONCE MetaDataImportersLRUCache::s_initOnce = INIT_ONCE_STATIC_INIT;
MetaDataImportersLRUCache* MetaDataImportersLRUCache::s_pMetaDataImportersLRUCacheInstance = nullptr;

MetaDataImportersLRUCache* MetaDataImportersLRUCache::GetMetaDataImportersLRUCacheInstance()
{
    BOOL fInitializationSucceeded = InitOnceExecuteOnce(
        &s_initOnce,
        ConstructLRUCacheIfNecessary,
        nullptr,
        nullptr);

    UNREFERENCED_PARAMETER(fInitializationSucceeded);

    return s_pMetaDataImportersLRUCacheInstance;
}

// Called via InitOnceExecuteOnce.
BOOL CALLBACK MetaDataImportersLRUCache::ConstructLRUCacheIfNecessary(
    PINIT_ONCE /*initOnce*/,
    PVOID /*parameter*/,
    PVOID* /*context*/)
{
    HRESULT hr = S_OK;

    if (s_pMetaDataImportersLRUCacheInstance == nullptr)
    {
        s_pMetaDataImportersLRUCacheInstance = new MetaDataImportersLRUCache();

        if (s_pMetaDataImportersLRUCacheInstance == nullptr)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    return SUCCEEDED(hr);
}

HRESULT MetaDataImportersLRUCache::GetMetaDataImporter(
    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
    _In_ PCWSTR pszCandidateFilePath,
    _Outptr_opt_ IMetaDataImport2** ppMetaDataImporter)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(pszCandidateFilePath, "Candidate file path")
    );

    if (ppMetaDataImporter == nullptr)
    {
        return ERROR_BAD_ARGUMENTS;
    }

    HRESULT hr = S_OK;
    *ppMetaDataImporter = nullptr;

    EnterCriticalSection(&_csCacheLock);

    if (IsFilePathCached(pszCandidateFilePath))
    {
        // Get metadata importer from cache.
        *ppMetaDataImporter = _metadataImportersMap[pszCandidateFilePath];
        IMetaDataImport2* value = *ppMetaDataImporter;
        if (value != nullptr)
        {
            value->AddRef();
        }
    }
    else
    {
        // Importer was not found in cache.
        hr = GetNewMetaDataImporter(
            pMetaDataDispenser,
            pszCandidateFilePath,
            ppMetaDataImporter);
    }

    LeaveCriticalSection(&_csCacheLock);

    return hr;
}

HRESULT MetaDataImportersLRUCache::GetNewMetaDataImporter(
    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
    _In_ PCWSTR pszCandidateFilePath,
    _Outptr_opt_ IMetaDataImport2** ppMetaDataImporter)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(pszCandidateFilePath, "Candidate file path")
    );


    if (pMetaDataDispenser == nullptr)
    {
        return E_INVALIDARG;
    }

    if (ppMetaDataImporter == nullptr)
    {
        return ERROR_BAD_ARGUMENTS;
    }

    HRESULT hr;

    hr = pMetaDataDispenser->OpenScope(
        pszCandidateFilePath,
        ofReadOnly,
        IID_IMetaDataImport2,
        reinterpret_cast<IUnknown**>(ppMetaDataImporter));

    if (SUCCEEDED(hr))
    {
        _metadataImportersMap.emplace(
            pszCandidateFilePath,
            *ppMetaDataImporter);
        IMetaDataImport2* value = *ppMetaDataImporter;
        if (value != nullptr)
        {
            value->AddRef();
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = AddNewFilePathToList(pszCandidateFilePath);
    }

    return hr;
}

HRESULT MetaDataImportersLRUCache::AddNewFilePathToList(PCWSTR pszFilePath)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(pszFilePath, "File path")
    );


    HRESULT hr = RemoveLeastRecentlyUsedItemIfListIsFull();

    if (SUCCEEDED(hr))
    {
        // Make room for new element.
        for (int i = g_dwMetaDataImportersLRUCacheSize - 2; i >= 0; i--)
        {
            _arFilePaths[i + 1] = _arFilePaths[i];
        }

        _arFilePaths[0] = AllocateAndCopyString(pszFilePath);

        if (_arFilePaths[0] == nullptr)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    return hr;
}

bool MetaDataImportersLRUCache::IsFilePathCached(PCWSTR pszFilePath)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    int filePathIndex = GetFilePathIndex(pszFilePath);

    if (filePathIndex != -1)
    {
        MoveElementToFrontOfList(filePathIndex);
        return true;
    }
    else
    {
        return false;
    }
}

int MetaDataImportersLRUCache::GetFilePathIndex(PCWSTR pszFilePath)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    int filePathIndex = -1;

    for (int i = 0; (i < g_dwMetaDataImportersLRUCacheSize) && (_arFilePaths[i] != nullptr); i++)
    {
        if (wcscmp(pszFilePath, _arFilePaths[i]) == 0)
        {
            filePathIndex = i;
            break;
        }
    }

    return filePathIndex;
}

void MetaDataImportersLRUCache::MoveElementToFrontOfList(int elementIndex)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    PWSTR pszFoundFilePath = _arFilePaths[elementIndex];

    for (int i = elementIndex - 1; i >= 0; i--)
    {
        _arFilePaths[i + 1] = _arFilePaths[i];
    }

    _arFilePaths[0] = pszFoundFilePath;
}

HRESULT MetaDataImportersLRUCache::RemoveLeastRecentlyUsedItemIfListIsFull()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    HRESULT hr = S_OK;
    PWSTR pszLastFilePathInList = _arFilePaths[g_dwMetaDataImportersLRUCacheSize - 1];

    if (pszLastFilePathInList != nullptr)
    {
        IMetaDataImport2* value = _metadataImportersMap[pszLastFilePathInList];
        if (value != nullptr)
        {
            value->Release();
            value = nullptr;
        }
        if (!_metadataImportersMap.erase(pszLastFilePathInList))
        {
            hr = E_UNEXPECTED;
        }

        delete[] pszLastFilePathInList;
        _arFilePaths[g_dwMetaDataImportersLRUCacheSize - 1] = nullptr;
    }

    return hr;
}



