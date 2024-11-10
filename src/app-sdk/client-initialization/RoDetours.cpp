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


#define WIN1019H1_BLDNUM 18362

// Ensure that metadata resolution functions are imported so they can be detoured
extern "C"
{
    __declspec(dllimport) HRESULT WINAPI RoGetMetaDataFile(
        const HSTRING name,
        IMetaDataDispenserEx* metaDataDispenser,
        HSTRING* metaDataFilePath,
        IMetaDataImport2** metaDataImport,
        mdTypeDef* typeDefToken);

    __declspec(dllimport) HRESULT WINAPI RoParseTypeName(
        HSTRING typeName,
        DWORD* partsCount,
        HSTRING** typeNameParts);

    __declspec(dllimport) HRESULT WINAPI RoResolveNamespace(
        const HSTRING name,
        const HSTRING windowsMetaDataDir,
        const DWORD packageGraphDirsCount,
        const HSTRING* packageGraphDirs,
        DWORD* metaDataFilePathsCount,
        HSTRING** metaDataFilePaths,
        DWORD* subNamespacesCount,
        HSTRING** subNamespaces);

    __declspec(dllimport) HRESULT WINAPI RoIsApiContractPresent(
        PCWSTR name,
        UINT16 majorVersion,
        UINT16 minorVersion,
        BOOL* present);

    __declspec(dllimport) HRESULT WINAPI RoIsApiContractMajorVersionPresent(
        PCWSTR name,
        UINT16 majorVersion,
        BOOL* present);
}

static decltype(RoActivateInstance)* TrueRoActivateInstance = RoActivateInstance;
static decltype(RoGetActivationFactory)* TrueRoGetActivationFactory = RoGetActivationFactory;
static decltype(RoGetMetaDataFile)* TrueRoGetMetaDataFile = RoGetMetaDataFile;
static decltype(RoResolveNamespace)* TrueRoResolveNamespace = RoResolveNamespace;

//VERSIONHELPERAPI IsWindowsVersionOrGreaterEx(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor, WORD wBuildNumber)
//{
//    OSVERSIONINFOEXW osvi = { sizeof(osvi) };
//    DWORDLONG const dwlConditionMask =
//        VerSetConditionMask(
//            VerSetConditionMask(
//                VerSetConditionMask(
//                    VerSetConditionMask(
//                        0, VER_MAJORVERSION, VER_GREATER_EQUAL),
//                    VER_MINORVERSION, VER_GREATER_EQUAL),
//                VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL),
//            VER_BUILDNUMBER, VER_GREATER_EQUAL);
//
//    osvi.dwMajorVersion = wMajorVersion;
//    osvi.dwMinorVersion = wMinorVersion;
//    osvi.wServicePackMajor = wServicePackMajor;
//    osvi.dwBuildNumber = wBuildNumber;
//
//    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_BUILDNUMBER, dwlConditionMask) != FALSE;
//}
//
//// TODO: Need to change this function to the minimum func MIDI supports
//// we'll also use this in the "is supported" public API for this SDK
//
//VERSIONHELPERAPI IsWindows1019H1OrGreater()
//{
//    return IsWindowsVersionOrGreaterEx(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 0, WIN1019H1_BLDNUM);
//}

VOID CALLBACK EnsureMTAInitializedCallBack
(
    PTP_CALLBACK_INSTANCE instance,
    PVOID parameter,
    PTP_WORK work
)
{
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(parameter);
    UNREFERENCED_PARAMETER(work);

    wil::com_ptr<IComThreadingInfo> spThreadingInfo;
    CoGetObjectContext(IID_PPV_ARGS(&spThreadingInfo));
}

