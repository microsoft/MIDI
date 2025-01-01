// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"


// TODO: Change to a loop
// - Incoming messages should go into a list
// - Loop will check the list and send out whatever is in there (requires that the list has locks to prevent reading partial UMPs)
// - Loop should also handle a regular outgoing ping
// - Loop should also handle sending keep-alive empty UDP packets




_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::Initialize(
    MidiNetworkConnectionRole const role,
    winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
    winrt::Windows::Networking::HostName const& hostName,
    winrt::hstring const& port,
    std::wstring const& thisEndpointName,
    std::wstring const& thisProductInstanceId,
    uint16_t const retransmitBufferMaxCommandPacketCount,
    uint8_t const maxForwardErrorCorrectionCommandPacketCount
)
{
    m_sessionActive = false;

    m_role = role;

    m_remoteHostName = hostName;
    m_remotePort = port;

    m_thisEndpointName = thisEndpointName;
    m_thisProductInstanceId = thisProductInstanceId;

    m_retransmitBufferMaxCommandPacketCount = retransmitBufferMaxCommandPacketCount;
    m_maxForwardErrorCorrectionCommandPacketCount = maxForwardErrorCorrectionCommandPacketCount;

    // build out the retransmit buffer used for FEC and retransmit requests
    try
    {
        m_retransmitBuffer.set_capacity(max(m_retransmitBufferMaxCommandPacketCount, m_maxForwardErrorCorrectionCommandPacketCount));
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_OUTOFMEMORY);
    }

#pragma push_macro("max")
#undef max
    m_lastSentUmpCommandSequenceNumber = std::numeric_limits<uint16_t>::max();    // init to this so first real one is zero
#pragma pop_macro("max")

    // create the data writer
    m_writer = std::make_shared<MidiNetworkDataWriter>();
    RETURN_IF_NULL_ALLOC(m_writer);
    RETURN_IF_FAILED(m_writer->Initialize(socket.GetOutputStreamAsync(hostName, port).get()));

    return S_OK;
}




_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::SetMidiCallback(
    IMidiCallback* callback
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    m_callback = callback;

    return S_OK;
}

HRESULT
MidiNetworkConnection::RemoveMidiCallback()
{
    m_callback = nullptr;

    return S_OK;
}



_Use_decl_annotations_
HRESULT 
MidiNetworkConnection::HandleIncomingUmpData(
    uint64_t const timestamp,
    std::vector<uint32_t> const& words
)
{
    if (m_callback != nullptr)
    {
        // this may have more than one message, so we need to tease it apart here
        // and send the individual messages

        auto it = words.begin();

        while (it < words.end())
        {
            uint8_t messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(*it);

            LOG_IF_FAILED(m_callback->Callback(
                (PVOID)(&(*it)),
                (UINT)(messageWordCount * sizeof(uint32_t)),
                timestamp, 
                (LONGLONG)0));            // todo: may need to pass along the context

            it += messageWordCount;
        }
    }

    return S_OK;
}



