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
    _Success_(return == true)
    bool MidiInputEndpointConnection::InternalInitialize(
        _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        _In_ winrt::guid const connectionId,
        _In_ winrt::hstring const endpointDeviceId)
    {
        internal::LogInfo(__FUNCTION__, L" Initialize ");

        try
        {
            m_connectionId = connectionId;
            m_inputDeviceId = endpointDeviceId;

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
        internal::LogInfo(__FUNCTION__, L" Open ");

        if (!IsOpen())
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiIn), (void**)&m_inputAbstraction))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                IsOpen(false);
                InputIsOpen(false);

                m_inputAbstraction = nullptr;

                return false;
            }

            if (m_inputAbstraction != nullptr)
            {
                try
                {
                    DWORD mmcssTaskId{};  // TODO: Does this need to be session-wide? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

                    winrt::check_hresult(m_inputAbstraction->Initialize(
                        (LPCWSTR)(InputEndpointDeviceId().c_str()),
                        &mmcssTaskId,
                        (IMidiCallback*)(this)
                    ));

                    IsOpen(true);
                    InputIsOpen(true);

                    CallOnConnectionOpenedOnPlugins();

                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_inputAbstraction = nullptr;

                    IsOpen(false);
                    InputIsOpen(false);

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" Endpoint interface is nullptr");

                IsOpen(false);
                InputIsOpen(false);

                return false;

            }


        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }

    void MidiInputEndpointConnection::Close()
    {
        internal::LogInfo(__FUNCTION__, L" Close ");

        if (m_closeHasBeenCalled) return;

        try
        {
            CleanupPlugins();

            if (m_inputAbstraction != nullptr)
            {
                m_inputAbstraction->Cleanup();
                m_inputAbstraction = nullptr;
            }

            IsOpen(false);
            InputIsOpen(false);

            // TODO: any event cleanup?

            m_closeHasBeenCalled = true;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception on close");
        }
    }



    MidiInputEndpointConnection::~MidiInputEndpointConnection()
    {
        if (!m_closeHasBeenCalled)
            Close();
    }

}
