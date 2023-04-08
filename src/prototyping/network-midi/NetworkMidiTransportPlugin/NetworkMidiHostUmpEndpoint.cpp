#include "pch.h"
#include "NetworkMidiHostUmpEndpoint.h"
#include "NetworkMidiHostUmpEndpoint.g.cpp"

#include "NetworkHostAdvertiser.h"

#include "NetworkMidiCommandPacketHelper.h"

#include <iostream>
#include <iomanip>

#include <chrono>
#include <thread>

#include "SocketHelpers.h"
#include "TimeHelpers.h"


#define LOOP_SLEEP_DURATION 100ms



namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::MidiMessageBuffer NetworkMidiHostUmpEndpoint::IncomingMidiMessages()
    {
        return _incomingMidiMessages;
    }
    winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::MidiMessageBuffer NetworkMidiHostUmpEndpoint::OutgoingMidiMessages()
    {
        return _outgoingMidiMessages;
    }



    hstring NetworkMidiHostUmpEndpoint::Id()
    {
        return _id;
    }


    winrt::Windows::Foundation::Collections::PropertySet NetworkMidiHostUmpEndpoint::Properties()
    {
        return _properties;
    }




    winrt::Windows::Foundation::IAsyncAction NetworkMidiHostUmpEndpoint::OnUdpPacketReceived(sock::DatagramSocket const& sender, sock::DatagramSocketMessageReceivedEventArgs const& args)
    {
      //  std::cout << " - DatagramSocket.MessageReceived" << std::endl;

        // For each command packet. This needs to be super fast
        // - If a ping, add a pong to the outgoing out-of-band queue, complete with address info
        // - if an invite, add it to the incoming invite queue
        // - if another recognized message
        //   - if there's a session active, add the message to the session's incoming queue
        //   - if there's no session for this address, add a NAK to the out-of-band queue
        // - If it's some other garbage, discard the packet and move on

        auto reader = args.GetDataReader();

        if (reader.UnconsumedBufferLength() < sizeof(uint32_t) * 2)
        {
            // not a message we understand. Needs to be at least the size of the 
            // MIDI header plus a command packet header. Really it needs to be larger, but
            // just trying to weed out blips

            std::cout << " - UDP Packet does not have the minimum amount of data. Discarding." << std::endl;

            co_return;
        }


        uint32_t udpHeader = reader.ReadUInt32();

        if (udpHeader != RequiredUdpPayloadHeader)
        {
            // not a message we understand

            std::cout << " - UDP Packet does not have MIDI header. Discarding." << std::endl;

            co_return;
        }


        // TODO: Socket read operations are blocking, so if there's bad data (command payload length not correct)
        // then we need to be able to handle that here. Otherwise, it just waits. Recommend first checking 
        // unconsumed buffer length before doing any set of reads for a command packet

        while (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
        {
            // grab the MIDI command packet header
            uint32_t commandHeaderWord = reader.ReadInt32();

            auto packetHeader = NetworkMidiCommandPacketHeader();
            packetHeader.HeaderWord = commandHeaderWord;


            std::string packetType = NetworkMidiCommandPacketHelper::GetPacketDescription(packetHeader.HeaderWord);

            TimeHelpers::PrintCurrentTime();
            std::cout << std::setw(NetworkMidiCommandPacketHelper::MaxDescriptionWidth) << std::left << std::setfill(' ') << packetType;
            std::cout << " IN : 0x" << std::hex << std::setw(8) << std::setfill('0') << commandHeaderWord;
            std::cout << " | 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packetHeader.HeaderData.CommandCode);
            std::cout << " | Pay Len: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packetHeader.HeaderData.CommandPayloadLength);
            std::cout << " | Cmd Spec: 0x" << std::hex << std::setw(4) << std::setfill('0') << packetHeader.HeaderData.CommandSpecificData.AsUInt16;
            std::cout << " | From " << winrt::to_string(args.RemoteAddress().ToString());
            std::cout << " : " << winrt::to_string(args.RemotePort());
            std::cout << std::endl;


            uint8_t payloadWordsToRead = packetHeader.HeaderData.CommandPayloadLength;

            // shove the packet on the right queue

            switch (packetHeader.HeaderData.CommandCode)
            {
            case 0x01:  // Invitation
            {
                // Invitations are always handled centrally here.

             //   std::cout << "Enqueuing Incoming Invitation Message" << std::endl;

                NetworkMidiOutOfBandIncomingCommandPacket packet;
                packet.SourceHostName = args.RemoteAddress();
                packet.SourcePort = args.RemotePort();
                packet.Header = packetHeader;
                packet.SetMinimumBufferDataSizeAndAlign(packetHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t));
                reader.ReadBytes(packet.DataBuffer);

                _incomingOutOfBandCommands.push(packet);
            }
            break;

            case 0xFF:  // MIDI
            {
                // TODO: Going to need to handle incoming retransmits
                // TODO: need to make sure we get packets in sequence.

 
              //  std::cout << "Command packet is UMP" << std::endl;

                if (SessionAlreadyExists(args.RemoteAddress(), args.RemotePort()))
                {
                    auto sessionKey = CreateSessionMapKey(args.RemoteAddress(), args.RemotePort());

                    _activeSessions[sessionKey].LastIncomingMessageTime = std::chrono::steady_clock::now();

                    uint32_t midiWord;
                    while (payloadWordsToRead > 0 && SocketHelpers::CheckedReadUInt32(reader, midiWord))
                    {
                        // std::cout << ">> Wrote " << std::dec << wordsWritten << " MIDI message words to MIDI input stream" << std::endl;
                        
                        _incomingMidiMessages.WriteWord(midiWord);
                        payloadWordsToRead--;
                    }
                }
                else
                {
                    // TODO: Send a NAK
                    std::cout << "UMP received, but no active session for this client." << std::endl;

                    // drain the buffer
                    reader.ReadBuffer(payloadWordsToRead * sizeof(uint32_t));
                }


            }
            break;
            case 0x20:  // Ping
            {
             //   std::cout << "Enqueuing Incoming Ping Message" << std::endl;

                // should check to see if in a session. If so, send it there to handle.
                NetworkMidiOutOfBandIncomingCommandPacket packet;
                packet.SourceHostName = args.RemoteAddress();
                packet.SourcePort = args.RemotePort();
                packet.Header = packetHeader;
                packet.SetMinimumBufferDataSizeAndAlign(packetHeader.HeaderData.CommandPayloadLength * sizeof(uint32_t));
                reader.ReadBytes(packet.DataBuffer);

                _incomingOutOfBandCommands.push(packet);
            }
            break;

            case 0x21:  // Pong
                // we need to handle these, likely at the session level
                // skip payload
                reader.ReadBuffer(payloadWordsToRead * sizeof(uint32_t));
                break;

            default:
                // skip payload
                reader.ReadBuffer(payloadWordsToRead * sizeof(uint32_t));
                break;
            }
        }

        co_return;
    }



    winrt::Windows::Foundation::IAsyncAction NetworkMidiHostUmpEndpoint::StartAsync(
        hstring hostName,
        hstring port,
        hstring midiEndpointName,
        hstring midiProductInstanceId,
        bool advertise, 
        hstring serviceInstanceName)
    {
        // pulled these numbers out of air. Will need to use real numbers
        //_incomingMidiMessages.Size(1001 * sizeof(uint32_t));
        //_outgoingMidiMessages.Size(1001 * sizeof(uint32_t));


        using namespace std::chrono_literals;   // for the temporary sleep

        std::cout << "StartAsServer" << std::endl;

        _midiEndpointName = midiEndpointName;

        // std::cout << "Generating Placeholder Id" << std::endl;

        _id = GenerateEndpointDeviceId(L"HOSTSERVER", hostName, port, midiEndpointName, midiProductInstanceId);

        std::cout << winrt::to_string(_id) << std::endl;

        // was a port specified? If not, figure out a port to host this.
        
        //if (port.empty())
        //{
            // TODO: Algorithm to find a free socket

            //port = L"13337";
        //}


        //std::cout << "adding properties" << std::endl;

        // TEMP -----------------------------------------------
        // WinRT can't expose constants, so maybe create a WinRT class with a bunch
        // of property getters that represent the different property keys. That becomes
        // part of the API contract, though, and so it can't be added to easily. So
        // it may be that documentation (ugh) or some language-specific header files
        // are the way to go.
        _properties.Insert(PKEY_Id, winrt::box_value(_id));
        _properties.Insert(PKEY_MidiEndpointName, winrt::box_value(midiEndpointName));
        _properties.Insert(PKEY_MidiProductInstanceId, winrt::box_value(midiProductInstanceId));

        _properties.Insert(PKEY_Server_ServiceInstanceName, winrt::box_value(serviceInstanceName));
        _properties.Insert(PKEY_Server_HostName, winrt::box_value(hostName));
        _properties.Insert(PKEY_Server_Port, winrt::box_value(port));
        // TEMP -----------------------------------------------



        // return from this function now, but spin everything else off into a threadpool thread
        //std::cout << "Async return" << std::endl;
        co_await winrt::resume_background();



        // this should have error checking
        auto portNumber = (uint16_t)std::stoi(winrt::to_string(port));
         
         
        std::cout << "Binding UDP Socket" << std::endl;
        // create and bind the socket
        auto socket = sock::DatagramSocket();
        co_await socket.BindServiceNameAsync(port);

 
        if (hostName.empty())
        {
            // todo: find the best local host name to use like I did in C#
        }

        // this should come from the consumer as a parameter
        auto hostNameObject = networking::HostName(hostName);

        // std::cout << "Created HostName" << std::endl;


        // if we should advertise, do so

        if (advertise)
        {
            std::cout << "About to Advertise on mDNS" << std::endl;

            auto ad = NetworkHostAdvertiser();
            if (!co_await ad.AdvertiseAsync(serviceInstanceName, hostNameObject, socket, portNumber, midiEndpointName, midiProductInstanceId))
            {
                // failed to advertise
                std::cout << "Failed to advertise" << std::endl;
            }
        }


        // TODO: Need to create the listener, spin up thread to watch the socket, etc.

        // There will be only one UMP Endpoint for the given host/port. It's multi-client, though so
        // any number of apps can send to it or listen for new messages. It's like a USB device
        // in that way. Where it's not like a USB device, is that any number of network clients
        // can also connect to this, making it more of a hub/spoke model than a point to point or 
        // a fan-out approach. We'll offer the ability to limit the number of network clients
        // that connect, in case that's needed. The protocol has rejection messages built in.

//        std::cout << "About to connect DatagramSocket.MessageReceived event" << std::endl;
        socket.MessageReceived({ get_weak(), &NetworkMidiHostUmpEndpoint::OnUdpPacketReceived });
//        std::cout << "DatagramSocket.MessageReceived event connected" << std::endl;



        while (true)
        {
            //std::cout << ". ";

            if (!_incomingOutOfBandCommands.empty())
            {
                //std::cout << "- Processing incoming" << std::endl;

                auto packet = _incomingOutOfBandCommands.front();
                _incomingOutOfBandCommands.pop();

                switch (packet.Header.HeaderData.CommandCode)
                {
                case 0x01:  // Invitation
                    HandleInvitation(packet);
                    break;
                case 0x20:  // Ping
                    // should check to see if in a session. If so, send it there to handle.
                    HandleOutOfBandPing(packet);
                    break;
                default:
                    // send a nak
                    break;
                }

            }

            if (!_outgoingOutOfBandCommands.empty())
            {
                //std::cout << "- Sending outgoing" << std::endl;
                // we send each as short packets because, in most cases, we're not going to 
                // have batched traffic from the same endpoint

                // TODO: need to lock the socket while writing to it.
                // TODO: Need to lock hte queue while doing the front/pop

                auto packet = _outgoingOutOfBandCommands.front();
                _outgoingOutOfBandCommands.pop();


                auto packetType = NetworkMidiCommandPacketHelper::GetPacketDescription(packet.Header.HeaderWord);


                TimeHelpers::PrintCurrentTime();
                std::cout << std::setw(NetworkMidiCommandPacketHelper::MaxDescriptionWidth) << std::left << std::setfill(' ') << packetType;
                std::cout << " OUT: 0x" << std::hex << std::setw(8) << std::setfill('0') << packet.Header.HeaderWord;

                //std::cout << "Protocol Out: 0x" << std::hex << std::setw(8) << std::setfill('0') << packet.Header.HeaderWord;
                std::cout << " | 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packet.Header.HeaderData.CommandCode);
                std::cout << " | Pay Len: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packet.Header.HeaderData.CommandPayloadLength);
                std::cout << " | Cmd Spec: 0x" << std::hex << std::setw(4) << std::setfill('0') << packet.Header.HeaderData.CommandSpecificData.AsUInt16;
                std::cout << " |   To " << winrt::to_string(packet.DestinationHostName.ToString());
                std::cout << " : " << winrt::to_string(packet.DestinationPort);
                std::cout << std::endl;

                auto ostream = co_await socket.GetOutputStreamAsync(packet.DestinationHostName, packet.DestinationPort);

                auto writer = streams::DataWriter(ostream);

                // UDP MIDI header
                writer.WriteUInt32(RequiredUdpPayloadHeader);

                // packet header
                writer.WriteUInt32(packet.Header.HeaderWord);

                // packet data
                writer.WriteBytes(packet.DataBuffer);

                co_await writer.StoreAsync();

                //writer.Close();

            }


            // Loop Process incoming UDP packets
            // 
            // whenever there's a ping, look to see if there's an active session. 
            // - If not, reply with the active session flag set to 0
            // - if so, reply with the active session flag set to 1
            // If there's an invite, spin up a new session and accept the invite
            // If there's any other session message, but no active session with that ip/port, send NAK
            // if there's a bye message, terminate the session (host server stays up)
            // If there's a UMP packet, send it to the session to manage



            // Is it an invite? If so, set up a new session


            // Is it a ping? if so, queue up a pong, regardless if in or out of session


            // Is it a MIDI message? if so enqueue on the incoming UMP 

            // TODO: other types of messages



            // Loop process outgoing UDP packets
            // - For each outgoing queue, process outgoing messages up to one UDP packet in size
            // - 

            // each session has
            // - host name for the other end
            // - outgoing message queue for that session. Only for protocol messages
            // All incoming packets are handled centrally
            // All outgoing packets are handled centrally, but round-robin the outgoing queues in the session objects
            //  plus the outgoing out-of-band messages


            // for initial design, everything is going to happen in-line in a single loop.

            // 10 receive incoming UDP packets
            //socket.

            // 20 check incoming MIDI messages

            // 30 send outgoing MIDI messages

            // 40 send outgoing UDP packets

            // 50 temporary thread sleep. 
            // This should instead just wait for some activity if all the queues are empty


            if (_incomingOutOfBandCommands.empty() && _outgoingOutOfBandCommands.empty())
            {
                std::this_thread::sleep_for(LOOP_SLEEP_DURATION);
            }
        }

        co_return;
    }

    void NetworkMidiHostUmpEndpoint::HandleOutOfBandPing(NetworkMidiOutOfBandIncomingCommandPacket packet)
    {
        // return the ping with a matching pong

        //std::cout << "- Enqueuing Pong" << std::endl;

        auto responsePacket = NetworkMidiOutOfBandOutgoingCommandPacket();

        responsePacket.Header.HeaderData.CommandCode = NetworkMidiCommandCodePingReply;
        responsePacket.Header.HeaderData.CommandSpecificData.AsBytes.Byte1 = 0; // out of band ping always says session is not active
        responsePacket.Header.HeaderData.CommandSpecificData.AsBytes.Byte2 = 0;
        responsePacket.DestinationHostName = packet.SourceHostName;
        responsePacket.DestinationPort = packet.SourcePort;

        //responsePacket.SetMinimumBufferDataSize(packet.DataBuffer.size());
       
        responsePacket.CopyInData(packet.DataBuffer);

        _outgoingOutOfBandCommands.push(responsePacket);
    }

    void NetworkMidiHostUmpEndpoint::HandleInvitation(NetworkMidiOutOfBandIncomingCommandPacket packet)
    {
        std::string invitee(packet.DataBuffer.begin(), packet.DataBuffer.end()) ;

        std::cout << "Invitation from '" << invitee <<
          "' ("  + winrt::to_string(packet.SourceHostName.ToString())
            + ":" + winrt::to_string(packet.SourcePort) + ")" << std::endl;


        // We always accept here. Will want to change that to allow for limiting active sessions

        std::string nameUtf8 = winrt::to_string(_midiEndpointName) + '\0';    // convert to utf-8

        auto responsePacket = NetworkMidiOutOfBandOutgoingCommandPacket();

        // copy our endpoint name into the data. It does the 32 bit alignment + null automatically
        responsePacket.CopyInData(nameUtf8);

        responsePacket.Header.HeaderData.CommandCode = NetworkMidiCommandCodeInvitationReplyAccepted;
        responsePacket.Header.HeaderData.CommandSpecificData.AsUInt16 = 0;
        responsePacket.DestinationHostName = packet.SourceHostName;
        responsePacket.DestinationPort = packet.SourcePort;

       // std::cout << "Name length " << std::dec << nameUtf8.length() << std::endl;
       // std::cout << "Command Payload length (words) " << std::dec << static_cast<int>(responsePacket.Header.HeaderData.CommandPayloadLength) << std::endl;

        // create the session

        auto sessionKey = CreateSessionMapKey(packet.SourceHostName, packet.SourcePort);

        if (_activeSessions.find(sessionKey) == _activeSessions.end())
        {
            // session doesn't exist, so we add it
            std::cout << " - Session doesn't exist. Creating a new one" << std::endl;

            _activeSessions[sessionKey] = NetworkMidiHostSession();
            _activeSessions[sessionKey].StartAsync(responsePacket.DestinationHostName, responsePacket.DestinationPort);
        }
        else
        {
            // session already exists. We still reply, but no other action needed
            std::cout << " - Session already exists. Accepting again." << std::endl;
        }

        //std::cout << "- Enqueuing Invitation Accept: '" << nameUtf8 << "'" << std::endl;

        _outgoingOutOfBandCommands.push(responsePacket);
    }



}