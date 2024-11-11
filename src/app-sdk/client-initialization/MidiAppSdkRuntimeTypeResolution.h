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


// Returned string is cached globally, and should not be freed by the caller.
//HRESULT GetProcessExeDir(PCWSTR* path);

//HRESULT FindTypeInMetaDataFile(
//    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
//    _In_ PCWSTR pszFullName,
//    _In_ PCWSTR pszCandidateFilePath,
//    _In_ TYPE_RESOLUTION_OPTIONS resolutionOptions,
//    _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
//    _Out_opt_ mdTypeDef* pmdTypeDef);

//HRESULT FindTypeInDirectory(
//    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
//    _In_ PCWSTR pszFullName,
//    _In_ PCWSTR pszDirectoryPath,
//    _Out_opt_ HSTRING* phstrMetaDataFilePath,
//    _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
//    _Out_opt_ mdTypeDef* pmdTypeDef);
//
//HRESULT FindTypeInDirectoryWithNormalization(
//    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
//    _In_ PCWSTR pszFullName,
//    _In_ PCWSTR pszDirectoryPath,
//    _Out_opt_ HSTRING* phstrMetaDataFilePath,
//    _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
//    _Out_opt_ mdTypeDef* pmdTypeDef);
//
//HRESULT ResolveThirdPartyType(
//    _In_ IMetaDataDispenserEx* pMetaDataDispenser,
//    _In_ PCWSTR pszFullName,
//    _Out_opt_ HSTRING* phstrMetaDataFilePath,
//    _COM_Outptr_opt_result_maybenull_ IMetaDataImport2** ppMetaDataImport,
//    _Out_opt_ mdTypeDef* pmdTypeDef);

inline PWSTR AllocateAndCopyString(_In_ PCWSTR pszKey)
{
    size_t length = wcslen(pszKey);
    PWSTR pszKeyCopy = nullptr;

    if (length <= MAX_PATH)
    {
        pszKeyCopy = new WCHAR[length + 1];
        if (pszKeyCopy != nullptr)
        {
            SUCCEEDED(StringCchCopyW(pszKeyCopy, length + 1, pszKey));
        }
    }

    return pszKeyCopy;
}

//
// Metada importers LRU cache. Singleton.
//
const DWORD g_dwMetaDataImportersLRUCacheSize = 5;

class MetaDataImportersLRUCache
{
public:
    static MetaDataImportersLRUCache* GetMetaDataImportersLRUCacheInstance();

    HRESULT GetMetaDataImporter(
        _In_ IMetaDataDispenserEx* pMetaDataDispenser,
        _In_ PCWSTR pszCandidateFilePath,
        _Outptr_opt_ IMetaDataImport2** ppMetaDataImporter);

private:
    MetaDataImportersLRUCache()
    {
        InitializeCriticalSection(&_csCacheLock);
        ZeroMemory(_arFilePaths, sizeof(_arFilePaths));
    }

    ~MetaDataImportersLRUCache()
    {
        _metadataImportersMap.clear();
        for (int i = 0; i < g_dwMetaDataImportersLRUCacheSize; i++)
        {
            if (_arFilePaths[i] != nullptr)
            {
                delete[] _arFilePaths[i];
                _arFilePaths[i] = nullptr;
            }
        }

        DeleteCriticalSection(&_csCacheLock);
    }

    static BOOL CALLBACK ConstructLRUCacheIfNecessary(
        PINIT_ONCE /*initOnce*/,
        PVOID /*parameter*/,
        PVOID* /*context*/);

    HRESULT GetNewMetaDataImporter(
        _In_ IMetaDataDispenserEx* pMetaDataDispenser,
        _In_ PCWSTR pszCandidateFilePath,
        _Outptr_opt_ IMetaDataImport2** ppMetaDataImporter);

    bool IsFilePathCached(PCWSTR pszFilePath);
    int GetFilePathIndex(PCWSTR pszFilePath);
    void MoveElementToFrontOfList(int elementIndex);
    HRESULT AddNewFilePathToList(PCWSTR pszFilePath);
    HRESULT RemoveLeastRecentlyUsedItemIfListIsFull();

    static INIT_ONCE s_initOnce;
    static MetaDataImportersLRUCache* s_pMetaDataImportersLRUCacheInstance;
    std::unordered_map<std::wstring, IMetaDataImport2*> _metadataImportersMap;
    PWSTR _arFilePaths[g_dwMetaDataImportersLRUCacheSize];
    CRITICAL_SECTION _csCacheLock;
};



