#pragma once

#include<unordered_map>
#include<queue>
#include<string>

#include "NetworkMidiHostSession.h"
#include "NetworkMidiHostUmpEndpoint.g.h"

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

            return L"\\\\?\\SWD\\MMDEVAPI\\MIDI2_NETWORK_" + connectionType + L"\\" + hostName + L"_" + port + L"_" + midiEndpointName + L"_pii" + midiProductInstanceId;
        }

        //void OnUdpMessageReceived(sock::DatagramSocket const& sender, sock::DatagramSocketMessageReceivedEventArgs const& args)

        void HandleOutOfBandPing(NetworkMidiOutOfBandIncomingCommandPacket packet);
        void HandleInvitation(NetworkMidiOutOfBandIncomingCommandPacket packet);


        //winrt::Windows::Foundation::Collections::IVector<uint32_t> _incomingMessages;
        //winrt::Windows::Foundation::Collections::IVector<uint32_t> _outgoingMessages;

        streams::InMemoryRandomAccessStream _incomingMidiMessages = streams::InMemoryRandomAccessStream();
        streams::InMemoryRandomAccessStream _outgoingMidiMessages = streams::InMemoryRandomAccessStream();

        streams::DataWriter _incomingMidiMessagesWriter { _incomingMidiMessages };
        streams::DataReader _outgoingMidiMessagesReader { _outgoingMidiMessages };

    public:
        NetworkMidiHostUmpEndpoint() = default;

        //winrt::Windows::Foundation::Collections::IVector<uint32_t> IncomingMessages();
        //winrt::Windows::Foundation::Collections::IVector<uint32_t> OutgoingMessages();

                
        streams::IInputStream IncomingMessages();   // MIDI In
        streams::IOutputStream OutgoingMessages(); // MIDI Out

        hstring Id();
        winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Foundation::IInspectable> Properties();

        winrt::Windows::Foundation::IAsyncAction StartAsync(
            hstring hostName, hstring port, hstring midiEndpointName, hstring productInstanceId, bool advertise, hstring serviceInstanceName);
    };
}
namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::factory_implementation
{
    struct NetworkMidiHostUmpEndpoint : NetworkMidiHostUmpEndpointT<NetworkMidiHostUmpEndpoint, implementation::NetworkMidiHostUmpEndpoint>
    {
    };
}