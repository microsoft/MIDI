// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    void MidiEndpointConnection::InitializePlugins() noexcept
    {
        // ensure plugins are not added or removed while we are iterating through them
        std::lock_guard<std::mutex> guard(m_messageProcessingPluginsLock);

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Initialize(*this);
            }
            catch (winrt::hresult_error const& ex)
            {
                MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error Initializing plugins.");
            }
            catch (...)
            {
                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception Initializing plugins.");
            }
        }
    }


    void MidiEndpointConnection::CallOnConnectionOpenedOnPlugins() noexcept
    {
        // ensure plugins are not added or removed while we are iterating through them
        std::lock_guard<std::mutex> guard(m_messageProcessingPluginsLock);

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.OnEndpointConnectionOpened();
            }
            catch (winrt::hresult_error const& ex)
            {
                MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error calling OnEndpointConnectionOpened on plugins.");
            }
            catch (...)
            {
                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception calling OnEndpointConnectionOpened on plugins.");
            }
        }
    }

    void MidiEndpointConnection::CleanupPlugins() noexcept
    {
        // ensure plugins are not added or removed while we are iterating through them
        std::lock_guard<std::mutex> guard(m_messageProcessingPluginsLock);

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Cleanup();
            }
            catch (winrt::hresult_error const& ex)
            {
                MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error cleaning up plugins.");
            }
            catch (...)
            {
                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception cleaning up plugins.");
            }
        }

        m_messageProcessingPlugins.Clear();
    }




    _Use_decl_annotations_
    midi2::MidiMessageProcessingPluginAddResult MidiEndpointConnection::AddMessageProcessingPlugin(midi2::IMidiEndpointMessageProcessingPlugin const& plugin)
    {
        if (plugin == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Message processing plugin was null.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
            );

            return midi2::MidiMessageProcessingPluginAddResult::FailedPluginIsNull;
        }

        // SetMessagesReceivedCallback needs both of these in the opposite order, so acquire
        // them together rather than nesting one inside the other.
        std::scoped_lock guard(m_messageProcessingPluginsLock, m_comCallbackLock);

        if (m_comCallback != nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Cannot add a message processing plugin when a COM extensions messages received callback is registered. The plugin would never be called.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                TraceLoggingGuid(plugin.PluginId(), "PluginId")
            );

            OutputDebugString(L"MIDI App SDK: AddMessageProcessingPlugin failed. A COM extensions messages received callback is registered on this connection, and it bypasses all message processing plugins.\n");

            return midi2::MidiMessageProcessingPluginAddResult::FailedRawCallbackRegistered;
        }

        for (uint32_t i = 0; i < m_messageProcessingPlugins.Size(); i++)
        {
            if (m_messageProcessingPlugins.GetAt(i).PluginId() == plugin.PluginId())
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"This message processing plugin has already been added to this connection.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                    TraceLoggingGuid(plugin.PluginId(), "PluginId")
                );

                return midi2::MidiMessageProcessingPluginAddResult::FailedPluginAlreadyAdded;
            }
        }

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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error initializing or calling OnEndpointConnectionOpened on newly-added plugin.");

            return midi2::MidiMessageProcessingPluginAddResult::FailedPluginInitializationError;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception initializing or calling OnEndpointConnectionOpened on newly-added plugin.");

            return midi2::MidiMessageProcessingPluginAddResult::FailedPluginInitializationError;
        }

        return midi2::MidiMessageProcessingPluginAddResult::Succeeded;
    }

    _Use_decl_annotations_
    void MidiEndpointConnection::RemoveMessageProcessingPlugin(winrt::guid pluginId)
    {
        std::lock_guard<std::mutex> guard(m_messageProcessingPluginsLock);

        for (uint32_t i = 0; i < m_messageProcessingPlugins.Size(); i++)
        {
            if (m_messageProcessingPlugins.GetAt(i).PluginId() == pluginId)
            {
                try
                {
                    m_messageProcessingPlugins.GetAt(i).Cleanup();
                }
                catch (winrt::hresult_error const& ex)
                {
                    MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error cleaning up message processing plugin.");
                }
                catch (...)
                {
                    MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception cleaning up message processing plugin.");
                }

                m_messageProcessingPlugins.RemoveAt(i);
                break;
            }
        }
    }



}
