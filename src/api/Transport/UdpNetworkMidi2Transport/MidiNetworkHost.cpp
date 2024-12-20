// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <chrono>

_Use_decl_annotations_
HRESULT 
MidiNetworkHost::Initialize(
    MidiNetworkHostDefinition& hostDefinition
)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.HostName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ServiceInstanceName.empty());

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.UmpEndpointName.size() > MAX_UMP_ENDPOINT_NAME_LENGTH);

    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.empty());
    RETURN_HR_IF(E_INVALIDARG, hostDefinition.ProductInstanceId.size() > MAX_UMP_PRODUCT_INSTANCE_ID_LENGTH);

    if (!hostDefinition.UseAutomaticPortAllocation)
    {
        RETURN_HR_IF(E_INVALIDARG, hostDefinition.Port.empty());
    }

    m_hostDefinition = hostDefinition;

    if (m_hostDefinition.Advertise)
    {
        m_advertiser = std::make_shared<MidiNetworkAdvertiser>();
        RETURN_HR_IF_NULL(E_POINTER, m_advertiser);
        RETURN_IF_FAILED(m_advertiser->Initialize());
    }

    return S_OK;
}

HRESULT
MidiNetworkHost::Start()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    HostName hostName(m_hostDefinition.HostName);

    



    // this should have error checking
    m_socket.BindServiceNameAsync(winrt::to_hstring(m_hostDefinition.Port)).get();

    auto boundPort = static_cast<uint16_t>(std::stoi(winrt::to_string(m_socket.Information().LocalPort())));

    // advertise
    if (m_hostDefinition.Advertise && m_advertiser != nullptr)
    {
        RETURN_IF_FAILED(m_advertiser->Advertise(
            m_hostDefinition.ServiceInstanceName,
            hostName,
            m_socket,
            boundPort,
            m_hostDefinition.UmpEndpointName,
            m_hostDefinition.ProductInstanceId
        ));
    }


    // TODO: spin up new thread and start listening
    //ProcessIncomingPackets();
    


    return S_OK;
}

_Use_decl_annotations_
void MidiNetworkHost::OnUdpPacketReceived(
    DatagramSocket const& sender,
    DatagramSocketMessageReceivedEventArgs const& args)
{
    UNREFERENCED_PARAMETER(sender);

    auto reader = args.GetDataReader();

    if (reader.UnconsumedBufferLength() < sizeof(uint32_t) * 2)
    {
        // not a message we understand. Needs to be at least the size of the 
        // MIDI header plus a command packet header. Really it needs to be larger, but
        // just trying to weed out blips


        return;
    }

    uint32_t udpHeader = reader.ReadUInt32();

    if (udpHeader != MIDI_UDP_PAYLOAD_HEADER)
    {
        // not a message we understand

        return;
    }

    while (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
    {
        uint32_t commandHeaderWord = reader.ReadUInt32();

        MidiNetworkCommandPacketHeader packetHeader;
        packetHeader.HeaderWord = commandHeaderWord;

        uint8_t payloadWordsToRead = packetHeader.HeaderData.CommandPayloadLength;


        switch (packetHeader.HeaderData.CommandCode)
        {
        case CommandClientToHost_Invitation:
        case CommandClientToHost_InvitationWithAuthentication:
        case CommandClientToHost_InvitationWithUserAuthentication:
        case CommandCommon_Ping:
        case CommandCommon_PingReply:
        case CommandCommon_NAK:
        case CommandCommon_Bye:
        case CommandCommon_ByeReply:
            {
                MidiNetworkOutOfBandIncomingCommandPacket packet{};

                packet.SourceHostName = args.RemoteAddress().ToString();
                packet.SourcePort = args.RemotePort();
                packet.Header = packetHeader;

                // command payload length is given in 32 bit words
                auto expectedDataByteCount = packetHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t);

                // Socket read operations are blocking, so if there's bad data (command payload length not correct)
                // then we need to be able to handle that here. Otherwise, it just waits.
                if (reader.UnconsumedBufferLength() < expectedDataByteCount)
                {
                    // TODO: packets must be complete by spec, but not sure if Windows might chunk them
                    // in the reader. TBD.
                }
                else
                {
                    // TODO: We need to read only the expectedDataByteCount. There could be
                    // more than one command packet in the buffer

                    packet.SetMinimumBufferDataSizeAndAlign(packetHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t));
                    reader.ReadBytes(packet.DataBuffer);

                    m_incomingOutOfBandCommands.push(packet);
                }
            }
            break;

        case CommandCommon_UmpData:
            if (SessionExists(args.RemoteAddress(), args.RemotePort()))
            {
                auto sessionKey = CreateSessionMapKey(args.RemoteAddress(), args.RemotePort());

                // TODO: check the UMP message sequence number. If it is lower than the last 
                // received sequence number (accounting for wrap), then just ignore.
                // If it is the last received sequence number + 1, then the session can process 
                // them immediately. FEC is also covered by this as we walk the messages.
                // If we have a gap in the sequence numbers, request a retransmit of the missing
                // message data packet(s). This will require either holding on to the current 
                // packet or discarding it and re-requesting the missing plus this packet.
                // Note: The packet number is only 16 bits, so it'll wrap. Need to ensure we
                // handle that here.





            }
            else
            {
                // no active session. Drain the buffer
                reader.ReadBuffer(payloadWordsToRead * sizeof(uint32_t));
            }
            break;

        case CommandCommon_RetransmitRequest:
        case CommandCommon_RetransmitError:
            // TODO: handle retransmitting data
            break;

        case CommandCommon_SessionReset:
        case CommandCommon_SessionResetReply:
            // TODO: handle session reset
            break;

        default:
            // TODO: unexpected command code
            break;

        }

    }




}













HRESULT 
MidiNetworkHost::ProcessIncomingPackets()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // if the packet is a session packet, route to the appropriate session
    // otherwise, handle it here

    while (m_keepProcessing)
    {

    }

    return S_OK;
}


HRESULT
MidiNetworkHost::ProcessOutgoingPackets()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // if the packet is a session packet, route to the appropriate session
    // otherwise, handle it here





    return S_OK;
}





HRESULT 
MidiNetworkHost::EstablishNewSession()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // TODO: Add a new session to the session list

    // TODO: Let the endpoint manager know to enumerate this new endpoint

    // TODO: We'll need to make sure we can instantiate this before getting the SWD ID back, so protocol negotiation works


    return S_OK;
}


HRESULT 
MidiNetworkHost::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    if (m_advertiser)
    {
        m_advertiser->Shutdown();
    }

    // TODO: send "bye" to all sessions, and then unbind the socket

    // TODO: Stop packet processing thread


    return S_OK;
}