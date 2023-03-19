using Spectre.Console;
using System;
using System.Buffers.Text;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Reflection.PortableExecutable;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using Windows.Networking;
using Windows.Networking.Sockets;
using Windows.Storage.Streams;


// TODO: This code currently assumes a single client. But the protocol specs
// read that any number of clients can connect to the same host, so this needs
// to be multi-client at a network level (this is above and beyond Windows API
// UMP clients using this UMP Endpoint, but more about network entities connecting
// to the UMP endpoint).


namespace NetworkMidi2ServerProto
{
    public class NetworkMidi2CommandPacket
    {
        public HostName SourceHostName { get; set; }
        public string SourcePort { get; set; }


        public byte CommandCode { get; set; }
        public byte PayloadLengthInWords { get; set; }
        public int PayloadLengthInBytes => PayloadLengthInWords * sizeof(UInt32);
        public byte TotalCommandPacketSizeInBytes => (byte)(4 + PayloadLengthInBytes);

        public byte CommandSpecificData0 { get; set; }
        public byte CommandSpecificData1 { get; set; }

        // expose this as a member, not a property, because we resize it
        public byte[] Data;

        public string FriendlyPacketType
        {
            get
            {
                switch (CommandCode)
                {
                    case 0xff:
                        return "UMP";
                    case 0x01:
                        return "Invitation";
                    case 0x02:
                        return "Invitation with Authentication";
                    case 0x03:
                        return "Invitation with User Authentication";
                    case 0x10:
                        return "Invitation Reply: Accepted";
                    case 0x11:
                        return "Invitation Reply: Pending";
                    case 0x12:
                        return "Invitation Reply: Authentication Required";
                    case 0x13:
                        return "Invitation Reply: User Authentication Required";
                    case 0x20:
                        return "Ping";
                    case 0x21:
                        return "Pong";
                    case 0x80:
                        return "Retransmit";
                    case 0x81:
                        return "Retransmit Error";
                    case 0x82:
                        return "Report";
                    case 0x8F:
                        return "NAK";
                    case 0xF0:
                        return "Bye";
                    case 0xF1:
                        return "Bye Reply";
                    default:
                        return "Unknown";
                }
            }
        }

    }



    public class PacketProcessor
    {
        public bool IsSessionEstablished { get; private set; }

        public HostName ConnectedDestinationHostName { get; set; }
        public string ConnectedDestinationPort { get; set; }

        // this appears at the start of all udp packets
        private const UInt32 UdpPayloadHeader = 0x4D494449; // "MIDI" in ASCII
        private readonly byte[] UdpPayloadHeaderBytes = new byte[] { 0x4d, 0x49, 0x44, 0x49 };


        private const UInt16 UdpPacketMaxSizeInBytes = 1400;

        private DatagramSocket _socket;

        // not what you'd use in production, of course
        private Queue<NetworkMidi2CommandPacket> _incomingPacketQueue = new Queue<NetworkMidi2CommandPacket>(500);
        private Queue<NetworkMidi2CommandPacket> _outgoingPacketQueue = new Queue<NetworkMidi2CommandPacket>(500);

        private string _connectedClientEndpointName = string.Empty;
        private string _thisHostEndpointName = string.Empty;

        public PacketProcessor(DatagramSocket socket, string thisHostEndpointName)
        {
            _socket = socket;
            _thisHostEndpointName = thisHostEndpointName;
        }

        private async void OnSocketMessageReceived(DatagramSocket sender, DatagramSocketMessageReceivedEventArgs args)
        {
            ProcessIncomingPackets(args.RemoteAddress, args.RemotePort, args.GetDataReader());
        }


        public void Run()
        {
            // this is all serial here, but you'd want different incoming/outgoing processing threads

            _socket.MessageReceived += OnSocketMessageReceived;

            while (true)
            {
                Thread.Sleep(200);

                while (_incomingPacketQueue.Count > 0)
                {
                    AnsiConsole.MarkupLine("[grey39]Incoming queue count =[/] {0}", _incomingPacketQueue.Count);

                    NetworkMidi2CommandPacket packet;

                    lock (_incomingPacketQueue)
                    {
                        packet = _incomingPacketQueue.Dequeue();
                    }

                    ProcessCommandPacket(packet);
                    AnsiConsole.MarkupLine("");
                }

                SendOutgoingPackets(ConnectedDestinationHostName, ConnectedDestinationPort);

            }
        }


