// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"


namespace MIDI_CPP_NAMESPACE::implementation
{
    _Use_decl_annotations_
        HRESULT MidiEndpointConnection::Callback(PVOID data, UINT size, LONGLONG timestamp, LONGLONG)
    {
        //      internal::LogInfo(__FUNCTION__, L"Message Received in API callback");

        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<implementation::MidiMessageReceivedEventArgs>(data, size, timestamp);

            // we failed to create the event args
            if (args == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs", m_endpointDeviceId);

                return E_FAIL;
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

            return S_OK;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception handling received messages", ex, m_endpointDeviceId);

            return E_FAIL;
        }
        catch (std::exception const& ex)
        {
            internal::LogStandardExceptionError(__FUNCTION__, L"hresult exception handling received messages", ex, m_endpointDeviceId);

            return E_FAIL;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception handling received message", m_endpointDeviceId);

            return E_FAIL;
        }
    }

}