/*
In the context callback call to the MTA apartment, there is a bug that prevents COM
from automatically initializing MTA remoting. It only allows NTA to be initialized
outside of the NTA and blocks all others. The workaround for this is to spin up another
thread that is not been CoInitialize. COM treats this thread as a implicit MTA and
when we call CoGetObjectContext on it we implicitly initialized the MTA.
*/
HRESULT EnsureMTAInitialized()
{
    TP_CALLBACK_ENVIRON callBackEnviron;
    InitializeThreadpoolEnvironment(&callBackEnviron);
    PTP_POOL pool = CreateThreadpool(nullptr);
    if (pool == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    SetThreadpoolThreadMaximum(pool, 1);
    if (!SetThreadpoolThreadMinimum(pool, 1))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    PTP_CLEANUP_GROUP cleanupgroup = CreateThreadpoolCleanupGroup();
    if (cleanupgroup == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    SetThreadpoolCallbackPool(&callBackEnviron, pool);
    SetThreadpoolCallbackCleanupGroup(&callBackEnviron,
        cleanupgroup,
        nullptr);
    PTP_WORK ensureMTAInitializedWork = CreateThreadpoolWork(
        &EnsureMTAInitializedCallBack,
        nullptr,
        &callBackEnviron);
    if (ensureMTAInitializedWork == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    SubmitThreadpoolWork(ensureMTAInitializedWork);
    CloseThreadpoolCleanupGroupMembers(cleanupgroup,
        false,
        nullptr);
    return S_OK;
}

HRESULT GetActivationLocation(HSTRING activatableClassId, ActivationLocation& activationLocation)
{
    APTTYPE aptType;
    APTTYPEQUALIFIER aptQualifier;
    RETURN_IF_FAILED(CoGetApartmentType(&aptType, &aptQualifier));

    ABI::Windows::Foundation::ThreadingType threading_model;
    RETURN_IF_FAILED(WinRTGetThreadingModel(activatableClassId, &threading_model)); //REGDB_E_CLASSNOTREG

    switch (threading_model)
    {
    case ABI::Windows::Foundation::ThreadingType_BOTH:
        activationLocation = ActivationLocation::CurrentApartment;
        break;
    case ABI::Windows::Foundation::ThreadingType_STA:
        if (aptType == APTTYPE_MTA)
        {
            return RO_E_UNSUPPORTED_FROM_MTA;
        }
        else
        {
            activationLocation = ActivationLocation::CurrentApartment;
        }
        break;
    case ABI::Windows::Foundation::ThreadingType_MTA:
        if (aptType == APTTYPE_MTA)
        {
            activationLocation = ActivationLocation::CurrentApartment;
        }
        else
        {
            activationLocation = ActivationLocation::CrossApartmentMTA;
        }
        break;
    }
    return S_OK;
}




HRESULT WINAPI RoActivateInstanceDetour(HSTRING activatableClassId, IInspectable** instance)
{
    // If not in the MIDI namespace, then call the TrueRoActivateInstance function instead
    if (activatableClassId != NULL)
    {
        // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
        if (!ClassOrNamespaceIsInWindowsMidiServicesNamespace(activatableClassId))
        {
            // not in-scope, so we fall back to the default implementation
            return TrueRoActivateInstance(activatableClassId, instance);
        }
    }
    else
    {
        // null activatableClassId
        return E_INVALIDARG;
    }

    ActivationLocation location;
    HRESULT hr = GetActivationLocation(activatableClassId, location);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        return TrueRoActivateInstance(activatableClassId, instance);
    }
    RETURN_IF_FAILED(hr);

    // Activate in current apartment
    if (location == ActivationLocation::CurrentApartment)
    {
        Microsoft::WRL::ComPtr<IActivationFactory> pFactory;
        RETURN_IF_FAILED(WinRTGetActivationFactory(activatableClassId, __uuidof(IActivationFactory), (void**)&pFactory));
        return pFactory->ActivateInstance(instance);
    }

    // Cross apartment MTA activation
    struct CrossApartmentMTAActData {
        HSTRING activatableClassId;
        IStream* stream;
    };

    CrossApartmentMTAActData cbdata{ activatableClassId };
    CO_MTA_USAGE_COOKIE mtaUsageCookie;
    RETURN_IF_FAILED(CoIncrementMTAUsage(&mtaUsageCookie));
    RETURN_IF_FAILED(EnsureMTAInitialized());
    wil::com_ptr<IContextCallback> defaultContext;
    ComCallData data;
    data.pUserDefined = &cbdata;
    RETURN_IF_FAILED(CoGetDefaultContext(APTTYPE_MTA, IID_PPV_ARGS(&defaultContext)));

    RETURN_IF_FAILED(defaultContext->ContextCallback(
        [](_In_ ComCallData* pComCallData) -> HRESULT
        {
            CrossApartmentMTAActData* data = reinterpret_cast<CrossApartmentMTAActData*>(pComCallData->pUserDefined);
            wil::com_ptr<IInspectable> instance;
            wil::com_ptr<IActivationFactory> pFactory;

            RETURN_IF_FAILED(WinRTGetActivationFactory(data->activatableClassId, __uuidof(IActivationFactory), (void**)&pFactory));
            RETURN_IF_FAILED(pFactory->ActivateInstance(&instance));
            RETURN_IF_FAILED(CoMarshalInterThreadInterfaceInStream(IID_IInspectable, instance.get(), &data->stream));

            return S_OK;
        },
        &data, IID_ICallbackWithNoReentrancyToApplicationSTA, 5, nullptr)); // 5 is meaningless.

    RETURN_IF_FAILED(CoGetInterfaceAndReleaseStream(cbdata.stream, IID_IInspectable, (LPVOID*)instance));
    return S_OK;
}

HRESULT WINAPI RoGetActivationFactoryDetour(HSTRING activatableClassId, REFIID iid, void** factory)
{
    // If not in the MIDI namespace, then call the TrueRoGetActivationFactory function instead
    if (activatableClassId != NULL)
    {
        // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
        if (!ClassOrNamespaceIsInWindowsMidiServicesNamespace(activatableClassId))
        {
            // not in-scope, so we fall back to the default implementation
            return TrueRoGetActivationFactory(activatableClassId, iid, factory);
        }
    }
    else
    {
        // null activatableClassId
        return E_INVALIDARG;
    }


    ActivationLocation location;
    HRESULT hr = GetActivationLocation(activatableClassId, location);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        return TrueRoGetActivationFactory(activatableClassId, iid, factory);
    }
    RETURN_IF_FAILED(hr);

    // Activate in current apartment
    if (location == ActivationLocation::CurrentApartment)
    {
        RETURN_IF_FAILED(WinRTGetActivationFactory(activatableClassId, iid, factory));
        return S_OK;
    }
    // Cross apartment MTA activation
    struct CrossApartmentMTAActData {
        HSTRING activatableClassId;
        IStream* stream;
    };
    CrossApartmentMTAActData cbdata{ activatableClassId };
    CO_MTA_USAGE_COOKIE mtaUsageCookie;
    RETURN_IF_FAILED(CoIncrementMTAUsage(&mtaUsageCookie));
    RETURN_IF_FAILED(EnsureMTAInitialized());
    wil::com_ptr<IContextCallback> defaultContext;
    ComCallData data;
    data.pUserDefined = &cbdata;

    RETURN_IF_FAILED(CoGetDefaultContext(APTTYPE_MTA, IID_PPV_ARGS(&defaultContext)));
    defaultContext->ContextCallback(
        [](_In_ ComCallData* pComCallData) -> HRESULT
        {
            CrossApartmentMTAActData* data = reinterpret_cast<CrossApartmentMTAActData*>(pComCallData->pUserDefined);
            wil::com_ptr<IActivationFactory> pFactory;
            RETURN_IF_FAILED(WinRTGetActivationFactory(data->activatableClassId, __uuidof(IActivationFactory), (void**)&pFactory));
            RETURN_IF_FAILED(CoMarshalInterThreadInterfaceInStream(IID_IActivationFactory, pFactory.get(), &data->stream));

            return S_OK;
        },
        &data, IID_ICallbackWithNoReentrancyToApplicationSTA, 5, nullptr); // 5 is meaningless.

    RETURN_IF_FAILED(CoGetInterfaceAndReleaseStream(cbdata.stream, IID_IActivationFactory, factory));

    return S_OK;
}

HRESULT WINAPI RoGetMetaDataFileDetour(
    const HSTRING name,
    IMetaDataDispenserEx* metaDataDispenser,
    HSTRING* metaDataFilePath,
    IMetaDataImport2** metaDataImport,
    mdTypeDef* typeDefToken)
{
    // If not in the MIDI namespace, then call the TrueRoGetActivationFactory function instead
    if (name != NULL)
    {
        // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
        if (!ClassOrNamespaceIsInWindowsMidiServicesNamespace(name))
        {
            // not in-scope, so we fall back to the default implementation
            return TrueRoGetMetaDataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);
        }
    }
    else
    {
        // null type or namespace
        return E_INVALIDARG;
    }

    HRESULT hr = MidiSdkGetMetadataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);

    // Don't fallback on RO_E_METADATA_NAME_IS_NAMESPACE failure. This is the intended behavior for namespace names.
    if (FAILED(hr) && hr != RO_E_METADATA_NAME_IS_NAMESPACE)
    {
        hr = TrueRoGetMetaDataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);
    }
    return hr;
}

