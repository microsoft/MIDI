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
  //      internal::LogInfo(__FUNCTION__, L"Message Received in API callback");

        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<implementation::MidiMessageReceivedEventArgs>(data, size, timestamp);

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
            internal::LogHresultError(__FUNCTION__, L"hresult exception handling received messages", ex);

            return E_FAIL;
        }
        catch (std::exception const& ex)
        {
            internal::LogStandardExceptionError(__FUNCTION__, L"hresult exception handling received messages", ex);

            return E_FAIL;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception handling received message");

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
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (serviceAbstraction == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Error initializing endpoint. Service abstraction is null");

            return false;
        }

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
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (m_endpointAbstraction == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to open connection. endpoint abstraction is null");

            return false;
        }

        DWORD mmcssTaskId{};

        LPCWSTR connectionSettingsJsonString = nullptr;

        if (m_connectionSettings != nullptr)
        {
            internal::LogInfo(__FUNCTION__, L"Connection settings were specified. Including JSON in service call.");

            connectionSettingsJsonString = static_cast<LPCWSTR>(m_connectionSettings.SettingsJson().c_str());
        }

        ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ };
        abstractionCreationParams.DataFormat = MidiDataFormat_UMP; 
        abstractionCreationParams.InstanceConfigurationJsonData = connectionSettingsJsonString;

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
            internal::LogInfo(__FUNCTION__, L"Endpoint abstraction successfully initialized");

            m_isOpen = true;
            m_closeHasBeenCalled = false;
        }
        else
        {
            internal::LogHresultError(__FUNCTION__, L"Failed to open connection", result);
        }

        internal::LogInfo(__FUNCTION__, L"Connection Opened");

        return true;
    }

    // this does all the reconnection except for plugin init
    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalReopenAfterDisconnect()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (!ActivateMidiStream()) return false;

        return InternalOpen();
    }


    _Use_decl_annotations_
    bool MidiEndpointConnection::Open()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (!IsOpen())
        {
            if (!ActivateMidiStream()) return false;

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
                        internal::LogGeneralError(__FUNCTION__, L"InternalOpen() returned false.");

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
            internal::LogInfo(__FUNCTION__, L"Connection already open");

            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }



    void MidiEndpointConnection::InternalClose()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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

        internal::LogInfo(__FUNCTION__, L"Complete");
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
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (m_endpointAbstraction != nullptr)
        {
            // todo: some error / hresult handling here

            auto hr = m_endpointAbstraction->Cleanup();

            if (FAILED(hr))
            {
                internal::LogHresultError(__FUNCTION__, L"Failed HRESULT cleaning up endpoint abstraction", hr);

                return false;
            }

            m_endpointAbstraction == nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"Stream deactivated");

        return true;
    }

    
    _Use_decl_annotations_
    bool MidiEndpointConnection::ActivateMidiStream() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (m_serviceAbstraction == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Service abstraction is null.");

            return false;
        }

        try
        {
            winrt::check_hresult(m_serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**) &m_endpointAbstraction));

            if (m_endpointAbstraction == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint Abstraction failed to Activate and return null.");

                return false;
            }

            internal::LogInfo(__FUNCTION__, L"Activated IMidiBiDi");

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
