// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiClientInitializer.h"


HRESULT
MidiClientInitializer::Initialize(
) noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // initialize can only be called once so let it finish at least one time.
    auto lock = m_initializeLock.lock();

    if (m_initialized)
    {
        return S_OK;
    }

    auto cleanupOnError = wil::scope_exit([&]()
        {
            if (g_runtimeComponentCatalog)
            {
                g_runtimeComponentCatalog->Shutdown();
                g_runtimeComponentCatalog = nullptr;
            }

            RemoveWinRTActivationHooks();
            m_initialized = false;
        });


    // SDK initialization
    g_runtimeComponentCatalog = std::make_shared<MidiAppSdkRuntimeComponentCatalog>();
    if (g_runtimeComponentCatalog == nullptr)
    {
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(nullptr, "this"),
            TraceLoggingWideString(L"Unable to create runtime component catalog", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_NULL_ALLOC(g_runtimeComponentCatalog);
    }

    auto hrcatalog = g_runtimeComponentCatalog->Initialize();
    if (!SUCCEEDED(hrcatalog))
    {
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(nullptr, "this"),
            TraceLoggingWideString(L"Unable to initialize runtime component catalog", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_FAIL);
    }


    try
    {
        auto hr = InstallWinRTActivationHooks();

        if (!SUCCEEDED(hr))
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, "this"),
                TraceLoggingWideString(L"Unable to install WinRT activation hooks", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );
            RETURN_IF_FAILED(hr);
        }

        m_initialized = true;

        // success, so remove all the unwinding code
        cleanupOnError.release();

        return S_OK;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(nullptr, "this"),
            TraceLoggingWideString(L"Exception installing WinRT activation hooks", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_FAIL);
    }

}



_Use_decl_annotations_
HRESULT 
MidiClientInitializer::GetInstalledWindowsMidiServicesSdkVersion(
    MidiAppSDKPlatform* buildPlatform,
    USHORT* versionMajor,
    USHORT* versionMinor,
    USHORT* versionPatch,
    LPWSTR* buildSource,
    LPWSTR* versionName,
    LPWSTR* versionFullString
) noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);

    try
    {
        if (versionMajor != nullptr)
        {
            *versionMajor = static_cast<DWORD>(WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_MAJOR);
        }

        if (versionMinor != nullptr)
        {
            *versionMinor = static_cast<DWORD>(WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_MINOR);
        }

        if (versionPatch != nullptr)
        {
            *versionPatch = static_cast<DWORD>(WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_PATCH);
        }

        if (buildSource != nullptr)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_SOURCE);
            RETURN_IF_NULL_ALLOC(tempString.get());
            *buildSource = static_cast<LPWSTR>(tempString.release());
        }

        if (versionName != nullptr)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_NAME);
            RETURN_IF_NULL_ALLOC(tempString.get());
            *versionName = static_cast<LPWSTR>(tempString.release());
        }

        if (versionFullString != nullptr)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_FULL);
            RETURN_IF_NULL_ALLOC(tempString.get());
            *versionFullString = static_cast<LPWSTR>(tempString.release());
        }

        if (buildPlatform != nullptr)
        {
#if defined(_M_ARM64EC) || defined(_M_ARM64)
            // Arm64 and Arm64EC use same Arm64X binaries
            *buildPlatform = MidiAppSDKPlatform::Platform_Arm64X;
#elif defined(_M_AMD64)
            // other 64 bit Intel/AMD
            *buildPlatform = MidiAppSDKPlatform::Platform_x64;
#else
            // unsupported compile target architecture
#endif
        }

        return S_OK;
    }
    catch (std::invalid_argument ex)
    {
        // if any of the numeric values is not a number
        return E_FAIL;
    }
    catch (std::out_of_range ex)
    {
        // if someone has enormous numbers in the strings
        return E_FAIL;
    }


}


HRESULT
MidiClientInitializer::EnsureServiceAvailable() noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);


    // midisrv client initialization. Doesn't actually connect to service here
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_serviceTransport)));
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_serviceTransport);

    wil::com_ptr_nothrow<IMidiSessionTracker> sessionTracker;
    RETURN_IF_FAILED(m_serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker));
    RETURN_HR_IF_NULL(E_POINTER, sessionTracker);

    RETURN_IF_FAILED(sessionTracker->Initialize());
    bool connected = sessionTracker->VerifyConnectivity();
    RETURN_IF_FAILED(sessionTracker->Shutdown());

    sessionTracker.reset();

    if (connected)
    {
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

bool
MidiClientInitializer::CanUnloadNow() noexcept
{
    // if we've been initialized, the detours are in place, so cannot unload
    if (m_initialized)
    {
        return false;
    }

    return true;
}



HRESULT
MidiClientInitializer::Shutdown() noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // there are still references being held. Don't shut down detours
    if (m_cRef > 0)
    {
        return S_OK;
    }


    auto lock = m_initializeLock.lock();

    if (m_initialized)
    {
        // Remove activation hooks
        RemoveWinRTActivationHooks();

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Setting transport to null", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        if (m_serviceTransport)
        {
            m_serviceTransport.reset();
        }

        if (g_runtimeComponentCatalog != nullptr)
        {
            g_runtimeComponentCatalog->Shutdown();
            g_runtimeComponentCatalog.reset();
        }

        m_initialized = false;

        // this just feels strange
        g_midiClientInitializer.Reset();
    }

    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Done", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


MidiClientInitializer::MidiClientInitializer()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    Initialize();
}


MidiClientInitializer::~MidiClientInitializer()
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    Shutdown();
}


ULONG __stdcall MidiClientInitializer::AddRef() noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return InterlockedIncrement(&m_cRef);
}

ULONG __stdcall MidiClientInitializer::Release() noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (InterlockedDecrement(&m_cRef) == 0)
    {
        //delete this;
        Shutdown();
        return 0;
    }

    return m_cRef;
}

HRESULT __stdcall MidiClientInitializer::QueryInterface(const IID& iid, void** ppv) noexcept
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (iid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (iid == __uuidof(IMidiClientInitializer))
    {
        *ppv = static_cast<IMidiClientInitializer*>(this);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    //reinterpret_cast<IUnknown*>(*ppv)->AddRef();

    return S_OK;
}
