#pragma once

#include<unordered_map>
#include<queue>
#include<string>

#include "NetworkMidiHostSession.h"
#include "NetworkMidiHostUmpEndpoint.g.h"

// I don't like #defines, but this is easiest way to deal with this. No consts in IDL and no 
// messing about with the implementation type vs projected type. Could also have a dedicated
// class with these in it as const wchar_t* if that turns out to be more useful/safer
#define PKEY_Id                         L"Id"
#define PKEY_ConnectionType             L"ConnectionType"
#define PKEY_MidiEndpointName           L"MidiEndpointName"
#define PKEY_MidiProductInstanceId      L"MidiProductInstanceId"
#define PKEY_Server_Advertise           L"Server_Advertise"
#define PKEY_Server_ServiceInstanceName L"Server_ServiceInstanceName"
#define PKEY_Server_Port                L"Server_Port"
#define PKEY_Server_HostName            L"Server_HostName"


namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    // key is clientIP:port
    using NetworkMidiHostSessionMap = std::unordered_map <const std::string, NetworkMidiHostSession, std::hash<std::string>>;

    struct NetworkMidiHostUmpEndpoint : NetworkMidiHostUmpEndpointT<NetworkMidiHostUmpEndpoint>
    {
    private:
        const uint32_t RequiredUdpPayloadHeader = 0x4D494449; // "MIDI" in ASCII
        hstring _midiEndpointName;
        NetworkMidiHostSessionMap _activeSessions ;
        hstring _id;

        std::queue<NetworkMidiOutOfBandIncomingCommandPacket> _incomingOutOfBandCommands;
        std::queue<NetworkMidiOutOfBandOutgoingCommandPacket> _outgoingOutOfBandCommands;

        inline std::string CreateSessionMapKey(networking::HostName hostName, winrt::hstring port)
        {
            return winrt::to_string(hostName.ToString() + L":" + port);
        }

        inline bool SessionAlreadyExists(networking::HostName hostName, winrt::hstring port)
        {
            auto key = CreateSessionMapKey(hostName, port);

            return _activeSessions.find(key) != _activeSessions.end();
        }

        inline hstring GenerateEndpointDeviceId(hstring connectionType, hstring hostName, hstring port, hstring midiEndpointName, hstring midiProductInstanceId)
        {
            // this will need to actually conform with how Windows does it today. For now,
            // it just kind of looks like it, if you squint a bit
            // Will be updated once we decide on PnP and enumeration approach

            return L"\\\\?\\SWD\\MMDEVAPI\\PROTOTYPE_MIDI2_NETWORK_" + connectionType + L"\\" + hostName + L"_" + port + L"_" + midiEndpointName + L"_" + midiProductInstanceId;
        }

        void HandleOutOfBandPing(NetworkMidiOutOfBandIncomingCommandPacket packet);
        void HandleInvitation(NetworkMidiOutOfBandIncomingCommandPacket packet);

        winrt::Windows::Foundation::IAsyncAction OnUdpPacketReceived(sock::DatagramSocket const& sender, sock::DatagramSocketMessageReceivedEventArgs const& args);

        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::MidiMessageBuffer _incomingMidiMessages;
        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::MidiMessageBuffer _outgoingMidiMessages;

        collections::PropertySet _properties = collections::PropertySet();

    public:
        NetworkMidiHostUmpEndpoint() = default;




        hstring Id();
        collections::PropertySet Properties();
                
        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::MidiMessageBuffer IncomingMidiMessages();   // MIDI In
        winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::MidiMessageBuffer OutgoingMidiMessages(); // MIDI Out

        winrt::Windows::Foundation::IAsyncAction StartAsync(
            hstring hostName, 
            hstring port, 
            hstring midiEndpointName, 
            hstring midiProductInstanceId, 
            bool advertise, 
            hstring serviceInstanceName);
    };
}
namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::factory_implementation
{
    struct NetworkMidiHostUmpEndpoint : NetworkMidiHostUmpEndpointT<NetworkMidiHostUmpEndpoint, implementation::NetworkMidiHostUmpEndpoint>
    {
    };
}