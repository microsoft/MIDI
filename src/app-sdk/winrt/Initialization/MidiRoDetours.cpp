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

#include "MidiRoDetours.h"


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

// keep the original function pointers
static decltype(RoActivateInstance)* TrueRoActivateInstance = RoActivateInstance;
static decltype(RoGetActivationFactory)* TrueRoGetActivationFactory = RoGetActivationFactory;
static decltype(RoGetMetaDataFile)* TrueRoGetMetaDataFile = RoGetMetaDataFile;
static decltype(RoResolveNamespace)* TrueRoResolveNamespace = RoResolveNamespace;



_Use_decl_annotations_
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
    LOG_IF_FAILED(CoGetObjectContext(IID_PPV_ARGS(&spThreadingInfo)));
}

/*
In the context callback call to the MTA apartment, there is a bug that prevents COM
from automatically initializing MTA remoting. It only allows NTA to be initialized
outside of the NTA and blocks all others. The workaround for this is to spin up another
thread that is not been CoInitialize. COM treats this thread as a implicit MTA and
when we call CoGetObjectContext on it we implicitly initialized the MTA.
*/
HRESULT 
EnsureMTAInitialized()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

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

_Use_decl_annotations_
HRESULT 
GetActivationLocation(
    HSTRING activatableClassId, 
    ActivationLocation& activationLocation)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(activatableClassId, NULL), "class")
    );

    RETURN_HR_IF_NULL(E_POINTER, g_runtimeComponentCatalog);

    APTTYPE aptType;
    APTTYPEQUALIFIER aptQualifier;
    RETURN_IF_FAILED(CoGetApartmentType(&aptType, &aptQualifier));

    ABI::Windows::Foundation::ThreadingType threading_model{ ABI::Windows::Foundation::ThreadingType::ThreadingType_BOTH };
    RETURN_IF_FAILED(g_runtimeComponentCatalog->GetThreadingModel(activatableClassId, &threading_model)); //REGDB_E_CLASSNOTREG

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


_Use_decl_annotations_
HRESULT WINAPI 
RoActivateInstanceDetour(
    HSTRING activatableClassId, 
    IInspectable** instance)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(activatableClassId, NULL), "class")
    );


    RETURN_HR_IF_NULL(E_INVALIDARG, activatableClassId);
    RETURN_HR_IF_NULL(E_POINTER, g_runtimeComponentCatalog);

    // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
    if (!g_runtimeComponentCatalog->TypeIsInScope(activatableClassId))
    {
        // not in-scope, so we fall back to the default implementation
        return TrueRoActivateInstance(activatableClassId, instance);
    }


    ActivationLocation location{ ActivationLocation::CurrentApartment };
    HRESULT hr = GetActivationLocation(activatableClassId, location);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        return TrueRoActivateInstance(activatableClassId, instance);
    }
    RETURN_IF_FAILED(hr);

    // Activate in current apartment
    if (location == ActivationLocation::CurrentApartment)
    {
        wil::com_ptr<IActivationFactory> pFactory;
        RETURN_IF_FAILED(g_runtimeComponentCatalog->GetActivationFactory(activatableClassId, __uuidof(IActivationFactory), (void**)&pFactory));
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
            CrossApartmentMTAActData* mtadata = reinterpret_cast<CrossApartmentMTAActData*>(pComCallData->pUserDefined);
            wil::com_ptr<IInspectable> instanceToActivate;
            wil::com_ptr<IActivationFactory> pFactory;

            RETURN_IF_FAILED(g_runtimeComponentCatalog->GetActivationFactory(mtadata->activatableClassId, __uuidof(IActivationFactory), (void**)&pFactory));
            RETURN_IF_FAILED(pFactory->ActivateInstance(&instanceToActivate));
            RETURN_IF_FAILED(CoMarshalInterThreadInterfaceInStream(IID_IInspectable, instanceToActivate.get(), &mtadata->stream));

            return S_OK;
        },
        &data, 
        IID_ICallbackWithNoReentrancyToApplicationSTA, 
        5, 
        nullptr)); // 5 is meaningless.

    RETURN_IF_FAILED(CoGetInterfaceAndReleaseStream(cbdata.stream, IID_IInspectable, (LPVOID*)instance));

    return S_OK;
}

_Use_decl_annotations_
HRESULT WINAPI 
RoGetActivationFactoryDetour(
    HSTRING activatableClassId, 
    REFIID iid, 
    void** factory)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(activatableClassId, NULL), "class")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, activatableClassId);
    RETURN_HR_IF_NULL(E_POINTER, g_runtimeComponentCatalog);

    // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
    if (!g_runtimeComponentCatalog->TypeIsInScope(activatableClassId))
    {
        // not in-scope, so we fall back to the default implementation
        return TrueRoGetActivationFactory(activatableClassId, iid, factory);
    }


    ActivationLocation location{ ActivationLocation::CurrentApartment };
    HRESULT hr = GetActivationLocation(activatableClassId, location);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        return TrueRoGetActivationFactory(activatableClassId, iid, factory);
    }
    RETURN_IF_FAILED(hr);

    // Activate in current apartment
    if (location == ActivationLocation::CurrentApartment)
    {
        RETURN_IF_FAILED(g_runtimeComponentCatalog->GetActivationFactory(activatableClassId, iid, factory));
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
            RETURN_IF_FAILED(g_runtimeComponentCatalog->GetActivationFactory(data->activatableClassId, __uuidof(IActivationFactory), (void**)&pFactory));
            RETURN_IF_FAILED(CoMarshalInterThreadInterfaceInStream(IID_IActivationFactory, pFactory.get(), &data->stream));

            return S_OK;
        },
        &data, 
        IID_ICallbackWithNoReentrancyToApplicationSTA, 
        5, 
        nullptr); // 5 is meaningless.

    RETURN_IF_FAILED(CoGetInterfaceAndReleaseStream(cbdata.stream, IID_IActivationFactory, factory));

    return S_OK;
}

