// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"


// init code goes in dllmain

winrt::com_ptr<IMidiClientInitializer> g_clientInitializer{ nullptr };



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

    if (m_initialized)
    {
        return S_OK;
    }

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

        RETURN_IF_FAILED(E_POINTER);
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

    m_initialized = true;

    return S_OK;
}



_Use_decl_annotations_
HRESULT 
MidiClientInitializer::GetInstalledWindowsMidiServicesSdkVersion(
    MidiAppSDKPlatform* buildPlatform,
    DWORD* versionMajor,
    DWORD* versionMinor,
    DWORD* versionRevision,
    DWORD* versionDateNumber,
    DWORD* versionTimeNumber,
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

    try
    {
        if (versionMajor != nullptr)
        {
            std::wstring versionMajorTempString{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_MAJOR };

            if (!versionMajorTempString.empty())
            {
                *versionMajor = static_cast<DWORD>(std::stol(versionMajorTempString));
            }
            else
            {
                *versionMajor = 0;
            }
        }

        if (versionMinor != nullptr)
        {
            std::wstring versionMinorTempString{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_MINOR };

            if (!versionMinorTempString.empty())
            {
                *versionMinor = static_cast<DWORD>(std::stol(versionMinorTempString));
            }
            else
            {
                *versionMinor = 0;
            }
        }

        if (versionRevision != nullptr)
        {
            std::wstring versionRevisionTempString{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_REVISION };

            if (!versionRevisionTempString.empty())
            {
                *versionRevision = static_cast<DWORD>(std::stol(versionRevisionTempString));
            }
            else
            {
                *versionRevision = 0;
            }
        }

        if (versionDateNumber != nullptr)
        {
            std::wstring versionDateNumberTempString{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_DATE_NUMBER };

            if (!versionDateNumberTempString.empty())
            {
                *versionDateNumber = static_cast<DWORD>(std::stol(versionDateNumberTempString));
            }
            else
            {
                *versionDateNumber = 0;
            }
        }

        if (versionTimeNumber != nullptr)
        {
            std::wstring versionTimeNumberTempString{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_TIME_NUMBER };

            if (!versionTimeNumberTempString.empty())
            {
                *versionTimeNumber = static_cast<DWORD>(std::stol(versionTimeNumberTempString));
            }
            else
            {
                *versionTimeNumber = 0;
            }
        }

        if (buildSource != nullptr)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(WINDOWS_MIDI_SERVICES_BUILD_SOURCE);
            RETURN_IF_NULL_ALLOC(tempString.get());
            *buildSource = static_cast<LPWSTR>(tempString.release());
        }

        if (versionName != nullptr)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(WINDOWS_MIDI_SERVICES_BUILD_VERSION_NAME);
            RETURN_IF_NULL_ALLOC(tempString.get());
            *versionName = static_cast<LPWSTR>(tempString.release());
        }

        if (versionFullString != nullptr)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(WINDOWS_MIDI_SERVICES_BUILD_VERSION_FULL);
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

    wil::com_ptr_nothrow<IMidiTransport> serviceTransport;
    wil::com_ptr_nothrow<IMidiSessionTracker> sessionTracker;

    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceTransport)));
    RETURN_IF_FAILED(serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker));

    RETURN_HR_IF_NULL(E_POINTER, sessionTracker);

    RETURN_IF_FAILED(sessionTracker->Initialize());

    if (sessionTracker->VerifyConnectivity())
    {
        return S_OK;
    }

    return E_FAIL;
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


    // Remove activation hooks
    RemoveWinRTActivationHooks();


    if (g_runtimeComponentCatalog != nullptr)
    {
        g_runtimeComponentCatalog->Shutdown();
        g_runtimeComponentCatalog.reset();
    }

    return S_OK;
}

