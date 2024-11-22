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
CMidiClientInitializer::Initialize(
)
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (DisableThreadLibraryCalls(_AtlModule.HModuleInstance))
    {
        g_runtimeComponentCatalog = std::make_shared<MidiAppSdkRuntimeComponentCatalog>();
        RETURN_HR_IF_NULL(E_POINTER, g_runtimeComponentCatalog);

        RETURN_IF_FAILED(g_runtimeComponentCatalog->Initialize());

        try
        {
            RETURN_IF_FAILED(InstallHooks());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return E_FAIL;
        }

        // TODO: set up the type resolver, detours, etc.
        // Note: May need to change something about detours to support Arm64EC. TBD.

        return S_OK;
    }
    else
    {
        // unable to disable thread library calls
        return E_FAIL;
    }
}



_Use_decl_annotations_
HRESULT 
CMidiClientInitializer::GetInstalledWindowsMidiServicesSdkVersion(
    MidiAppSDKPlatform* buildPlatform,
    DWORD* versionMajor,
    DWORD* versionMinor,
    DWORD* versionRevision,
    DWORD* versionDateNumber,
    DWORD* versionTimeNumber,
    LPWSTR* buildSource,
    LPWSTR* versionName,
    LPWSTR* versionFullString
)
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
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
CMidiClientInitializer::EnsureServiceAvailable()
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
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

    if (sessionTracker->VerifyConnectivity())
    {
        return S_OK;
    }

    return E_FAIL;
}


HRESULT
CMidiClientInitializer::Shutdown()
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // Remove activation hooks
    RemoveHooks();


    if (g_runtimeComponentCatalog != nullptr)
    {
        g_runtimeComponentCatalog->Shutdown();
        g_runtimeComponentCatalog.reset();
    }

    return S_OK;
}

