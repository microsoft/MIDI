// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiInputEndpointConnection.h"
#include "MidiInputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    IFACEMETHODIMP MidiInputEndpointConnection::Callback(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Timestamp)
    {
        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<MidiMessageReceivedEventArgs>(Data, Size, Timestamp);

            // we failed to create the event args
            if (args == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs");

                return E_FAIL;
            }

            bool skipMainMessageReceivedEvent = false;
            bool skipFurtherListeners = false;

            // If any listeners are hooked up, use them

            if (m_messageListeners && m_messageListeners.Size() > 0)
            {
                // loop through listeners
                for (const auto& listener : m_messageListeners)
                {
                    // TODO: this is synchronous by design, but that requires the client to not block
                    listener.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);

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
            internal::LogHresultError(__FUNCTION__, L"hresult exception calling message handlers", ex);

            return E_FAIL;
        }
    }

    _Success_(return == true)
    bool MidiInputEndpointConnection::ActivateMidiStream(
            _In_ com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ const IID & iid,
            _Out_ void** iface)
    {
        try
        {
            winrt::check_hresult(serviceAbstraction->Activate(iid, iface));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception activating stream. Service may be unavailable", ex);

            return false;
        }
    }

    _Success_(return == true)
    bool MidiInputEndpointConnection::InternalInitialize(
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction)
    {
        try
        {
            WINRT_ASSERT(!DeviceId().empty());
            WINRT_ASSERT(serviceAbstraction != nullptr);

            m_serviceAbstraction = serviceAbstraction;

            // TODO: Read any settings we need for this endpoint

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint.", ex);

            return false;
        }
    }


    _Success_(return == true)
    bool MidiInputEndpointConnection::Open()
    {
        if (!m_isOpen)
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiIn), (void**)&m_endpointInterface))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                return false;
            }

            if (m_endpointInterface != nullptr)
            {
                try
                {
                    DWORD mmcssTaskId{};  // TODO: Does this need to be session-wide? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

                    winrt::check_hresult(m_endpointInterface->Initialize(
                        (LPCWSTR)(DeviceId().c_str()),
                        &mmcssTaskId,
                        (IMidiCallback*)(this)
                    ));

                    m_isOpen = true;


                    // TODO: Send discovery messages using app provided settings and user settings read from the property store
                    // These get fired off here quickly so we can return. The listener is responsible for catching them.







                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_endpointInterface = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" Endpoint interface is nullptr");

                return false;

            }


        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }


    MidiInputEndpointConnection::~MidiInputEndpointConnection()
    {
        if (m_endpointInterface != nullptr)
        {
            m_endpointInterface->Cleanup();
        }

        m_isOpen = false;

        // TODO: any event cleanup?
    }

}
