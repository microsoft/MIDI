// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiService.h"
#include "MidiService.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    MidiApiMode MidiService::GetCurrentApiMode() noexcept
    {
        DWORD midiMode = MIDI_USE_MIDISRV;

        auto hr = wil::reg::get_value_nothrow<DWORD>(HKEY_CURRENT_USER, MIDI_DRIVERS32_REG_KEY, MIDI_USE_LEGACY_REG_KEY, &midiMode);

        if (FAILED(hr))
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"HRESULT indicates failure checking reg entry. It may not exist, which is ok.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(hr), MIDI_SDK_TRACE_HRESULT_FIELD)
            );

            // If the registry value doesn't exist or can't be read, default to full, which is also what the service code does.
            return MidiApiMode::FullWindowsMidiServicesMode;
        }

        switch (midiMode)
        {
        case MIDI_USE_LEGACY:
            return MidiApiMode::LegacyMode;
        case MIDI_USE_MIDISRV:
            return MidiApiMode::FullWindowsMidiServicesMode;
        case MIDI_USE_HYBRID_LEGACY:
            return MidiApiMode::HybridLegacyMode;
        default:
            return MidiApiMode::FullWindowsMidiServicesMode;        // todo: need to verify the service defaults to full if bad data

        }
        

    }

    bool MidiService::EnsureAvailable()
    {
        try
        {
            auto serviceTransport = winrt::create_instance<IMidiTransport>(__uuidof(Midi2MidiSrvTransport), CLSCTX_ALL);

            if (!serviceTransport)
            {
                return false;
            }

            winrt::com_ptr<IMidiSessionTracker> sessionTracker { nullptr };

            auto result = serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker);

            if (FAILED(result) || !sessionTracker)
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"HRESULT indicates failure checking connectivity to the service.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(static_cast<HRESULT>(result), MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            // this is what actually checks to see if we can talk to the service.
            return sessionTracker->VerifyConnectivity();
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
