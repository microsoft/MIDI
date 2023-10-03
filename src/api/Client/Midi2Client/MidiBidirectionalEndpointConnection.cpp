// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiBidirectionalEndpointConnection.h"
#include "MidiBidirectionalEndpointConnection.g.cpp"




namespace winrt::Windows::Devices::Midi2::implementation
{
    
    _Use_decl_annotations_
    bool MidiBidirectionalEndpointConnection::InternalInitialize(
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        winrt::guid const connectionId,
        winrt::hstring const endpointDeviceId, 
        midi2::MidiBidirectionalEndpointOpenOptions options
    )
    {
        internal::LogInfo(__FUNCTION__, L" Initialize ");

        try
        {
            m_connectionId = connectionId;

            m_inputDeviceId = endpointDeviceId;     // for a true bidirectional endpoint, input and output are the same Id
            m_outputDeviceId = endpointDeviceId;

            WINRT_ASSERT(!m_inputDeviceId.empty());
            WINRT_ASSERT(!m_outputDeviceId.empty());
            WINRT_ASSERT(serviceAbstraction != nullptr);

            m_serviceAbstraction = serviceAbstraction;

            
            m_options = options;


            // TODO: Add any other automatic handlers if the options allow for it

            if (options == nullptr || !options.DisableAutomaticStreamConfiguration())
            {
                auto configurator = winrt::make_self<implementation::MidiEndpointConfigurator>();

                m_messageProcessingPlugins.Append(*configurator);
            }

            if (options == nullptr || !options.DisableAutomaticEndpointMetadataHandling())
            {
                auto plug = winrt::make_self<implementation::MidiEndpointMetadataEndpointListener>();

                m_messageProcessingPlugins.Append(*plug);
            }

            if (options == nullptr || !options.DisableAutomaticFunctionBlockMetadataHandling())
            {
                auto plugin = winrt::make_self<implementation::MidiFunctionBlockEndpointListener>();

                m_messageProcessingPlugins.Append(*plugin);
            }


            IMidiInputConnection input = *this;
            IMidiOutputConnection output = *this;

            SetInputConnectionOnPlugins(input);
            SetOutputConnectionOnPlugins(output);

            if (options != nullptr)
            {
                SetRequestedStreamConfigurationOnPlugins(options.RequestedStreamConfiguration());
            }

            InitializePlugins();


            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint.", ex);

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiBidirectionalEndpointConnection::Open()
    {
        internal::LogInfo(__FUNCTION__, L" Open ");

        if (!IsOpen())
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            // we use m_inputAbstraction here and simply provide a copy of it to m_outputAbstraction so that
            // we don't have to duplicate all that code
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiBiDi), (void**)&m_inputAbstraction))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                return false;
            }

            if (m_inputAbstraction != nullptr)
            {
                try
                {
                    DWORD mmcssTaskId{};  // TODO: Does this need to be session-wide? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

                    winrt::check_hresult(m_inputAbstraction->Initialize(
                        (LPCWSTR)(InternalGetDeviceId().c_str()),
                        &mmcssTaskId,
                        (IMidiCallback*)(this)
                    ));

                    // provide a copy to the output logic
                    m_outputAbstraction = m_inputAbstraction;

                    IsOpen(true);
                    OutputIsOpen(true);
                    InputIsOpen(true);

                    CallOnConnectionOpenedOnPlugins();

                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_inputAbstraction = nullptr;
                    m_outputAbstraction = nullptr;

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



    void MidiBidirectionalEndpointConnection::Close()
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

            // output is also input, so don't call cleanup
            m_outputAbstraction = nullptr;

            IsOpen(false);
            InputIsOpen(false);
            OutputIsOpen(false);

            // TODO: any event cleanup?

            m_closeHasBeenCalled = true;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception on close");
        }
    }

    MidiBidirectionalEndpointConnection::~MidiBidirectionalEndpointConnection()
    {
        if (!m_closeHasBeenCalled)
            Close();
    }

}
