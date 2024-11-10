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

VERSIONHELPERAPI IsWindowsVersionOrGreaterEx(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor, WORD wBuildNumber);
VERSIONHELPERAPI IsWindows1019H1OrGreater();

VOID CALLBACK EnsureMTAInitializedCallBack(
    PTP_CALLBACK_INSTANCE instance,
    PVOID parameter,
    PTP_WORK work
);

/*
In the context callback call to the MTA apartment, there is a bug that prevents COM
from automatically initializing MTA remoting. It only allows NTA to be initialized
outside of the NTA and blocks all others. The workaround for this is to spin up another
thread that is not been CoInitialize. COM treats this thread as a implicit MTA and
when we call CoGetObjectContext on it we implicitly initialized the MTA.
*/
HRESULT EnsureMTAInitialized();

HRESULT GetActivationLocation(HSTRING activatableClassId, ActivationLocation& activationLocation);

HRESULT WINAPI RoActivateInstanceDetour(HSTRING activatableClassId, IInspectable** instance);

HRESULT WINAPI RoGetActivationFactoryDetour(HSTRING activatableClassId, REFIID iid, void** factory);

HRESULT WINAPI RoGetMetaDataFileDetour(
    const HSTRING name,
    IMetaDataDispenserEx* metaDataDispenser,
    HSTRING* metaDataFilePath,
    IMetaDataImport2** metaDataImport,
    mdTypeDef* typeDefToken);

HRESULT WINAPI RoResolveNamespaceDetour(
    const HSTRING name,
    const HSTRING windowsMetaDataDir,
    const DWORD packageGraphDirsCount,
    const HSTRING* packageGraphDirs,
    DWORD* metaDataFilePathsCount,
    HSTRING** metaDataFilePaths,
    DWORD* subNamespacesCount,
    HSTRING** subNamespaces);

HRESULT InstallHooks();
void RemoveHooks();

HRESULT ExtRoLoadCatalog();


//extern "C" void WINAPI winrtact_Initialize()
//{
//    return;
//}
