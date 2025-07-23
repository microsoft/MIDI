// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"
#include "MidiEndpointConnection.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    winrt::hstring MidiEndpointConnection::ToString()
    {
        return ConnectedEndpointDeviceId();
    }

    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalInitialize(
        winrt::guid const& sessionId,
        winrt::com_ptr<IMidiTransport> serviceTransport,
        winrt::guid const& connectionId,
        winrt::hstring const& endpointDeviceId,
        midi2::IMidiEndpointConnectionSettings const& connectionSettings
    )
    {
        if (serviceTransport == nullptr)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error initializing endpoint. Service transport is null", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return false;
        }

        try
        {
            if (connectionSettings != nullptr)
            {
                m_connectionSettings = connectionSettings;
            }
            else
            {
                // no connection settings provided, so use basic defaults
                m_connectionSettings = winrt::make<MidiEndpointConnectionBasicSettings>();
            }


            m_sessionId = sessionId;
            m_connectionId = connectionId;
            m_endpointDeviceId = endpointDeviceId;

            m_serviceTransport = serviceTransport;

            return true;
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
                TraceLoggingWideString(L"hresult exception initializing endpoint.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
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
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception initializing endpoint interface. Service may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return false;
        }
    }

    // this does all the connection except plugin init
    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalOpen()
    {
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

        if (m_endpointTransport == nullptr)
        {
            LOG_IF_FAILED(E_POINTER);  // force reporting of file/line number

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to open connection. endpoint transport is null", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return false;
        }

        DWORD mmcssTaskId{};
        LPCWSTR connectionSettingsJsonString = nullptr;

        if (m_connectionSettings != nullptr)
        {
            connectionSettingsJsonString = static_cast<LPCWSTR>(m_connectionSettings.SettingsJson().c_str());
        }

        TRANSPORTCREATIONPARAMS transportCreationParams{ };
        transportCreationParams.DataFormat = MidiDataFormats::MidiDataFormats_UMP;
        //transportCreationParams.InstanceConfigurationJsonData = connectionSettingsJsonString;

        IMidiCallback* obj;
        this->QueryInterface(__uuidof(IMidiCallback), (void**)&obj);

        auto result = m_endpointTransport->Initialize(
            (LPCWSTR)(ConnectedEndpointDeviceId().c_str()),
            &transportCreationParams,
            &mmcssTaskId,
            obj,
            0,
            m_sessionId
        );

        if (SUCCEEDED(result))
        {
            m_isOpen = true;
            m_closeHasBeenCalled = false;

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"InternalOpen succeeded. Exit.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
            );

            return true;
        }
        else
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to initialize endpoint transport", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(result, MIDI_SDK_TRACE_HRESULT_FIELD)
            );

            return false;
        }

    }



    _Use_decl_annotations_
    bool MidiEndpointConnection::Open()
    {
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

        if (!IsOpen())
        {
            if (!ActivateMidiStream()) return false;

            if (m_endpointTransport != nullptr)
            {
                try
                {
                    InitializePlugins();

                    if (InternalOpen())
                    {
                        m_wasAlreadyOpened = true;

                        CallOnConnectionOpenedOnPlugins();

                        return true;
                    }
                    else
                    {
                        LOG_IF_FAILED(E_FAIL);  // force reporting of file/line number

                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"InternalOpen() returned false.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                        );

                        return false;
                    }
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
                        TraceLoggingWideString(L"hresult exception initializing endpoint interface. Service may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                        TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
                    );

                    m_endpointTransport = nullptr;

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
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Exception initializing endpoint interface. Service may be unavailable.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                    );

                    m_endpointTransport = nullptr;

                    return false;
                }
            }
            else
            {
                LOG_IF_FAILED(E_FAIL);  // force reporting of file/line number

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Endpoint transport interface is nullptr.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

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

        if (m_closeHasBeenCalled) return;

        try
        {
            CleanupPlugins();
            DeactivateMidiStream(true);

            m_isOpen = false;

            // TODO: any event cleanup?

            m_closeHasBeenCalled = true;

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exit success", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
            );

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
                TraceLoggingWideString(L"Exception on close.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );
        }
    }

    MidiEndpointConnection::~MidiEndpointConnection()
    {
     //   InternalClose();

        // don't release COM pointers here
        //if (m_serviceTransport)
        //{
        //    m_serviceTransport = nullptr;
        //}

        m_messageReceivedEvent.clear();
        m_endpointDeviceDisconnectedEvent.clear();
        m_endpointDeviceReconnectedEvent.clear();
    }

    _Use_decl_annotations_
    bool MidiEndpointConnection::DeactivateMidiStream(bool const force)
    {
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

        if (m_endpointTransport != nullptr)
        {
            // Shutdown should also release any callback references
            auto hr = m_endpointTransport->Shutdown();
            m_endpointTransport == nullptr;

            if (FAILED(hr))
            {
                LOG_IF_FAILED(hr);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed HRESULT cleaning up endpoint transport", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingHResult(hr, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                // if we're just forcing our way through this, we ignore the error
                // this happens when the device disconnects or another service-side issue
                // happens.
                if (!force)
                {
                    return false;
                }
            }
        }

        return true;
    }

    
    _Use_decl_annotations_
    bool MidiEndpointConnection::ActivateMidiStream() noexcept
    {
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

        if (m_serviceTransport == nullptr)
        {
            LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Service transport is null.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return false;
        }

        try
        {
            if (m_endpointTransport != nullptr)
            {
                m_endpointTransport = nullptr;
            }

            auto result = m_serviceTransport->Activate(__uuidof(IMidiBidirectional), (void**) &m_endpointTransport);

            if (SUCCEEDED(result) && m_endpointTransport != nullptr)
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Endpoint activated. Exiting.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
                );

                return true;
            }
            else 
            {
                LOG_IF_FAILED(result);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Endpoint Transport failed to Activate and returned null.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingHResult(result, MIDI_SDK_TRACE_HRESULT_FIELD)
                    );

                return false;
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
                TraceLoggingWideString(L"Exception activating stream. Service may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return false;
        }
    }
}
