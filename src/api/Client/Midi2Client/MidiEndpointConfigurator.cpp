// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConfigurator.h"
#include "MidiEndpointConfigurator.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiEndpointConfigurator::Initialize() noexcept
    {
    }

    _Use_decl_annotations_
    void MidiEndpointConfigurator::OnEndpointConnectionOpened() noexcept
    {
        bool infoFoundInCache = false;

        // TODO: do we already have cached info? 

        if (!infoFoundInCache)
        {
            // No cached info. Send out the negotiation requests and go through the full process
            BeginDiscovery();
        }

    }

    _Use_decl_annotations_
    void MidiEndpointConfigurator::Cleanup() noexcept
    {
        // no cleanup required
    }



    _Use_decl_annotations_
    void MidiEndpointConfigurator::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent) noexcept
    {
        try
        {
            // all metadata messages (function block, endpoint name, product instance id, device id, etc.
            // are handled by other plugins). The only one handled here is endpoint information.

            if (args.MessageType() == MidiMessageType::Stream128)
            {
                // the endpoint info notification, when requested, is a required part of the 
                // MIDI 2.0 protocol. If we don't get one it's not a MIDI 2.0 implementation.
                if (MidiMessageUtility::GetStatusFromStreamMessageFirstWord(args.PeekFirstWord()) == 
                    MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION)
                {
                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;


                    MidiEndpointInformation info;
                    info.UpdateFromInfoNotificationMessage(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    // TODO: Update metadata cache with endpoint info

                    // TODO: remove any function block information we already have cached

                    // Request function blocks. Those are received by another listener
                    // We only request function blocks after we've recieved the info notification
                    // with a function block count > 0

                    if (info.FunctionBlockCount() > 0)
                    {
                        RequestAllFunctionBlocks();
                    }
                }
                else if (MidiMessageUtility::GetStatusFromStreamMessageFirstWord(args.PeekFirstWord()) == 
                    MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION)
                {
                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;

                    bool isProtocolWhatWeWant = false;

                    // TODO: Check to see if this is the protocol we want. If not, send a stream configuration request for what we want, assuming
                    // the info notification (if we have received it) supports what we want.

                    // TODO: Don't let this go into an infinite loop. One ping only :).
                    
                    if (!isProtocolWhatWeWant)
                    {
                        BeginNegotiation();
                    }




                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception processing incoming message.");
        }
        
    }


    _Use_decl_annotations_
    bool MidiEndpointConfigurator::BeginDiscovery() noexcept
    {
        try
        {
            // Request configuration based on the connection open options

            // see UMP spec page 37 for this process for protocol negotiation
            // the OS here is Device 1

            const auto discoveryFilterFlags =
                MidiEndpointDiscoveryFilterFlags::RequestEndpointInformation |
                MidiEndpointDiscoveryFilterFlags::RequestDeviceIdentity |
                MidiEndpointDiscoveryFilterFlags::RequestEndpointName |
                MidiEndpointDiscoveryFilterFlags::RequestProductInstanceId |
                MidiEndpointDiscoveryFilterFlags::RequestStreamConfiguration;

            // send out endpoint discovery asking for endpoint info and stream configuration
            auto discoveryMessage = midi2::MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(
                MidiClock::GetMidiTimestamp(),
                m_configurationRequested.SpecificationVersionMajor(),
                m_configurationRequested.SpecificationVersionMinor(),
                discoveryFilterFlags
            );

            return m_outputConnection.SendMessagePacket(discoveryMessage) == MidiSendMessageResult::Success;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception beginning negotiation.");

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointConfigurator::BeginNegotiation() noexcept
    {
        try
        {
            auto request = MidiStreamMessageBuilder::BuildStreamConfigurationRequestMessage(
                MidiClock::GetMidiTimestamp(),
                (uint8_t)m_configurationRequested.PreferredMidiProtocol(), 
                m_configurationRequested.RequestEndpointReceiveJitterReductionTimestamps(),
                m_configurationRequested.RequestEndpointTransmitJitterReductionTimestamps()
           );

            // eating the status here probably isn't all that cool a thing to do
            return m_outputConnection.SendMessagePacket(request) == MidiSendMessageResult::Success;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception beginning negotiation.");

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointConfigurator::RequestAllFunctionBlocks() noexcept
    {
        return RequestSingleFunctionBlock(0xFF);       // 0xFF is flag for "all"
    }

    _Use_decl_annotations_
    bool MidiEndpointConfigurator::RequestSingleFunctionBlock(uint8_t functionBlockNumber) noexcept
    {
        try
        {
            auto request = MidiStreamMessageBuilder::BuildFunctionBlockDiscoveryMessage(
                MidiClock::GetMidiTimestamp(),
                functionBlockNumber,
                midi2::MidiFunctionBlockDiscoveryFilterFlags::RequestFunctionBlockInformation | midi2::MidiFunctionBlockDiscoveryFilterFlags::RequestFunctionBlockName
            );

            // eating the status here probably isn't all that cool a thing to do
            return m_outputConnection.SendMessagePacket(request) == MidiSendMessageResult::Success;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" exception requesting function block.");

            return false;
        }
    }

}
