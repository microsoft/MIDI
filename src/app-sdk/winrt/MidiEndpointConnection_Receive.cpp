// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

#include "ump_iterator.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    HRESULT MidiEndpointConnection::Callback(MessageOptionFlags optionFlags, PVOID data, UINT size, LONGLONG timestamp, LONGLONG context)
    {
        UNREFERENCED_PARAMETER(context);
        UNREFERENCED_PARAMETER(optionFlags);


        // this is for the COM extensions approach to receiving messages. It's a fast exit.
        if (m_comCallback != nullptr)
        {
            if (size > 0)
            {
                // when you use the COM extensions, we bypass all other processing of incoming messages
                return m_comCallback->MessagesReceived(
                    m_sessionId,
                    m_connectionId,
                    static_cast<ULONGLONG>(timestamp),
                    size / sizeof(uint32_t),
                    static_cast<UINT32*>(data)
                );
            }
            else
            {
                // should never have a size of 0, but guarding just in case.
                return E_INVALIDARG;
            }
        }



        if ((!m_messageReceivedEvent) && (!m_messageProcessingPlugins || m_messageProcessingPlugins.Size() == 0))
        {
#ifdef _DEBUG
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exiting quickly due to no listeners", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
            );
#endif

            // fast exit if there's nothing listening
            return S_OK;
        }

#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
        );
#endif


        try
        {
            // Use the midi message iterator to loop through messages here. We raise the event for each message.
            // If someone wants the raw buffer, there's a COM interface for that.

            internal::UmpBufferIterator iterator(
                static_cast<uint32_t*>(data), 
                size / sizeof(uint32_t) );

            for (auto it = iterator.begin(); it < iterator.end(); ++it)
            {
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

                    RETURN_IF_FAILED(E_POINTER);
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

                        plugin.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);

                        // if the listener has told us to skip further listeners, effectively 
                        // removing this message from the queue, then break out of the loop
                        if (skipFurtherListeners) break;
                    }
                }

                // if the main message received event is hooked up, and we're not skipping it, use it
                if (m_messageReceivedEvent && !skipMainMessageReceivedEvent)
                {
                    m_messageReceivedEvent(*this, *args);
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