// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiOutputEndpointConnection.h"
#include "MidiOutputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    _Success_(return == true)
    bool MidiOutputEndpointConnection::InternalInitialize(
        _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        _In_ winrt::hstring const endpointId,
        _In_ winrt::hstring const deviceId)
    {
        try
        {
            m_id = endpointId;
            m_outputDeviceId = deviceId;


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
    bool MidiOutputEndpointConnection::Open()
    {
        if (!IsOpen())
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiOut), (void**)&m_outputAbstraction))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                IsOpen(false);
                OutputIsOpen(false);
                m_outputAbstraction = nullptr;

                return false;
            }

            if (m_outputAbstraction != nullptr)
            {
                try
                {
                    DWORD mmcssTaskId{};  // TODO: Does this need to be session-wide? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

                    winrt::check_hresult(m_outputAbstraction->Initialize(
                        (LPCWSTR)(OutputDeviceId().c_str()),
                        &mmcssTaskId
                    ));

                    IsOpen(true);
                    OutputIsOpen(true);

                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    IsOpen(false);
                    OutputIsOpen(false);
                    m_outputAbstraction = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" Endpoint interface is nullptr");

                IsOpen(false);
                OutputIsOpen(false);

                return false;
            }


        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }


    MidiOutputEndpointConnection::~MidiOutputEndpointConnection()
    {
        if (m_outputAbstraction != nullptr)
        {
            m_outputAbstraction->Cleanup();
            m_outputAbstraction = nullptr;
        }

        IsOpen(false);
        OutputIsOpen(false);

        // TODO: any event cleanup?
    }

}



