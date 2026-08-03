// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

#include "ump_iterator.h"

#include "MidiMessageReceivedEventArgs.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    HRESULT MidiEndpointConnection::Callback(MessageOptionFlags /*optionFlags*/, PVOID data, UINT size, LONGLONG timestamp, LONGLONG /*context*/)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, data);
        RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

        // COM Extension Handling ===================================================================================
        {
            std::lock_guard<std::mutex> guard(m_comCallbackLock);

            // this is for the COM extensions approach to receiving messages. It's a fast exit.
            if (m_comCallback != nullptr)
            {
                // when you use the COM extensions, we bypass all other processing of incoming messages
                RETURN_IF_FAILED(m_comCallback->MessagesReceived(
                    m_sessionId,
                    m_connectionId,
                    static_cast<ULONGLONG>(timestamp),
                    size / sizeof(uint32_t),
                    static_cast<UINT32*>(data))
                );

                return S_OK;
            }
        }

        // Event-based message Handling ===================================================================================

        if ((!m_messageReceivedEvent) && (!m_messageProcessingPlugins || m_messageProcessingPlugins.Size() == 0))
        {
            // fast exit if there's nothing listening
            return S_OK;
        }

        try
        {
            // Use the midi message iterator to loop through messages here. We raise the event for each message.
            // If someone wants the raw buffer with multiple messages, there's the COM extensions interface for that.

            internal::UmpBufferIterator iterator(
                static_cast<uint32_t*>(data), 
                size / sizeof(uint32_t) );

            for (auto it = iterator.begin(); it < iterator.end(); ++it)
            {
                // Guard against a malformed/truncated trailing message. CurrentMessageWordCount()
                // is derived from word0 and can claim more words than remain in the buffer; without
                // this check the event args constructor would memcpy past the end of the buffer.
                if (!it.CurrentMessageSeemsComplete())
                {
                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Incomplete/truncated message at end of received buffer. Stopping processing.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
                    );

                    OutputDebugString(L"MIDI App SDK: Incomplete/truncated message at end of received buffer.\n");

                    break;
                }

                // one copy of the event args for this gets sent to all listeners and the main event
                auto args = winrt::make_self<implementation::MidiMessageReceivedEventArgs>(
                    static_cast<PVOID>(it.get()), 
                    static_cast<UINT>(it.CurrentMessageWordCount() * sizeof(uint32_t)), 
                    timestamp);

                // we failed to create the event args
                if (args == nullptr)
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Unable to create MidiMessageReceivedEventArgs", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                    );

                    OutputDebugString(L"MIDI App SDK: Unable to create MidiMessageReceivedEventArgs\n");

                    RETURN_IF_FAILED(E_OUTOFMEMORY);
                }

                bool skipMainMessageReceivedEvent = false;
                bool skipFurtherListeners = false;

                // If any listeners are hooked up, use them

                if (m_messageProcessingPlugins && m_messageProcessingPlugins.Size() > 0)
                {
                    // loop through listeners
                    for (const auto& plugin : m_messageProcessingPlugins)
                    {
                        // This is synchronous by design, but that requires the listener (and the client app which sinks any event) to not block

                        if (plugin.IsEnabled())
                        {
                            try
                            {
                                plugin.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);
                            }
                            catch (winrt::hresult_error const& ex)
                            {
                                MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error bubbled up through plugin ProcessIncomingMessage.");
                            }
                            catch (...)
                            {
                                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception bubbled up through plugin ProcessIncomingMessage.");
                            }

                        }

                        // if the listener has told us to skip further listeners, effectively 
                        // removing this message from the queue, then break out of the loop
                        if (skipFurtherListeners) break;
                    }
                }


                try
                {
                    // if the main message received event is hooked up, and we're not skipping it, use it
                    if (m_messageReceivedEvent && !skipMainMessageReceivedEvent)
                    {
                        m_messageReceivedEvent(*this, *args);
                    }
                }
                catch (winrt::hresult_error const& ex)
                {
                    MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error bubbled up through message received event.");
                }
                catch (...)
                {
                    MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception bubbled up through message received event.");
                }

            }

            return S_OK;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception handling received message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: HRESULT exception handling received message\n");

            return ex.code();
        }
        catch (std::exception const& ex)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"std::exception handling received message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingString(ex.what(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: std::exception handling received message\n");

            return E_FAIL;
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
                TraceLoggingWideString(L"General exception handling received message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: General exception handling received message\n");

            return E_FAIL;
        }
    }

}