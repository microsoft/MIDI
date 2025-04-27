// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{

    void MidiEndpointConnection::InitializePlugins() noexcept
    {
        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Initialize(*this);
            }
            catch (...)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Exception initializing plugins.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

            }
        }
    }


    void MidiEndpointConnection::CallOnConnectionOpenedOnPlugins() noexcept
    {
        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.OnEndpointConnectionOpened();
            }
            catch (...)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Exception calling Open on plugins.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );
            }
        }
    }

    void MidiEndpointConnection::CleanupPlugins() noexcept
    {
        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Cleanup();
            }
            catch (...)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Exception cleaning up plugins.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

            }
        }

        m_messageProcessingPlugins.Clear();
    }




    _Use_decl_annotations_
    void MidiEndpointConnection::AddMessageProcessingPlugin(midi2::IMidiEndpointMessageProcessingPlugin const& plugin)
    {
        m_messageProcessingPlugins.Append(plugin);

        try
        {
            plugin.Initialize(*this);

            // if this is added after we've already been opened, call the
            // handler anyway to get it ready.

            if (m_isOpen)
            {
                plugin.OnEndpointConnectionOpened();
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception initializing or calling OnEndpointConnectionOpened on newly-added plugin.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

        }
    }

    _Use_decl_annotations_
    void MidiEndpointConnection::RemoveMessageProcessingPlugin(winrt::guid pluginId)
    {
        for (uint32_t i = 0; i < m_messageProcessingPlugins.Size(); i++)
        {
            if (m_messageProcessingPlugins.GetAt(i).PluginId() == pluginId)
            {
                m_messageProcessingPlugins.RemoveAt(i);
                break;
            }
        }
    }



}
