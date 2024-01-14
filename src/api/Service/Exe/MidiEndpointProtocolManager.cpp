// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"
#include "Midi2MidiSrvAbstraction.h"
#include "ump_helpers.h"
#include "midi_ump_message_defs.h"

namespace internal = ::Windows::Devices::Midi2::Internal;

// Note: This class only works if these type F messages aren't swallowed up
// by some endpoint transform / processor. We'll need to have code in here
// later to require the processor be added, and if an option is introduced
// later, to ensure that it sets the option to forward messages along.

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolNegotiationWorker::Callback(PVOID Data, UINT Size, LONGLONG Position, LONGLONG Context)
{
    OutputDebugString(L"\n" __FUNCTION__);

    UNREFERENCED_PARAMETER(Position);
    UNREFERENCED_PARAMETER(Context);

    if (Data != nullptr && Size == UMP128_BYTE_COUNT)
    {
        internal::PackedUmp128 ump;

        if (internal::FillPackedUmp128FromBytePointer((byte*)Data, (uint8_t)Size, ump))
        {
            // if type F, process it.

            if (internal::GetUmpMessageTypeFromFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_UMP_MESSAGE_TYPE)
            {

                // if message is a device identity message, set flag

                auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);

                switch (messageStatus)
                {
                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
                    m_endpointInfoReceived = true;
                    m_declaredFunctionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

                    RequestAllFunctionBlocks();

                    break;

                case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                    m_deviceIdentityReceived = true;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                    ProcessStreamConfigurationRequest(ump);

                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                    m_countFunctionBlocksReceived += 1;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_countFunctionBlockNamesReceived += 1;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_endpointProductInstanceIdReceived = true;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_endpointNameReceived = true;
                    }
                    break;

                default:
                    OutputDebugString(L" Message is unidentified stream message\n");
                    break;
                }


            }
            else
            {
                // not a stream message. Ignore and move on
            }
        }
        else
        {
            // couldn't fill the UMP. Shouldn't happen since we pre-validate
            return E_FAIL;
        }
    }
    else
    {
        // Either null (hopefully not) or not a UMP128 so can't be a stream message. Fall out quickly
    }


    // check flags. If we've received everything, signal

    if (m_endpointInfoReceived &&
        m_endpointNameReceived &&
        m_endpointProductInstanceIdReceived &&
        m_deviceIdentityReceived &&
        m_countFunctionBlockNamesReceived == m_declaredFunctionBlockCount &&
        m_countFunctionBlocksReceived == m_declaredFunctionBlockCount &&
        m_finalStreamNegotiationResponseReceived)
    {
        m_allMessagesReceived.SetEvent();
    }


    return S_OK;
}

HRESULT
CMidiEndpointProtocolNegotiationWorker::RequestAllFunctionBlocks()
{
    internal::PackedUmp128 ump{};

    if (m_endpoint)
    {
        // first word
        ump.word0 = internal::BuildFunctionBlockDiscoveryRequestFirstWord(
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS,
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_MESSAGE_ALL_FILTER_FLAGS);

        // send it immediately
        return m_endpoint->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }

    return E_FAIL;
}


