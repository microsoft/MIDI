// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"


_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::Initialize(
    MidiNetworkConnectionRole const role,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& hostName,
    winrt::hstring const& port,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount
)
{
    m_role = role;

    m_remoteHostName = hostName;
    m_remotePort = port;

    // create the data writer
    m_writer = std::make_shared<MidiNetworkDataWriter>();
    RETURN_IF_NULL_ALLOC(m_writer);

    RETURN_IF_FAILED(m_writer->Initialize(socket.GetOutputStreamAsync(hostName, port).get()));

    m_thisEndpointName = thisEndpointName;
    m_thisProductInstanceId = thisProductInstanceId;

    m_retransmitBufferMaxCommandPacketCount = retransmitBufferMaxCommandPacketCount;

    // TODO: Initialize retransmit buffer

    return S_OK;
}

HRESULT
MidiNetworkConnection::HandleIncomingBye()
{
    // send bye reply
    RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
    RETURN_IF_FAILED(m_writer->WriteCommandByeReply());
    RETURN_IF_FAILED(m_writer->Send());


    // todo: close the session etc.


    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingInvitation(
    MidiNetworkCommandPacketHeader const& header,
    MidiNetworkCommandInvitationCapabilities const& capabilities,
    std::wstring const& clientUmpEndpointName,
    std::wstring const& clientProductInstanceId
)
{
    UNREFERENCED_PARAMETER(capabilities);

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_writer);

    if (m_role == MidiNetworkConnectionRole::ConnectionWindowsIsHost)
    {
        if (m_sessionActive)
        {
            // if the session is already active, we simply accept

            RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
            RETURN_IF_FAILED(m_writer->WriteCommandInvitationReplyAccepted(m_thisEndpointName, m_thisProductInstanceId));
            RETURN_IF_FAILED(m_writer->Send());

            return S_OK;
        }

        // TODO: will we accept a session invitation from the specified hostname?
        // TODO: Also need to check auth mechanism and follow instructions in 6.4 and send a Bye if not supported

        // todo: see if we already have a session active for this remote. If so, use it.
        // otherwise, we need to spin up a new session

        std::wstring newDeviceInstanceId{ };
        std::wstring newEndpointDeviceInterfaceId{ };

        if (TransportState::Current().GetEndpointManager()->IsInitialized())
        {
            // TODO: Create the session object


            // Create the endpoint for Windows MIDI Services clients
            // This will also kick off discovery and protocol negotiation
            HRESULT hr = S_OK;

            if (TransportState::Current().GetEndpointManager() == nullptr)
            {
                hr = E_UNEXPECTED;
            }
            else
            {
                hr = TransportState::Current().GetEndpointManager()->CreateNewEndpoint(
                    clientUmpEndpointName,
                    clientProductInstanceId,
                    m_remoteHostName,
                    m_remotePort,
                    newDeviceInstanceId,
                    newEndpointDeviceInterfaceId
                );
            }

            if (FAILED(hr))
            {
                // let the other side know that we can't create the session

                RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
                // TODO: Move string to resources for localization
                RETURN_IF_FAILED(m_writer->WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined, L"Error attempting to create endpoint. Bad data?"));
                RETURN_IF_FAILED(m_writer->Send());


                RETURN_IF_FAILED(hr);
            }

            // Tell the remote endpoint we've accepted the invitation

            RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
            RETURN_IF_FAILED(m_writer->WriteCommandInvitationReplyAccepted(m_thisEndpointName, m_thisProductInstanceId));
            RETURN_IF_FAILED(m_writer->Send());

            m_sessionActive = true;
        }
        else
        {
            // this shouldn't happen, but we handle it anyway

            RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
            // TODO: Move string to resources for localization
            RETURN_IF_FAILED(m_writer->WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined, L"Host is unable to accept invitations at this time."));
            RETURN_IF_FAILED(m_writer->Send());
        }
    }
    else
    {
        // we are a client, not a host, so NAK this per spec 6.4
        RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());

        // TODO: Move string to resources for localization
        RETURN_IF_FAILED(m_writer->WriteCommandNAK(header.HeaderWord, MidiNetworkCommandNAKReason::CommandNAKReason_CommandNotExpected, L"Unexpected invitation sent to client."));
        RETURN_IF_FAILED(m_writer->Send());
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingPing(uint32_t const pingId)
{
    RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
    RETURN_IF_FAILED(m_writer->WriteCommandPingReply(pingId));
    RETURN_IF_FAILED(m_writer->Send());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingPingReply(uint32_t const pingId)
{
    UNREFERENCED_PARAMETER(pingId);

    // todo: calculate latency and update the latency properties used in the scheduler
    

    return E_NOTIMPL;

}


_Use_decl_annotations_
std::wstring MidiNetworkConnection::ReadUtf8String(
    winrt::Windows::Storage::Streams::DataReader reader, 
    size_t const byteCount)
{
    auto bytes = std::vector<byte>(byteCount);
    reader.ReadBytes(bytes);

    std::string s(bytes.begin(), bytes.end());

#pragma warning (push)
#pragma warning (disable: 4996)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    auto ws = convert.from_bytes(s);
#pragma warning (pop)

    return ws;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::ProcessIncomingMessage(
    winrt::Windows::Storage::Streams::DataReader const& reader
)
{
    // todo: update last message time to now


    while (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
    {
        uint32_t commandHeaderWord = reader.ReadUInt32();

        MidiNetworkCommandPacketHeader commandHeader;
        commandHeader.HeaderWord = commandHeaderWord;

//        uint8_t payloadWordsToRead = packetHeader.HeaderData.CommandPayloadLength;


        switch (commandHeader.HeaderData.CommandCode)
        {
        case CommandCommon_NAK:
            // we should probably do something with this based on the last commands sent
            break;

        case CommandCommon_Ping:
            HandleIncomingPing(reader.ReadUInt32());
            break;

        case CommandCommon_PingReply:
            HandleIncomingPingReply(reader.ReadUInt32());
            break;

        case CommandCommon_Bye:
            LOG_IF_FAILED(HandleIncomingBye());
            break;

        case CommandCommon_ByeReply:
            // don't need to do anything with this
            break;

        case CommandClientToHost_Invitation:
        {
            uint16_t endpointNameLengthInBytes = commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte1 * sizeof(uint32_t);
            uint16_t productInstanceIdLengthInBytes = commandHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t) - endpointNameLengthInBytes;
            auto capabilities = static_cast<MidiNetworkCommandInvitationCapabilities>(commandHeader.HeaderData.CommandSpecificData.AsBytes.Byte2);

            auto clientEndpointName = ReadUtf8String(reader, endpointNameLengthInBytes);
            auto clientProductInstanceId = ReadUtf8String(reader, productInstanceIdLengthInBytes);

            LOG_IF_FAILED(HandleIncomingInvitation(commandHeader, capabilities, clientEndpointName, clientProductInstanceId));
        }
            break;


        case CommandClientToHost_InvitationWithAuthentication:
            break;

        case CommandClientToHost_InvitationWithUserAuthentication:
        //{
        //    MidiNetworkOutOfBandIncomingCommandPacket packet{};

        //    packet.SourceHostName = args.RemoteAddress().ToString();
        //    packet.SourcePort = args.RemotePort();
        //    packet.Header = packetHeader;

        //    // command payload length is given in 32 bit words
        //    auto expectedDataByteCount = packetHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t);

        //    // Socket read operations are blocking, so if there's bad data (command payload length not correct)
        //    // then we need to be able to handle that here. Otherwise, it just waits.
        //    if (reader.UnconsumedBufferLength() < expectedDataByteCount)
        //    {
        //        // TODO: packets must be complete by spec, but not sure if Windows might chunk them
        //        // in the reader. TBD.
        //    }
        //    else
        //    {
        //        // TODO: We need to read only the expectedDataByteCount. There could be
        //        // more than one command packet in the buffer

        //        packet.SetMinimumBufferDataSizeAndAlign(packetHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t));
        //        reader.ReadBytes(packet.DataBuffer);

        //        m_incomingOutOfBandCommands.push(packet);
        //    }
        //}
            break;

        case CommandCommon_UmpData:
            // TODO: check the UMP message sequence number. If it is lower than the last 
            // received sequence number (accounting for wrap), then just ignore.
            // If it is the last received sequence number + 1, then the session can process 
            // them immediately. FEC is also covered by this as we walk the messages.
            // If we have a gap in the sequence numbers, request a retransmit of the missing
            // message data packet(s). This will require either holding on to the current 
            // packet or discarding it and re-requesting the missing plus this packet.
            // Note: The packet number is only 16 bits, so it'll wrap. Need to ensure we
            // handle that here.

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

            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unexpected network MIDI 2.0 command code", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt8(commandHeader.HeaderData.CommandCode, "Command Code")
            );

            break;

        }

    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::StoreUmpPacketInRetransmitBuffer(uint16_t const sequenceNumber, std::vector<uint32_t> const& words)
{
    UNREFERENCED_PARAMETER(sequenceNumber);
    UNREFERENCED_PARAMETER(words);

    // TODO: this is also responsible for keeping the retransmit buffer a reasonable size by removing old entries

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::RetransmitBufferedPackets(uint16_t const startingSequenceNumber, uint16_t const retransmitPacketCount)
{
    UNREFERENCED_PARAMETER(startingSequenceNumber);
    UNREFERENCED_PARAMETER(retransmitPacketCount);

    return S_OK;
}



_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendMidiMessagesToNetwork(std::vector<uint32_t> const& words)
{
    auto sequenceNumber = IncrementAndGetNextUmpSequenceNumber();
    LOG_IF_FAILED(StoreUmpPacketInRetransmitBuffer(sequenceNumber, words));

    RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
    
    // todo: write FEC packets by using this sequence number and transmitting the past N packets
    // need to look at max UDP packet size and ensure we'll be under that

    RETURN_IF_FAILED(m_writer->WriteCommandUmpMessages(sequenceNumber, words));
    RETURN_IF_FAILED(m_writer->Send());

    return S_OK;
}