HRESULT WINAPI RoResolveNamespaceDetour(
    const HSTRING name,
    const HSTRING windowsMetaDataDir,
    const DWORD packageGraphDirsCount,
    const HSTRING* packageGraphDirs,
    DWORD* metaDataFilePathsCount,
    HSTRING** metaDataFilePaths,
    DWORD* subNamespacesCount,
    HSTRING** subNamespaces)
{
    // TODO: If not in the MIDI namespace, then call the TrueRoResolveNamespace function instead
    if (name != NULL)
    {
        // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
        if (!ClassOrNamespaceIsInWindowsMidiServicesNamespace(name))
        {
            // not in-scope, so we fall back to the default implementation
            return TrueRoResolveNamespace(
                name, 
                windowsMetaDataDir, 
                packageGraphDirsCount, 
                packageGraphDirs, 
                metaDataFilePathsCount, 
                metaDataFilePaths, 
                subNamespacesCount, 
                subNamespaces);
        }
    }
    else
    {
        // null type or namespace
        return E_INVALIDARG;
    }

    // otherwise, set to the SDK install information, not the process information below
    auto sdkPath = GetMidiSdkPath();

    if (sdkPath.has_value())
    {
        PCWSTR sdkFilePath = sdkPath.value().c_str();
        auto pathReference = Microsoft::WRL::Wrappers::HStringReference(sdkFilePath);

        HSTRING packageGraphDirectories[] = { pathReference.Get() };

        HRESULT hr = TrueRoResolveNamespace(
            name,
            pathReference.Get(),
            ARRAYSIZE(packageGraphDirectories),
            packageGraphDirectories,
            metaDataFilePathsCount,
            metaDataFilePaths,
            subNamespacesCount,
            subNamespaces);

        return hr;
    }
    else
    {
        // no sdk installed. How did we get this far?
        return E_FAIL;
    }
}

