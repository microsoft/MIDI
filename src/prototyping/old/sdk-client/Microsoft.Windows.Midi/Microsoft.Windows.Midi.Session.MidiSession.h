#pragma once
#include "Microsoft.Windows.Midi.Session.MidiSession.g.h"


namespace winrt::Microsoft::Windows::Midi::Session::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Microsoft::Windows::Midi::Session::MidiSession Create(hstring const& name, winrt::Microsoft::Windows::Midi::Session::MidiSessionCreateOptions const& options);
        static winrt::Microsoft::Windows::Midi::Session::MidiSession Create(hstring const& name);
        winrt::guid Id();
        hstring Name();
        winrt::Windows::Foundation::DateTime CreateTime();
        void Close();
        winrt::Microsoft::Windows::Midi::Devices::MidiEndpoint OpenEndpoint(winrt::Microsoft::Windows::Midi::Enumeration::MidiEndpointInformation const& information);
        winrt::Microsoft::Windows::Midi::Devices::MidiEndpoint OpenEndpoint(winrt::guid const& deviceId, winrt::guid const& endpointId);
        winrt::event_token MessageReceived(winrt::Windows::Foundation::EventHandler<winrt::Microsoft::Windows::Midi::MidiMessageReceivedEventArgs> const& handler);
        void MessageReceived(winrt::event_token const& token) noexcept;
        void SendUmp(winrt::Microsoft::Windows::Midi::Enumeration::MidiEndpointInformation const& information, winrt::Microsoft::Windows::Midi::Messages::Ump const& message);
        void SendUmp(winrt::guid const& deviceId, winrt::guid const& endpointId, winrt::Microsoft::Windows::Midi::Messages::Ump const& message);
        void SendUmp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0);
        void SendUmp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0, uint32_t word1);
        void SendUmp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0, uint32_t word1, uint32_t word2);
        void SendUmp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3);
        void SendUmpWithJRTimestamp(winrt::Microsoft::Windows::Midi::Enumeration::MidiEndpointInformation const& information, winrt::Microsoft::Windows::Midi::Messages::Ump const& message);
        void SendUmpWithJRTimestamp(winrt::guid const& deviceId, winrt::guid const& endpointId, winrt::Microsoft::Windows::Midi::Messages::Ump const& message);
        void SendUmpWithJRTimestamp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0);
        void SendUmpWithJRTimestamp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0, uint32_t word1);
        void SendUmpWithJRTimestamp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0, uint32_t word1, uint32_t word2);
        void SendUmpWithJRTimestamp(winrt::guid const& deviceId, winrt::guid const& endpointId, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3);
        bool UpdateName(hstring const& newName);
    };
}
namespace winrt::Microsoft::Windows::Midi::Session::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
