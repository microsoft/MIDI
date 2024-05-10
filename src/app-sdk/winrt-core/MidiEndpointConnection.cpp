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
            internal::LogGeneralError(__FUNCTION__, L"Error initializing endpoint. Service abstraction is null", endpointDeviceId);

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
            internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint.", ex, endpointDeviceId);

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
            internal::LogGeneralError(__FUNCTION__, L"Failed to open connection. endpoint abstraction is null", m_endpointDeviceId);

            return false;
        }

        DWORD mmcssTaskId{};
        LPCWSTR connectionSettingsJsonString = nullptr;

        if (m_connectionSettings != nullptr)
        {
            internal::LogInfo(__FUNCTION__, L"Connection settings were specified. Including JSON in service call.", m_endpointDeviceId);

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
            internal::LogInfo(__FUNCTION__, L"Endpoint abstraction successfully initialized", m_endpointDeviceId);

            m_isOpen = true;
            m_closeHasBeenCalled = false;
        }
        else
        {
            internal::LogHresultError(__FUNCTION__, L"Failed to initialize endpoint abstraction", result, m_endpointDeviceId);
        }

        internal::LogInfo(__FUNCTION__, L"Connection Opened", m_endpointDeviceId);

        return true;
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
                        m_wasAlreadyOpened = true;

                        CallOnConnectionOpenedOnPlugins();

                        return true;
                    }
                    else
                    {
                        internal::LogGeneralError(__FUNCTION__, L"InternalOpen() returned false.", m_endpointDeviceId);

                        return false;
                    }
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint interface. Service may be unavailable.", ex, m_endpointDeviceId);

                    m_endpointAbstraction = nullptr;

                    return false;
                }
                catch (...)
                {
                    internal::LogGeneralError(__FUNCTION__, L"Exception initializing endpoint interface. Service may be unavailable.", m_endpointDeviceId);

                    m_endpointAbstraction = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint abstraction interface is nullptr", m_endpointDeviceId);

                return false;
            }
        }
        else
        {
            internal::LogInfo(__FUNCTION__, L"Connection already open", m_endpointDeviceId);

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
            DeactivateMidiStream(true);

            m_isOpen = false;

            // TODO: any event cleanup?

            m_closeHasBeenCalled = true;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception on close", m_endpointDeviceId);
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
    bool MidiEndpointConnection::DeactivateMidiStream(bool const force)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (m_endpointAbstraction != nullptr)
        {
            // todo: some error / hresult handling here

            auto hr = m_endpointAbstraction->Cleanup();

            if (FAILED(hr))
            {
                internal::LogHresultError(__FUNCTION__, L"Failed HRESULT cleaning up endpoint abstraction", hr, m_endpointDeviceId);

                // if we're just forcing our way through this, we ignore the error
                // this happens when the device disconnects or another service-side issue
                // happens.
                if (!force)
                {
                    return false;
                }
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
            internal::LogGeneralError(__FUNCTION__, L"Service abstraction is null.", m_endpointDeviceId);

            return false;
        }

        try
        {
            winrt::check_hresult(m_serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**) &m_endpointAbstraction));

            if (m_endpointAbstraction == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint Abstraction failed to Activate and returned null.", m_endpointDeviceId);

                return false;
            }

            internal::LogInfo(__FUNCTION__, L"Activated IMidiBiDi", m_endpointDeviceId);

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception activating stream. Service may be unavailable", ex, m_endpointDeviceId);

            return false;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception activating stream. Service may be unavailable", m_endpointDeviceId);

            return false;
        }
    }
}
