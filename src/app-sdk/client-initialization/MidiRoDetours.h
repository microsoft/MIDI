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

#define WIN1019H1_BLDNUM 18362

enum class ActivationLocation
{
    CurrentApartment,
    CrossApartmentMTA
};


class MidiRoDetours
{
public:
    HRESULT Initialize();
    HRESULT Shutdown();


    //VERSIONHELPERAPI IsWindowsVersionOrGreaterEx(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor, WORD wBuildNumber);
    //VERSIONHELPERAPI IsWindows1019H1OrGreater();

    /*
    In the context callback call to the MTA apartment, there is a bug that prevents COM
    from automatically initializing MTA remoting. It only allows NTA to be initialized
    outside of the NTA and blocks all others. The workaround for this is to spin up another
    thread that is not been CoInitialize. COM treats this thread as a implicit MTA and
    when we call CoGetObjectContext on it we implicitly initialized the MTA.
    */
    HRESULT EnsureMTAInitialized();

    HRESULT GetActivationLocation(
        _In_ HSTRING activatableClassId, 
        _In_ ActivationLocation& activationLocation);

    HRESULT WINAPI ActivateInstanceDetour(
        _In_ HSTRING activatableClassId, 
        _Inout_ IInspectable** instance);

    HRESULT WINAPI GetActivationFactoryDetour(
        _In_ HSTRING activatableClassId, 
        _In_ REFIID iid, 
        _Inout_ void** factory);

    HRESULT WINAPI GetMetaDataFileDetour(
        _In_ const HSTRING name,
        _In_opt_ IMetaDataDispenserEx* metaDataDispenser,
        _In_ HSTRING* metaDataFilePath,
        _In_ IMetaDataImport2** metaDataImport,
        _In_ mdTypeDef* typeDefToken);

    HRESULT WINAPI ResolveNamespaceDetour(
        _In_ const HSTRING name,
        _In_ const HSTRING windowsMetaDataDir,
        _In_ const DWORD packageGraphDirsCount,
        _In_ const HSTRING* packageGraphDirs,
        _In_ DWORD* metaDataFilePathsCount,
        _In_ HSTRING** metaDataFilePaths,
        _In_ DWORD* subNamespacesCount,
        _In_ HSTRING** subNamespaces);

    HRESULT InstallHooks();
    void RemoveHooks();

//    HRESULT ExtRoLoadCatalog();

private:
    std::shared_ptr<MidiAppSdkRuntimeComponentCatalog> m_catalog;


};

VOID CALLBACK EnsureMTAInitializedCallBack(
    _In_ PTP_CALLBACK_INSTANCE instance,
    _In_ PVOID parameter,
    _In_ PTP_WORK work
);



inline HRESULT WINAPI RoActivateInstanceDetour(
    _In_ HSTRING activatableClassId,
    _Inout_ IInspectable** instance)
{
    UNREFERENCED_PARAMETER(activatableClassId);
    UNREFERENCED_PARAMETER(instance);

    return S_OK;
}

inline HRESULT WINAPI RoGetActivationFactoryDetour(
    _In_ HSTRING activatableClassId,
    _In_ REFIID iid,
    _Inout_ void** factory)
{
    UNREFERENCED_PARAMETER(activatableClassId);
    UNREFERENCED_PARAMETER(iid);
    UNREFERENCED_PARAMETER(factory);

    return S_OK;
}

inline HRESULT WINAPI RoGetMetaDataFileDetour(
    _In_ const HSTRING name,
    _In_ IMetaDataDispenserEx* metaDataDispenser,
    _In_ HSTRING* metaDataFilePath,
    _In_ IMetaDataImport2** metaDataImport,
    _In_ mdTypeDef* typeDefToken)
{
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(metaDataDispenser);
    UNREFERENCED_PARAMETER(metaDataFilePath);
    UNREFERENCED_PARAMETER(metaDataImport);
    UNREFERENCED_PARAMETER(typeDefToken);

    return S_OK;
}

inline HRESULT WINAPI RoResolveNamespaceDetour(
    _In_ const HSTRING name,
    _In_ const HSTRING windowsMetaDataDir,
    _In_ const DWORD packageGraphDirsCount,
    _In_ const HSTRING* packageGraphDirs,
    _In_ DWORD* metaDataFilePathsCount,
    _In_ HSTRING** metaDataFilePaths,
    _In_ DWORD* subNamespacesCount,
    _In_ HSTRING** subNamespaces)
{
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(windowsMetaDataDir);
    UNREFERENCED_PARAMETER(packageGraphDirsCount);
    UNREFERENCED_PARAMETER(packageGraphDirs);
    UNREFERENCED_PARAMETER(metaDataFilePathsCount);
    UNREFERENCED_PARAMETER(metaDataFilePaths);
    UNREFERENCED_PARAMETER(subNamespacesCount);
    UNREFERENCED_PARAMETER(subNamespaces);

    return S_OK;
}



//extern "C" void WINAPI winrtact_Initialize()
//{
//    return;
//}


