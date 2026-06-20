// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiApi.h"
#include "MidiApi.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    MidiApiMode MidiApi::GetCurrentApiMode() noexcept
    {
        DWORD midiMode = MIDI_USE_MIDISRV;

        auto hr = wil::reg::get_value_nothrow<DWORD>(HKEY_CURRENT_USER, MIDI_DRIVERS32_REG_KEY, MIDI_USE_LEGACY_REG_KEY, &midiMode);

        if (FAILED(hr))
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"HRESULT indicates failure checking API Mode reg entry. It may not exist, which is ok. Defaulting to full MIDI Service mode.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(hr), MIDI_SDK_TRACE_HRESULT_FIELD)
            );

            // If the registry value doesn't exist or can't be read, default to full, which is also what the service code does.
            return MidiApiMode::FullWindowsMidiServicesMode;
        }

        switch (midiMode)
        {
        case MIDI_USE_LEGACY:
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Windows MIDI Services is in Legacy-only mode. No service.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
            return MidiApiMode::LegacyMode;

        case MIDI_USE_MIDISRV:
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Windows MIDI Services is in Full MIDI Service mode.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
            return MidiApiMode::FullWindowsMidiServicesMode;

        case MIDI_USE_HYBRID_LEGACY:
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Windows MIDI Services is in hybrid MIDI Service mode.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
            return MidiApiMode::HybridLegacyMode;

        default:
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Windows MIDI Services unknown mode. Defaulting to Full MIDI Service mode.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
            return MidiApiMode::FullWindowsMidiServicesMode;        // todo: need to verify the service defaults to full if bad data
        }
        

    }



    bool MidiApi::EnsureServiceAvailable() noexcept
    {
        // short-circuit if we're on legacy mode
        if (GetCurrentApiMode() == MidiApiMode::LegacyMode)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"EnsureServiceAvailable called when Windows MIDI Services is in legacy-only mode.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        try
        {
            // if this fails, then the MIDI API is not available.
            wil::com_ptr_nothrow<IMidiTransport> serviceTransport;
            auto transportResult = CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceTransport));

            if (FAILED(transportResult) || serviceTransport == nullptr)
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"HRESULT indicates failure attempting to create Midi2MidiSrvTransport or serviceTransport is null.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(static_cast<HRESULT>(transportResult), MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            wil::com_ptr_nothrow<IMidiSessionTracker> sessionTracker;
            auto sessionTrackerResult = serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker);

            if (FAILED(sessionTrackerResult) || sessionTracker == nullptr)
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"HRESULT indicates failure attempting to activate IMidiSessionTracker or sessionTracker is null.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(static_cast<HRESULT>(sessionTrackerResult), MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            // this is what actually checks to see if we can talk to the service. It will trigger start the service when called
            // return result is Win32 BOOL
            auto serviceAvailable = sessionTracker->VerifyConnectivity();

            if (serviceAvailable)
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"MIDI Service is available.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return true;
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"MIDI Service is not available.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"HRESULT error checking connectivity to the service.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

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
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"An unknown error occurred checking connectivity to the service. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }
    }
}
