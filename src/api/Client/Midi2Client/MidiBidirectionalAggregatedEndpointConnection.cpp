// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#include "pch.h"
#include "MidiBidirectionalAggregatedEndpointConnection.h"
#include "MidiBidirectionalAggregatedEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{


    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::InternalInitialize(
        _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        _In_ winrt::hstring const connectionId,
        _In_ winrt::hstring const inputEndpointDeviceId,
        _In_ winrt::hstring const outputEndpointDeviceId)
    {
        try
        {
            m_id = connectionId;

            m_inputDeviceId = inputEndpointDeviceId;
            m_outputDeviceId = outputEndpointDeviceId;

            WINRT_ASSERT(!InputDeviceId().empty());
            WINRT_ASSERT(!OutputDeviceId().empty());

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
    bool MidiBidirectionalAggregatedEndpointConnection::Open()
    {
        if (!IsOpen())
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiIn), (void**)&m_inputAbstraction))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate input MIDI Stream");

                return false;
            }

            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiOut), (void**)&m_outputAbstraction))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate output MIDI Stream");

                return false;
            }



            if (m_outputAbstraction != nullptr && m_inputAbstraction != nullptr)
            {
                try
                {
                    DWORD mmcssTaskIdInput{};
                    DWORD mmcssTaskIdIOutput{};

                    winrt::check_hresult(m_inputAbstraction->Initialize(
                        (LPCWSTR)(InputEndpointDeviceId().c_str()),
                        &mmcssTaskIdInput,
                        (IMidiCallback*)(this)
                    ));

                    winrt::check_hresult(m_outputAbstraction->Initialize(
                        (LPCWSTR)(OutputEndpointDeviceId().c_str()),
                        &mmcssTaskIdIOutput
                    ));

                    IsOpen(true);
                    InputIsOpen(true);
                    OutputIsOpen(true);

                    CallOnConnectionOpenedOnPlugins();

                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_inputAbstraction = nullptr;
                    m_outputAbstraction = nullptr;

                    IsOpen(false);
                    InputIsOpen(false);
                    OutputIsOpen(false);

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" Endpoint interface is nullptr");

                m_inputAbstraction = nullptr;
                m_outputAbstraction = nullptr;

                IsOpen(false);
                InputIsOpen(false);
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



    MidiBidirectionalAggregatedEndpointConnection::~MidiBidirectionalAggregatedEndpointConnection()
    {
        // TODO
    }


}