HRESULT
CMidiEndpointProtocolNegotiationWorker::RequestAllEndpointDiscoveryInformation()
{
    internal::PackedUmp128 ump{};

    if (m_endpoint)
    {
        // first word
        ump.word0 = internal::BuildEndpointDiscoveryRequestFirstWord(MIDI_PREFERRED_UMP_VERSION_MAJOR, MIDI_PREFERRED_UMP_VERSION_MINOR);

        // second word
        uint8_t filterBitmap = MIDI_ENDPOINT_DISCOVERY_MESSAGE_ALL_FILTER_FLAGS;
        internal::SetMidiWordMostSignificantByte4(ump.word1, filterBitmap);

        // send it immediately
        return m_endpoint->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolNegotiationWorker::ProcessStreamConfigurationRequest(internal::PackedUmp128 ump)
{
    // see if all is what we want. If not, we'll send a request to change configuration.

    if (m_endpoint)
    {
        auto protocol = internal::GetStreamConfigurationNotificationProtocolFromFirstWord(ump.word0);
        auto endpointRxJR = internal::GetStreamConfigurationNotificationReceiveJRFromFirstWord(ump.word0);
        auto endpointTxJR = internal::GetStreamConfigurationNotificationTransmitJRFromFirstWord(ump.word0);

        if (protocol != m_preferredMidiProtocol ||
            endpointRxJR != m_preferToSendJRTimestampsToEndpoint ||
            endpointTxJR != m_preferToReceiveJRTimestampsFromEndpoint)
        {
            if (!m_alreadyTriedToNegotiationOnce)
            {
                internal::PackedUmp128 configurationRequestUmp{};

                ump.word0 = internal::BuildStreamConfigurationRequestFirstWord(
                    m_preferredMidiProtocol,
                    m_preferToSendJRTimestampsToEndpoint,
                    m_preferToReceiveJRTimestampsFromEndpoint);

                m_alreadyTriedToNegotiationOnce = true;

                // send it immediately
                return m_endpoint->SendMidiMessage((byte*)&configurationRequestUmp, (UINT)sizeof(configurationRequestUmp), 0);
            }
            else
            {
                // we've already tried negotiating once. Don't do it again
                m_finalStreamNegotiationResponseReceived = true;
                return S_OK;
            }
        }
        else
        {
            // all good on this try
            m_finalStreamNegotiationResponseReceived = true;
            return S_OK;
        }

    }

    return E_FAIL;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolNegotiationWorker::Start(
    LPCWSTR DeviceInterfaceId,
    BOOL PreferToSendJRTimestampsToEndpoint,
    BOOL PreferToReceiveJRTimestampsFromEndpoint,
    BYTE PreferredMidiProtocol,
    WORD TimeoutMS,    
    wil::com_ptr_nothrow<IMidiAbstraction> ServiceAbstraction)

{
    m_preferToSendJRTimestampsToEndpoint = PreferToSendJRTimestampsToEndpoint;
    m_preferToReceiveJRTimestampsFromEndpoint = PreferToReceiveJRTimestampsFromEndpoint;
    m_preferredMidiProtocol = PreferredMidiProtocol;

    // by using the existing abstractions and activation methods just 
    // like any other client, we will get the transforms, including 
    // the metadata listener, for free. So we don't need to duplicate
    // metadata capture code here, and we don't need to make any
    // explicit call to the metadata capture transform plugin

    RETURN_HR_IF_NULL(E_FAIL, ServiceAbstraction);
    RETURN_IF_FAILED(ServiceAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&m_endpoint));

    // Create and open a connection to the endpoint, complete with metadata listeners

    DWORD mmcssTaskId{};
    ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP };

    RETURN_IF_FAILED(m_endpoint->Initialize(
        DeviceInterfaceId,
        &abstractionCreationParams,
        &mmcssTaskId,
        this,
        0
    ));

    m_allMessagesReceived.create();

    // Send initial discovery request
    // the rest happens in response to messages in the callback
    RETURN_IF_FAILED(RequestAllEndpointDiscoveryInformation());


    // Wait until all metadata arrives or we timeout
    if (!m_allMessagesReceived.wait(TimeoutMS))
    {
        // we didn't receive everything, but that's not a failure condition for this.
    }

    return S_OK;
}





_Use_decl_annotations_
HRESULT 
CMidiEndpointProtocolManager::Initialize(
    std::shared_ptr<CMidiClientManager>& ClientManager,
    std::shared_ptr<CMidiDeviceManager>& DeviceManager
)
{
    m_clientManager = ClientManager;
    m_deviceManager = DeviceManager;

    // connect to the service. Needing a reference to this abstraction def 
    // creates a circular reference to the MidiSrv Abstraction. Not sure of 
    // a good way around that other than fixing up the ClientManager to make
    // local connections with local handles reasonable.
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_serviceAbstraction)));

    return S_OK;
}





_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::NegotiateAndRequestMetadata(
    LPCWSTR DeviceInterfaceId,
    BOOL PreferToSendJRTimestampsToEndpoint,
    BOOL PreferToReceiveJRTimestampsFromEndpoint,
    BYTE PreferredMidiProtocol,
    WORD TimeoutMS
) noexcept
{
    // We assume the endpoint manager which calls this function knows whether or not protocol
    // negotiation etc. should happen. So we leave it up to the transport to correctly call
    // this or not, and we just assume it made the correct choice.
    // 
    // For now, we'll do this synchronously, but it should be in a separate thread in the future

    // create the queue worker thread

    CMidiEndpointProtocolNegotiationWorker worker;

    // TODO: Going to need to keep a map of these
    //std::thread workerThread(
    //    &CMidiEndpointProtocolNegotiationWorker::Start(DeviceInterfaceId, PreferToSendJRTimestampsToEndpoint, PreferToReceiveJRTimestampsFromEndpoint, PreferredMidiProtocol, TimeoutMS),
    //    &worker);

    //m_queueWorkerThread = std::move(workerThread);

    // start up the worker thread
    //m_queueWorkerThread.detach();


    // Synchronous for testing
    return worker.Start(
        DeviceInterfaceId, 
        PreferToSendJRTimestampsToEndpoint, 
        PreferToReceiveJRTimestampsFromEndpoint, 
        PreferredMidiProtocol, 
        TimeoutMS,
        m_serviceAbstraction
    );

}



HRESULT 
CMidiEndpointProtocolManager::Cleanup()
{
    m_clientManager.reset();
    m_deviceManager.reset();

    return S_OK;
}