HRESULT
MidiNetworkConnection::HandleIncomingBye()
{
    // send bye reply
    RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
    RETURN_IF_FAILED(m_writer->WriteCommandByeReply());
    RETURN_IF_FAILED(m_writer->Send());

    if (m_sessionActive)
    {
        if (TransportState::Current().GetEndpointManager() != nullptr)
        {
            RETURN_IF_FAILED(TransportState::Current().GetEndpointManager()->DeleteEndpoint(m_sessionDeviceInstanceId));
            RETURN_IF_FAILED(TransportState::Current().RemoveSessionConnection(m_sessionEndpointDeviceInterfaceId));

            m_sessionActive = false;
            m_sessionDeviceInstanceId.clear();
            m_sessionEndpointDeviceInterfaceId.clear();
        }
    }
    else
    {
        // not an active session. Nothing to clean up
    }

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

            if (SUCCEEDED(hr))
            {
                m_sessionEndpointDeviceInterfaceId = newEndpointDeviceInterfaceId;
                m_sessionDeviceInstanceId = newDeviceInstanceId;

                std::shared_ptr<MidiNetworkConnection> connection;
                connection.reset(this);

                // this is what the BiDi uses when it is created
                TransportState::Current().AddSessionConnection(m_sessionEndpointDeviceInterfaceId, connection);

                // protocol negotiation needs to happen here, not in the endpoint creation
                // because we need to wire up the connection first. Bit of a race.

                LOG_IF_FAILED(TransportState::Current().GetEndpointManager()->InitiateDiscoveryAndNegotiation(m_sessionEndpointDeviceInterfaceId));
            }
            else
            {
                // let the other side know that we can't create the session

                auto lock = m_socketWriterLock.lock();

                RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
                // TODO: Move string to resources for localization
                RETURN_IF_FAILED(m_writer->WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined, L"Error attempting to create endpoint. Bad data?"));
                RETURN_IF_FAILED(m_writer->Send());

                // exit out of here, and log while we're at it
                RETURN_IF_FAILED(hr);
            }

            // Tell the remote endpoint we've accepted the invitation

            auto lock = m_socketWriterLock.lock();

            RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
            RETURN_IF_FAILED(m_writer->WriteCommandInvitationReplyAccepted(m_thisEndpointName, m_thisProductInstanceId));
            RETURN_IF_FAILED(m_writer->Send());

            m_sessionActive = true;
        }
        else
        {
            // this shouldn't happen, but we handle it anyway

            auto lock = m_socketWriterLock.lock();

            RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());
            // TODO: Move string to resources for localization
            RETURN_IF_FAILED(m_writer->WriteCommandBye(MidiNetworkCommandByeReason::CommandByeReasonCommon_Undefined, L"Host is unable to accept invitations at this time."));
            RETURN_IF_FAILED(m_writer->Send());
        }
    }
    else
    {
        // we are a client, not a host, so NAK this per spec 6.4
        auto lock = m_socketWriterLock.lock();
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
    auto lock = m_socketWriterLock.lock();

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
    // todo: update last message time to this timestamp
    uint64_t packetMidiTimestamp = internal::GetCurrentMidiTimestamp();

    while (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
    {
        uint32_t commandHeaderWord = reader.ReadUInt32();

        MidiNetworkCommandPacketHeader commandHeader;
        commandHeader.HeaderWord = commandHeaderWord;

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
            break;

        case CommandCommon_UmpData:
        {
            uint8_t numberOfWords = commandHeader.HeaderData.CommandPayloadLength;
            MidiSequenceNumber sequenceNumber(commandHeader.HeaderData.CommandSpecificData.AsUInt16);

            std::vector<uint32_t> words{ };

            // TODO: a message can have zero MIDI words, but a valid sequence number. Need to handle that.


            // TODO: This logic doesn't handle wrap. Need to have a window
            if (sequenceNumber <= m_lastReceivedUmpCommandSequenceNumber)
            {


                // todo: skip all words
            }
            else if (sequenceNumber == m_lastReceivedUmpCommandSequenceNumber + 1)
            {
                // todo: process UMP data


                m_lastReceivedUmpCommandSequenceNumber = sequenceNumber;
            }
            else
            {
                // skipped data. Re-request missing packets
            }








            for (uint8_t i = 0; i < numberOfWords; i++)
            {
                if (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
                {
                    // TODO: We need to check this sequence number, and if there's any gap from the last received, drop this data and request a retransmit

                    if (sequenceNumber > m_lastReceivedUmpCommandSequenceNumber)
                    {
                        // add to our vector
                        words.push_back(reader.ReadUInt32());

                    }
                    else
                    {
                        // just read and discard
                        reader.ReadUInt32();
                    }
                }
                else
                {
                    // bad / incomplete packet

                    auto lock = m_socketWriterLock.lock();
                    m_writer->WriteUdpPacketHeader();
                    m_writer->WriteCommandNAK(commandHeader.HeaderWord, MidiNetworkCommandNAKReason::CommandNAKReason_CommandMalformed, L"Packet data incomplete");
                    m_writer->Send();

                    // TODO: We should request a retransmit of this packet

                    return E_FAIL;
                }
            }

            if (sequenceNumber > m_lastReceivedUmpCommandSequenceNumber)
            {
                m_lastReceivedUmpCommandSequenceNumber = sequenceNumber;
            }

            if (words.size() > 0)
            {
                LOG_IF_FAILED(HandleIncomingUmpData(packetMidiTimestamp, words));
            }
        }
            break;

        case CommandCommon_RetransmitRequest:
        {
            //uint8_t payloadLengthWords = commandHeader.HeaderData.CommandPayloadLength;
            uint16_t sequenceNumber = commandHeader.HeaderData.CommandSpecificData.AsUInt16;
            uint16_t numberOfUmpCommands = reader.ReadUInt16();

            reader.ReadUInt16();    // reserved

            LOG_IF_FAILED(HandleIncomingRetransmitRequest(sequenceNumber, numberOfUmpCommands));

        }
            break;
        case CommandCommon_RetransmitError:
            // TODO: handle retransmitting data
            break;

        case CommandCommon_SessionReset:
        case CommandCommon_SessionResetReply:
            // TODO: handle session reset
            break;

        default:
            // TODO: unexpected command code. Send NAK.

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
MidiNetworkConnection::AddUmpPacketToRetransmitBuffer(MidiSequenceNumber const sequenceNumber, std::vector<uint32_t> const& words)
{
    MidiRetransmitBufferEntry entry;
    entry.SequenceNumber = sequenceNumber;
    entry.Words = words;

    m_retransmitBuffer.push_back(entry);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkConnection::HandleIncomingRetransmitRequest(uint16_t const startingSequenceNumber, uint16_t const retransmitPacketCount)
{
    // TODO: A packet count of 0x0000 means to send all the data we have starting at the startingSequenceNumber

    // find the starting sequence number in the circular buffer

    auto firstPacket = std::find_if(m_retransmitBuffer.begin(), m_retransmitBuffer.end(), [&](const MidiRetransmitBufferEntry& s) { return s.SequenceNumber == startingSequenceNumber; });

    auto lock = m_socketWriterLock.lock();

    if (firstPacket == m_retransmitBuffer.end())
    {

        // Send a retransmit error

        RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());

        if (m_retransmitBuffer.size() > 0)
        {
            RETURN_IF_FAILED(m_writer->WriteCommandRetransmitError(m_retransmitBuffer.begin()->SequenceNumber, MidiNetworkCommandRetransmitErrorReason::RetransmitErrorReason_DataNotAvailable));
        }
        else
        {
            RETURN_IF_FAILED(m_writer->WriteCommandRetransmitError(0, MidiNetworkCommandRetransmitErrorReason::RetransmitErrorReason_DataNotAvailable));
        }

        RETURN_IF_FAILED(m_writer->Send());
    }
    else
    {
        RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());

        auto endPacket = firstPacket + retransmitPacketCount;

        if (retransmitPacketCount == 0x0000)
        {
            endPacket = m_retransmitBuffer.end();
        }

        for (auto& it = firstPacket; it != m_retransmitBuffer.end() && it < endPacket && it->SequenceNumber >= startingSequenceNumber; it++)
        {
            // TODO: We need to account for UDP packet size here

            RETURN_IF_FAILED(m_writer->WriteCommandUmpMessages(it->SequenceNumber, it->Words));
        }

        RETURN_IF_FAILED(m_writer->Send());
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendMidiMessagesToNetwork(std::vector<uint32_t> const& words)
{
    auto sequenceNumber = ++m_lastSentUmpCommandSequenceNumber;

    auto lock = m_socketWriterLock.lock();

    RETURN_IF_FAILED(m_writer->WriteUdpPacketHeader());

    // TODO: We need to ensure we don't go over the UDP max packet size
    if (m_retransmitBuffer.size() > 0)
    {
        // write the Forward Error Correction packets using the retransmit buffer stored data

        size_t numberOfForwardErrorCorrectionPackets{ 0 };
        numberOfForwardErrorCorrectionPackets = min(m_retransmitBuffer.size(), m_maxForwardErrorCorrectionCommandPacketCount);

        for (size_t pos = m_retransmitBuffer.size() - numberOfForwardErrorCorrectionPackets; pos < m_retransmitBuffer.size(); pos++)
        {
            RETURN_IF_FAILED(m_writer->WriteCommandUmpMessages(m_retransmitBuffer.at(pos).SequenceNumber, m_retransmitBuffer.at(pos).Words));
        }
    }

    // write the actual data we have
    RETURN_IF_FAILED(m_writer->WriteCommandUmpMessages(sequenceNumber, words));
    RETURN_IF_FAILED(m_writer->Send());

    // everything has gone well, so add this to our retransmit buffer
    LOG_IF_FAILED(AddUmpPacketToRetransmitBuffer(sequenceNumber, words));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkConnection::SendMidiMessagesToNetwork(
    PVOID const bytes,
    UINT const byteCount)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, bytes);
    RETURN_HR_IF(E_INVALIDARG, byteCount < sizeof(uint32_t));

    std::vector<uint32_t> words{ };
    uint32_t* wordPointer{ static_cast<uint32_t*>(bytes) };
    size_t wordCount{ byteCount / sizeof(uint32_t) };

    words.insert(words.end(), wordPointer, wordPointer + wordCount);

    return SendMidiMessagesToNetwork(words);
}