HRESULT InstallHooks()
{
    // TODO: Should read the registry stuff here, and then cache it
    // This is also where we should check for the SDK install. If it's
    // not present, then we don't install the hooks








    // If this is loaded in a Detours helper process and not the actual process
    // to be hooked, just return without performing any other operations.
    if (DetourIsHelperProcess())
        return S_OK;

    DetourRestoreAfterWith();

    RETURN_IF_WIN32_ERROR(DetourTransactionBegin());
    RETURN_IF_WIN32_ERROR(DetourUpdateThread(GetCurrentThread()));
    RETURN_IF_WIN32_ERROR(DetourAttach(&(PVOID&)TrueRoActivateInstance, RoActivateInstanceDetour));
    RETURN_IF_WIN32_ERROR(DetourAttach(&(PVOID&)TrueRoGetActivationFactory, RoGetActivationFactoryDetour));
    RETURN_IF_WIN32_ERROR(DetourAttach(&(PVOID&)TrueRoGetMetaDataFile, RoGetMetaDataFileDetour));
    RETURN_IF_WIN32_ERROR(DetourAttach(&(PVOID&)TrueRoResolveNamespace, RoResolveNamespaceDetour));
    RETURN_IF_WIN32_ERROR(DetourTransactionCommit());
    return S_OK;
}

void RemoveHooks()
{

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)TrueRoActivateInstance, RoActivateInstanceDetour);
    DetourDetach(&(PVOID&)TrueRoGetActivationFactory, RoGetActivationFactoryDetour);
    DetourDetach(&(PVOID&)TrueRoGetMetaDataFile, RoGetMetaDataFileDetour);
    DetourDetach(&(PVOID&)TrueRoResolveNamespace, RoResolveNamespaceDetour);
    DetourTransactionCommit();
}

HRESULT ExtRoLoadCatalog()
{
    // TODO: This is where we'll build the catalog of MIDI types

    // Get path from registry

    // set the filepath to that path

    // no manifest - or perhaps we include the manifest in with the SDK impl and winmd files?

    return LoadMidiComponentTypes();

}


extern "C" void WINAPI winrtact_Initialize()
{
    return;
}
