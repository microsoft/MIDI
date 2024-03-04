// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"
#include "MidiEndpointConnection.g.cpp"




namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    HRESULT MidiEndpointConnection::Callback(PVOID data, UINT size, LONGLONG timestamp, LONGLONG)
    {
        internal::LogInfo(__FUNCTION__, L"Message Received");

        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<midi2::implementation::MidiMessageReceivedEventArgs>(data, size, timestamp);

            // we failed to create the event args
            if (args == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs");

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
            internal::LogHresultError(__FUNCTION__, L"hresult exception calling message handlers", ex);

            return E_FAIL;
        }
    }



    
    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalInitialize(
        winrt::guid sessionId,
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        winrt::guid const connectionId,
        winrt::hstring const endpointDeviceId
    )
    {
        internal::LogInfo(__FUNCTION__, L"Internal Initialize ");

        try
        {
            m_sessionId = sessionId;
            m_connectionId = connectionId;

            m_endpointDeviceId = endpointDeviceId;

            //WINRT_ASSERT(!m_endpointDeviceId.empty());
            //WINRT_ASSERT(serviceAbstraction != nullptr);

            m_serviceAbstraction = serviceAbstraction;
            
            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint.", ex);

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointConnection::Open()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Open ");

        if (!IsOpen())
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiBiDi), (void**)&m_endpointAbstraction))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                return false;
            }

            if (m_endpointAbstraction != nullptr)
            {
                try
                {
                    InitializePlugins();

                    DWORD mmcssTaskId{};  
                    ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP };

                    winrt::check_hresult(m_endpointAbstraction->Initialize(
                        (LPCWSTR)(EndpointDeviceId().c_str()),
                        &abstractionCreationParams,
                        &mmcssTaskId,
                        (IMidiCallback*)(this),
                        0,
                        m_sessionId
                    ));

                    // provide a copy to the output logic
                    m_isOpen = true;

                    CallOnConnectionOpenedOnPlugins();

                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_endpointAbstraction = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint interface is nullptr");

                return false;
            }
        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }



    void MidiEndpointConnection::InternalClose()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Close");

        if (m_closeHasBeenCalled) return;

        try
        {
            CleanupPlugins();

            if (m_endpointAbstraction != nullptr)
            {
                m_endpointAbstraction->Cleanup();
                m_endpointAbstraction = nullptr;
            }

            // output is also input, so don't call cleanup
            m_endpointAbstraction = nullptr;

            m_isOpen = false;

            // TODO: any event cleanup?

            m_closeHasBeenCalled = true;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception on close");
        }
    }

    MidiEndpointConnection::~MidiEndpointConnection()
    {
        if (!m_closeHasBeenCalled)
        {
            InternalClose();
        }
    }




    _Use_decl_annotations_
    bool MidiEndpointConnection::ActivateMidiStream(
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        const IID& iid,
        void** iface) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Activating MIDI Stream");

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
}