_Use_decl_annotations_
HRESULT WINAPI 
RoGetMetaDataFileDetour(
    const HSTRING name,
    IMetaDataDispenserEx* metaDataDispenser,
    HSTRING* metaDataFilePath,
    IMetaDataImport2** metaDataImport,
    mdTypeDef* typeDefToken)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(name, NULL), "name")
    );


    RETURN_HR_IF_NULL(E_INVALIDARG, name);
    RETURN_HR_IF_NULL(E_POINTER, g_runtimeComponentCatalog);

    // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
    if (!g_runtimeComponentCatalog->TypeIsInScope(name))
    {
        // not in-scope, so we fall back to the default implementation
        return TrueRoGetMetaDataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);
    }

    HRESULT hr = g_runtimeComponentCatalog->GetMetadataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);

    // Don't fallback on RO_E_METADATA_NAME_IS_NAMESPACE failure. This is the intended behavior for namespace names.
    if (FAILED(hr) && hr != RO_E_METADATA_NAME_IS_NAMESPACE)
    {
        hr = TrueRoGetMetaDataFile(name, metaDataDispenser, metaDataFilePath, metaDataImport, typeDefToken);
    }
    return hr;
}

_Use_decl_annotations_
HRESULT WINAPI 
RoResolveNamespaceDetour(
    const HSTRING name,
    const HSTRING windowsMetaDataDir,
    const DWORD packageGraphDirsCount,
    const HSTRING* packageGraphDirs,
    DWORD* metaDataFilePathsCount,
    HSTRING** metaDataFilePaths,
    DWORD* subNamespacesCount,
    HSTRING** subNamespaces)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(WindowsGetStringRawBuffer(name, NULL), "name")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, name);
    RETURN_HR_IF_NULL(E_POINTER, g_runtimeComponentCatalog);

    // Check for MIDI_SDK_ROOT_NAMESPACE in the activatableClassId
    if (!g_runtimeComponentCatalog->TypeIsInScope(name))
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

    // otherwise, set to the SDK install information, not the process information below
    //auto sdkPath = g_runtimeComponentCatalog->GetSdkDirectory();

    //PCWSTR sdkFilePath = sdkPath.c_str();
    //auto pathReference = Microsoft::WRL::Wrappers::HStringReference(sdkFilePath);
    //HSTRING packageGraphDirectories[] = { pathReference.Get() };

    std::wstring sdkMetadataFilePath{ g_runtimeComponentCatalog->GetSdkMetadataFullFilename() };

    HSTRING winmdPath;
    if (SUCCEEDED(WindowsCreateString(sdkMetadataFilePath.c_str(), static_cast<UINT32>(sdkMetadataFilePath.length()), &winmdPath)))
    {
        HSTRING winmdPaths[] = { winmdPath };
        *metaDataFilePathsCount = ARRAYSIZE(winmdPaths);
        *metaDataFilePaths = winmdPaths;
    }

    // we have a single winmd for Windows MIDI Services

    return S_OK;

    //HRESULT hr = TrueRoResolveNamespace(
    //    name,
    //    pathReference.Get(),
    //    ARRAYSIZE(packageGraphDirectories),
    //    packageGraphDirectories,
    //    metaDataFilePathsCount,
    //    metaDataFilePaths,
    //    subNamespacesCount,
    //    subNamespaces);

    //return hr;
}

wil::critical_section m_detourSetupLock;
bool g_detourActive{ false };

HRESULT 
InstallWinRTActivationHooks()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto lock = m_detourSetupLock.lock();

    if (!g_detourActive)
    {
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

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(nullptr, "this"),
            TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        g_detourActive = true;
    }

    return S_OK;
}

void RemoveWinRTActivationHooks()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto lock = m_detourSetupLock.lock();

    if (g_detourActive)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)TrueRoActivateInstance, RoActivateInstanceDetour);
        DetourDetach(&(PVOID&)TrueRoGetActivationFactory, RoGetActivationFactoryDetour);
        DetourDetach(&(PVOID&)TrueRoGetMetaDataFile, RoGetMetaDataFileDetour);
        DetourDetach(&(PVOID&)TrueRoResolveNamespace, RoResolveNamespaceDetour);
        DetourTransactionCommit();

        g_detourActive = false;
    }

    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );
}

//extern "C" void WINAPI winrtact_Initialize()
//{
//    return;
//}
