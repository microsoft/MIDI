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
CMidiEndpointProtocolManager::Initialize(
    std::shared_ptr<CMidiClientManager>& ClientManager,
    std::shared_ptr<CMidiDeviceManager>& DeviceManager
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    OutputDebugString(__FUNCTION__ L"");

    m_clientManager = ClientManager;
    m_deviceManager = DeviceManager;


    m_allMessagesReceived.create();
    m_queueWorkerThreadWakeup.create();

    // connect to the service. Needing a reference to this abstraction def 
    // creates a circular reference to the MidiSrv Abstraction. Not sure of 
    // a good way around that other than fixing up the ClientManager to make
    // local connections with local handles reasonable.
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_serviceAbstraction)));

    try
    {
        // start background processing thread

        std::thread workerThread(
            &CMidiEndpointProtocolManager::ThreadWorker,
            this);

        m_queueWorkerThread = std::move(workerThread);

        // start up the worker thread
        m_queueWorkerThread.detach();

    }
    CATCH_RETURN();

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
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    OutputDebugString(__FUNCTION__ L"");

    ProtocolManagerWork work;

    // DEBUG
    //TimeoutMS = 25000;



    work.EndpointInstanceId = DeviceInterfaceId;
    work.PreferToSendJRTimestampsToEndpoint = PreferToSendJRTimestampsToEndpoint;
    work.PreferToReceiveJRTimestampsFromEndpoint = PreferToReceiveJRTimestampsFromEndpoint;
    work.PreferredMidiProtocol = PreferredMidiProtocol;
    work.TimeoutMS = TimeoutMS;

    m_workQueue.push(std::move(work));


    // todo: signal event that there's new work
    m_queueWorkerThreadWakeup.SetEvent();


    return S_OK;
}


HRESULT
CMidiEndpointProtocolManager::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    OutputDebugString(__FUNCTION__ L"");

    // TODO terminate any open threads and ensure they close up

    m_clientManager.reset();
    m_deviceManager.reset();

    // clear the queue
    while (m_workQueue.size() > 0) m_workQueue.pop();

    m_shutdown = true;
    m_queueWorkerThreadWakeup.SetEvent();

    return S_OK;
}









_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::Callback(PVOID Data, UINT Size, LONGLONG Position, LONGLONG Context)
{
    OutputDebugString(L"\n" __FUNCTION__ L" - enter");

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
                auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);

                switch (messageStatus)
                {
                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION\n");
                    m_currentWorkItem.TaskEndpointInfoReceived = true;
                    m_currentWorkItem.DeclaredFunctionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

                    RequestAllFunctionBlocks();

                    break;

                case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION\n");
                    m_currentWorkItem.TaskDeviceIdentityReceived = true;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION\n");
                    ProcessStreamConfigurationRequest(ump);

                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION\n");
                    m_currentWorkItem.CountFunctionBlocksReceived += 1;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION\n");
                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_currentWorkItem.CountFunctionBlockNamesReceived += 1;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION\n");
                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_currentWorkItem.TaskEndpointProductInstanceIdReceived = true;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                    OutputDebugString(__FUNCTION__ L" - MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION\n");
                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_currentWorkItem.TaskEndpointNameReceived = true;
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
                OutputDebugString(__FUNCTION__ L" - not a stream message\n");

            }
        }
        else
        {
            // couldn't fill the UMP. Shouldn't happen since we pre-validate
            OutputDebugString(__FUNCTION__ L" - unable to fill UMP128 with the data\n");

            return E_FAIL;
        }
    }
    else
    {
        // Either null (hopefully not) or not a UMP128 so can't be a stream message. Fall out quickly
        OutputDebugString(__FUNCTION__ L" - Not a message we're interested in\n" );

    }


    // check flags. If we've received everything, signal

    if (m_currentWorkItem.TaskEndpointInfoReceived &&
        m_currentWorkItem.TaskEndpointNameReceived &&
        m_currentWorkItem.TaskEndpointProductInstanceIdReceived &&
        m_currentWorkItem.TaskDeviceIdentityReceived &&
        m_currentWorkItem.CountFunctionBlockNamesReceived == m_currentWorkItem.DeclaredFunctionBlockCount &&
        m_currentWorkItem.CountFunctionBlocksReceived == m_currentWorkItem.DeclaredFunctionBlockCount &&
        m_currentWorkItem.TaskFinalStreamNegotiationResponseReceived)
    {
        OutputDebugString(__FUNCTION__ L" - All messages received. Setting event\n");

        m_allMessagesReceived.SetEvent();

        // done
    }


    return S_OK;
}

HRESULT
CMidiEndpointProtocolManager::RequestAllFunctionBlocks()
{
    OutputDebugString(__FUNCTION__ L"");
    
    internal::PackedUmp128 ump{};

    if (m_currentWorkItem.Endpoint)
    {
        // first word
        ump.word0 = internal::BuildFunctionBlockDiscoveryRequestFirstWord(
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS,
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_MESSAGE_ALL_FILTER_FLAGS);

        // send it immediately
        return m_currentWorkItem.Endpoint->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }

    return E_FAIL;
}


