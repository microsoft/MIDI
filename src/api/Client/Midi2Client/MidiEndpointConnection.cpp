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




namespace MIDI_CPP_NAMESPACE::implementation
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
        winrt::hstring const endpointDeviceId,
        midi2::IMidiEndpointConnectionSettings connectionSettings,
        bool autoReconnect
    )
    {
        internal::LogInfo(__FUNCTION__, L"Internal Initialize ");

        try
        {
            m_sessionId = sessionId;
            m_connectionId = connectionId;

            m_endpointDeviceId = endpointDeviceId;

            m_serviceAbstraction = serviceAbstraction;

            m_connectionSettings = connectionSettings;
            m_autoReconnect = autoReconnect;

            
            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint.", ex);

            return false;
        }
    }

    // this does all the connection except plugin init
    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalOpen()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Open ");


        DWORD mmcssTaskId{};

        LPCWSTR connectionSettingsJsonString = nullptr;

        if (m_connectionSettings != nullptr)
        {
            connectionSettingsJsonString = m_connectionSettings.SettingsJson().c_str();
        }

        ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP, connectionSettingsJsonString };

        auto result = m_endpointAbstraction->Initialize(
            (LPCWSTR)(EndpointDeviceId().c_str()),
            &abstractionCreationParams,
            &mmcssTaskId,
            (IMidiCallback*)(this),
            0,
            m_sessionId
        );

        if (SUCCEEDED(result))
        {
            m_isOpen = true;
            m_closeHasBeenCalled = false;
        }
        else
        {
            internal::LogHresultError(__FUNCTION__, L"Failed to open connection", result);
        }

        return true;
    }

    // this does all the reconnection except for plugin init
    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalReopenAfterDisconnect()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Reopen ");

        // Activate the endpoint for this device. Will fail if the device is not a BiDi device
        if (!ActivateMidiStream())
        {
            internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

            return false;
        }

        return InternalOpen();
    }


    _Use_decl_annotations_
    bool MidiEndpointConnection::Open()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Open ");

        // Activate the endpoint for this device. Will fail if the device is not a BiDi device
        if (!ActivateMidiStream())
        {
            internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

            return false;
        }


        if (!IsOpen())
        {
            if (m_endpointAbstraction != nullptr)
            {
                try
                {
                    InitializePlugins();

                    if (InternalOpen())
                    {
                        // If autoReconnect, set up a watcher
                        if (m_autoReconnect)
                        {
                            StartDeviceWatcher();
                        }

                        CallOnConnectionOpenedOnPlugins();

                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_endpointAbstraction = nullptr;

                    return false;
                }
                catch (...)
                {
                    internal::LogGeneralError(__FUNCTION__, L"Exception initializing endpoint interface. Service may be unavailable.");

                    m_endpointAbstraction = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint abstraction interface is nullptr");

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
            DeactivateMidiStream();

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
    bool MidiEndpointConnection::DeactivateMidiStream()
    {
        internal::LogInfo(__FUNCTION__, L"Deactivating MIDI Stream");

        if (m_endpointAbstraction != nullptr)
        {
            // todo: some error / hresult handling here

            m_endpointAbstraction->Cleanup();
            m_endpointAbstraction == nullptr;
        }

        return true;
    }

    
    _Use_decl_annotations_
    bool MidiEndpointConnection::ActivateMidiStream() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Activating MIDI Stream");

        try
        {
            winrt::check_hresult(m_serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**) &m_endpointAbstraction));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception activating stream. Service may be unavailable", ex);

            return false;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception activating stream. Service may be unavailable");

            return false;
        }
    }
}