        private async void SendOutgoingPackets(HostName destinationHostName, string destinationPort)
        {
            int currentUdpPacketLength = 0;
            //bool headerSent = false;


            if (IsSessionEstablished)
            {
                var ostream = await _socket.GetOutputStreamAsync(destinationHostName, destinationPort);

                using (var writer = new DataWriter(ostream))
                {
                    while (_outgoingPacketQueue.Count > 0)
                    {
                        AnsiConsole.MarkupLine("[grey39]Outgoing queue count =[/] {0}", _outgoingPacketQueue.Count);

                        NetworkMidi2CommandPacket packet;

                        lock (_outgoingPacketQueue)
                        {
                            packet = _outgoingPacketQueue.Dequeue();
                        }

                        if (currentUdpPacketLength == 0)
                        {
                            // we haven't yet sent the header, so do so.

                            WriteUdpPacketHeader(writer);
                            currentUdpPacketLength = UdpPayloadHeaderBytes.Length;
                        }
                        else if (currentUdpPacketLength + packet.TotalCommandPacketSizeInBytes > UdpPacketMaxSizeInBytes)
                        {
                            // Adding this command will make the UDP packet too bit. So
                            // send what we have and start a new UDP packet

                            TransmitUdpPacket(writer);

                            // start up a new packet
                            WriteUdpPacketHeader(writer);
                            currentUdpPacketLength = UdpPayloadHeaderBytes.Length;
                        }

                        // Add the MIDI network protocol packet into the UDP packet

                        WriteCommandPacket(writer, packet);
                        currentUdpPacketLength += packet.TotalCommandPacketSizeInBytes;
                    }

                    // we're all done processing the queue. If we have command packets ready to send, send them
                    if (currentUdpPacketLength > 0)
                    {
                        TransmitUdpPacket(writer);
                    }
                }
            }
        }

        private void WriteUdpPacketHeader(DataWriter writer)
        {
            writer.WriteUInt32(UdpPayloadHeader);
        }

        private async void WriteCommandPacket(DataWriter writer, NetworkMidi2CommandPacket packet)
        {
            AnsiConsole.MarkupLine("Writing Command Packet [green]0x{0:X2} {1}[/]", packet.CommandCode, packet.FriendlyPacketType);

            // command packet
            writer.WriteByte(packet.CommandCode);
            writer.WriteByte(packet.PayloadLengthInWords);
            writer.WriteByte(packet.CommandSpecificData0);
            writer.WriteByte(packet.CommandSpecificData1);

            writer.WriteBytes(packet.Data);
        }

        private async void TransmitUdpPacket(DataWriter writer)
        {
            await writer.StoreAsync();
        }

        public void ProcessIncomingPackets(HostName sourceHost, string sourcePort, DataReader reader)
        {

            UInt32 header = reader.ReadUInt32();
            if (header != UdpPayloadHeader)
            {
                // garbage. We need to drop this packet completely
                AnsiConsole.MarkupLine("[red]‼️[/] Incoming UDP packet does not start with \"MIDI\"");
            }
            else
            {
                AnsiConsole.MarkupLine("[green]☑️[/] Incoming UDP packet starts with \"MIDI\"");

                while (reader.UnconsumedBufferLength > 0)
                {
                    string outputString = string.Empty;

                    var packet = new NetworkMidi2CommandPacket();

                    // get the command code
                    packet.SourceHostName = sourceHost;
                    packet.SourcePort = sourcePort;
                    packet.CommandCode = reader.ReadByte();
                    packet.PayloadLengthInWords = reader.ReadByte();
                    packet.CommandSpecificData0 = reader.ReadByte();
                    packet.CommandSpecificData1 = reader.ReadByte();

                    //AnsiConsole.MarkupLine("Command Code: 0x{0:X2}: {1}", packet.CommandCode, packet.FriendlyPacketType);
                    //AnsiConsole.MarkupLine("Payload Length in Words: {0}", packet.PayloadLengthInWords);
                    //AnsiConsole.MarkupLine("Command Data: 0x{0:X2}, 0x{1:X2}", packet.CommandSpecificData0, packet.CommandSpecificData1);

                    packet.Data = new byte[packet.PayloadLengthInBytes];

                    int bytesRead = 0;
                    while (reader.UnconsumedBufferLength > 0 && bytesRead < packet.PayloadLengthInBytes)
                    {
                        var b = reader.ReadByte();

                        packet.Data[bytesRead] = b;

                        bytesRead++;

                        char outputChar;

                        if (b == 0)
                        {
                            outputChar = ' ';
                        }
                        else
                        {
                            outputChar = (char)b;
                        }

                        if (outputString.Length == 0)
                        {
                            outputString = "[yellow]>[/] [grey]|[/]";

                            AnsiConsole.Markup("[yellow]>[/] ");
                        }

                        outputString += outputChar + "[grey23]|[/]";

                        AnsiConsole.Markup("[grey23]0x[/]{0:X2} ", b);
                    }

                    AnsiConsole.MarkupLine("");
                   // AnsiConsole.MarkupLine(outputString);

                    lock (_incomingPacketQueue)
                    {
                        _incomingPacketQueue.Enqueue(packet);
                    }
                }
            }
        }

        // in the C++ version, the memory would just be allocated as a multiple of sizeof(uint32_t)
        // and zero-set before copying data
        private void PadArrayTo32BitBoundary(ref byte[] arr)
        {
            if (arr.Length % 4 != 0)
            {
                var oldSize = arr.Length;
                var newSize = arr.Length + 4 - (arr.Length % 4);

                Array.Resize<byte>(ref arr, newSize);

                // zero pad. In C++ just do pave over the part of memory
                for (int i = oldSize-1; i < newSize; i++)
                {
                    arr[i] = 0;
                }
            }
        }