HRESULT
CMidiEndpointProtocolManager::RequestAllEndpointDiscoveryInformation()
{
    OutputDebugString(__FUNCTION__ L"");
    
    internal::PackedUmp128 ump{};

    if (m_currentWorkItem.Endpoint)
    {
        OutputDebugString(__FUNCTION__ L" - sending discovery");

        // first word
        ump.word0 = internal::BuildEndpointDiscoveryRequestFirstWord(MIDI_PREFERRED_UMP_VERSION_MAJOR, MIDI_PREFERRED_UMP_VERSION_MINOR);

        // second word
        uint8_t filterBitmap = MIDI_ENDPOINT_DISCOVERY_MESSAGE_ALL_FILTER_FLAGS;
        internal::SetMidiWordMostSignificantByte4(ump.word1, filterBitmap);

        // send it immediately
        return m_currentWorkItem.Endpoint->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" - endpoint null");
    }

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::ProcessStreamConfigurationRequest(internal::PackedUmp128 ump)
{
    OutputDebugString(__FUNCTION__ L"");
 
    // see if all is what we want. If not, we'll send a request to change configuration.

    if (m_currentWorkItem.Endpoint)
    {
        auto protocol = internal::GetStreamConfigurationNotificationProtocolFromFirstWord(ump.word0);
        auto endpointRxJR = internal::GetStreamConfigurationNotificationReceiveJRFromFirstWord(ump.word0);
        auto endpointTxJR = internal::GetStreamConfigurationNotificationTransmitJRFromFirstWord(ump.word0);

        if (protocol != m_currentWorkItem.PreferredMidiProtocol ||
            endpointRxJR != m_currentWorkItem.PreferToSendJRTimestampsToEndpoint ||
            endpointTxJR != m_currentWorkItem.PreferToReceiveJRTimestampsFromEndpoint)
        {
            if (!m_currentWorkItem.AlreadyTriedToNegotiationOnce)
            {
                internal::PackedUmp128 configurationRequestUmp{};

                ump.word0 = internal::BuildStreamConfigurationRequestFirstWord(
                    m_currentWorkItem.PreferredMidiProtocol,
                    m_currentWorkItem.PreferToSendJRTimestampsToEndpoint,
                    m_currentWorkItem.PreferToReceiveJRTimestampsFromEndpoint);

                m_currentWorkItem.AlreadyTriedToNegotiationOnce = true;

                // send it immediately
                return m_currentWorkItem.Endpoint->SendMidiMessage((byte*)&configurationRequestUmp, (UINT)sizeof(configurationRequestUmp), 0);
            }
            else
            {
                // we've already tried negotiating once. Don't do it again
                m_currentWorkItem.TaskFinalStreamNegotiationResponseReceived = true;
                return S_OK;
            }
        }
        else
        {
            // all good on this try
            m_currentWorkItem.TaskFinalStreamNegotiationResponseReceived = true;
            return S_OK;
        }

    }

    return E_FAIL;
}



HRESULT
CMidiEndpointProtocolManager::ProcessCurrentWorkEntry()
{
    OutputDebugString(__FUNCTION__ L"");
    OutputDebugString(m_currentWorkItem.EndpointInstanceId.c_str());


    // by using the existing abstractions and activation methods just 
    // like any other client, we will get the transforms, including 
    // the metadata listener, for free. So we don't need to duplicate
    // metadata capture code here, and we don't need to make any
    // explicit call to the metadata capture transform plugin

    OutputDebugString(__FUNCTION__ L" - check for null service abstraction");
    RETURN_HR_IF_NULL(E_FAIL, m_serviceAbstraction);

    OutputDebugString(__FUNCTION__ L" - activate BiDi abstraction");
    RETURN_IF_FAILED(m_serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&(m_currentWorkItem.Endpoint)));

    // Create and open a connection to the endpoint, complete with metadata listeners

    OutputDebugString(__FUNCTION__ L" - Initialize endpoint");

    DWORD mmcssTaskId{};
    ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP };

    RETURN_IF_FAILED(m_currentWorkItem.Endpoint->Initialize(
        m_currentWorkItem.EndpointInstanceId.c_str(),
        &abstractionCreationParams,
        &mmcssTaskId,
        (IMidiCallback*)this,
        MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT
    ));

//    OutputDebugString(__FUNCTION__ L" - Resetting messages received event");

//    if (m_allMessagesReceived.is_signaled()) m_allMessagesReceived.ResetEvent();

    HRESULT hr = S_OK;

    // Send initial discovery request
    // the rest happens in response to messages in the callback
    LOG_IF_FAILED(hr = RequestAllEndpointDiscoveryInformation());

    if (SUCCEEDED(hr))
    {
        OutputDebugString(__FUNCTION__ L" - Requested discovery information");
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" - FAILED to request discovery information");
    }

    if (SUCCEEDED(hr))
    {
        // Wait until all metadata arrives or we timeout
        if (!m_allMessagesReceived.wait(m_currentWorkItem.TimeoutMS))
        {
            // we didn't receive everything, but that's not a failure condition for this.
        }
    }

    if (m_allMessagesReceived.is_signaled()) m_allMessagesReceived.ResetEvent();

    OutputDebugString(__FUNCTION__ L" - About to cleanup");

    m_currentWorkItem.Endpoint->Cleanup();
    m_currentWorkItem.Endpoint = nullptr;

    return hr;
}


void CMidiEndpointProtocolManager::ThreadWorker()
{
    while (!m_shutdown)
    {
        if (m_workQueue.size() > 0)
        {
            {
                OutputDebugString(__FUNCTION__ L" - Item in queue. About to lock and pop :)");

                std::lock_guard<std::mutex> lock{ m_queueMutex };

                m_currentWorkItem = std::move(m_workQueue.front());
                m_workQueue.pop();

                OutputDebugString(__FUNCTION__ L" - Obtained item from queue. About to process");

            }

            // this will block until completed
            LOG_IF_FAILED(ProcessCurrentWorkEntry());
        }

        // todo
        Sleep(300);
    }
}

