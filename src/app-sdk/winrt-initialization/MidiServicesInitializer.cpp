// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiServicesInitializer.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization::implementation
{


    foundation::Uri MidiServicesInitializer::GetLatestRuntimeReleaseInstallerUri()
    {
        foundation::Uri installerUri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller");

        return installerUri;
    }

    foundation::Uri MidiServicesInitializer::GetLatestSettingsAppReleaseInstallerUri()
    {
        foundation::Uri installerUri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller");

        return installerUri;
    }

    foundation::Uri MidiServicesInitializer::GetLatestConsoleAppReleaseInstallerUri()
    {
        foundation::Uri installerUri(L"https://aka.ms/MidiServicesLatestConsoleInstaller");

        return installerUri;
    }



    bool MidiServicesInitializer::IsCompatibleDesktopAppSdkRuntimeInstalled()
    {
        // get location of runtime. If the reg key isn't present, return failure

        // TODO

        return false;
    }

    bool MidiServicesInitializer::IsOperatingSystemSupported()
    {

        // TODO

        return false;
    }


    bool MidiServicesInitializer::InitializeSdkRuntime()
    {
        if (!IsCompatibleDesktopAppSdkRuntimeInstalled())
        {
            return false;
        }

        // TODO: set up the type resolver, detours, etc.
        // Note: May need to change something about detours to support Arm64EC. TBD.





        return false;
    }

    // returns True if the MIDI Service is available on this PC
    bool MidiServicesInitializer::EnsureServiceAvailable()
    {
        try
        {
            auto serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            // winrt::try_create_instance indicates failure by returning an empty com ptr
            // winrt::create_instance indicates failure with an exception
            if (serviceAbstraction == nullptr)
            {
                LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error contacting service. Service abstraction is nullptr", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );                
                
                return false;
            }

            winrt::com_ptr<IMidiSessionTracker> tracker;

            auto sessionTrackerResult = serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&tracker);
            if (FAILED(sessionTrackerResult))
            {
                LOG_IF_FAILED(sessionTrackerResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failure hresult received activating interface", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(sessionTrackerResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            auto verifyConnectivityResult = tracker->VerifyConnectivity();
            if (FAILED(verifyConnectivityResult))
            {
                LOG_IF_FAILED(verifyConnectivityResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failure hresult received verifying connectivity", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(verifyConnectivityResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            return true;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error contacting service. It may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );


            // winrt::create_instance fails by throwing an exception
            return false;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error contacting service. It may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // winrt::create_instance fails by throwing an exception
            return false;
        }
    }
}