        // In the real code, this will create a MIDI Endpoint, enumerated by the system
        // and available to all apps. It will also handle security
        // Also need to handle one-button invite
        private void HandleIncomingInvitation(NetworkMidi2CommandPacket packet)
        {
            if (IsSessionEstablished)
            {
                if (packet.SourceHostName.RawName != ConnectedDestinationHostName.RawName)
                {
                    // need to send a NO to this if we're single-client
                }
            }



            // Protocol says that we should reply to the invitation even if the session is already established.

            _connectedClientEndpointName = System.Text.Encoding.UTF8.GetString(packet.Data);

            AnsiConsole.MarkupLine("Invitation from: [green]{0}[/]", _connectedClientEndpointName);

            // Queue up outgoing response. For the prototype, we always accept the invitation

            NetworkMidi2CommandPacket responsePacket = new NetworkMidi2CommandPacket();
            responsePacket.CommandCode = 0x10;
            responsePacket.CommandSpecificData0 = 0;
            responsePacket.CommandSpecificData1 = 0;

            // the data is just the endpoint name
            responsePacket.Data = System.Text.Encoding.UTF8.GetBytes(_thisHostEndpointName + '\0');

            PadArrayTo32BitBoundary(ref responsePacket.Data);

            responsePacket.PayloadLengthInWords = (byte)(responsePacket.Data.Length / sizeof(UInt32));

            ConnectedDestinationHostName = packet.SourceHostName;
            ConnectedDestinationPort = packet.SourcePort;

            AnsiConsole.MarkupLine(" - [yellow]Enqueueing Accept response.[/]");

            _outgoingPacketQueue.Enqueue(responsePacket);

            IsSessionEstablished = true;
        }

        private void HandleIncomingPing(NetworkMidi2CommandPacket packet)
        {
            AnsiConsole.MarkupLine("Ping from: [green]{0}[/]", _connectedClientEndpointName);
            AnsiConsole.MarkupLine("Ping id: [green]0x{0:X2},0x{1:X2},0x{2:X2},0x{3:X2}[/]", packet.Data[0], packet.Data[1], packet.Data[2], packet.Data[3]);


            NetworkMidi2CommandPacket responsePacket = new NetworkMidi2CommandPacket();
            responsePacket.CommandCode = 0x21;
            responsePacket.CommandSpecificData0 = 1;    // session active
            responsePacket.CommandSpecificData1 = 0;
            responsePacket.Data = packet.Data;          // respond to ping with same data info

            PadArrayTo32BitBoundary(ref responsePacket.Data);

            responsePacket.PayloadLengthInWords = (byte)(responsePacket.Data.Length / sizeof(UInt32));

            //responsePacket.DestinationHostName = packet.SourceHostName;
            //responsePacket.DestinationPort = packet.SourcePort;

            AnsiConsole.MarkupLine(" - [yellow]Enqueueing pong.[/]");

            _outgoingPacketQueue.Enqueue(responsePacket);
        }

        private void HandleIncomingUmp(NetworkMidi2CommandPacket packet)
        {
            if (IsSessionEstablished)
            {
                AnsiConsole.MarkupLine("UMP from: [green]{0}[/]", _connectedClientEndpointName);
                AnsiConsole.MarkupLine("UMP Message Type: [green]0x{0:X1}[/]", (packet.Data[0] & 0xF0) >> 4);

                // UMP messages need to be sent into the UMP Endpoint's incoming message queue
                // No other handling here

                AnsiConsole.MarkupLine(" - [yellow]Sending to UMP Endpoint input queue[/]");

                // TODO
            }

        }


        private void ProcessCommandPacket(NetworkMidi2CommandPacket packet)
        {
            AnsiConsole.MarkupLine("Command Code: [green]0x{0:X2}: {1}[/]", packet.CommandCode, packet.FriendlyPacketType);

            switch (packet.CommandCode)
            {
                case 0xff:
                    HandleIncomingUmp(packet);
                    break;
                case 0x01:
                    HandleIncomingInvitation(packet);
                    break;
                    //case 0x02:
                    //    return "Invitation with Authentication";
                    //case 0x03:
                    //    return "Invitation with User Authentication";
                    //case 0x10:
                    //    return "Invitation Reply: Accepted";
                    //case 0x11:
                    //    return "Invitation Reply: Pending";
                    //case 0x12:
                    //    return "Invitation Reply: Authentication Required";
                    //case 0x13:
                    //    return "Invitation Reply: User Authentication Required";
                case 0x20:
                    HandleIncomingPing(packet);
                    break;
                    //    return "Ping";
                    //case 0x21:
                    //    return "Pong";
                    //case 0x80:
                    //    return "Retransmit";
                    //case 0x81:
                    //    return "Retransmit Error";
                    //case 0x82:
                    //    return "Report";
                    //case 0x8F:
                    //    return "NAK";
                    //case 0xF0:
                    //    return "Bye";
                    //case 0xF1:
                    //    return "Bye Reply";
                default:
                    //AnsiConsole.MarkupLine("Unknown");
                    AnsiConsole.MarkupLine(" - Not yet set up to handle message.");
                    return;
            }
        }




    }
}