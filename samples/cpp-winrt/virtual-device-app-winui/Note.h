#pragma once
#include "Note.g.h"

namespace winrt::virtual_device_app_winui::implementation
{
    struct Note : NoteT<Note>
    {
        Note() = default;

        uint8_t NoteNumber() { return m_noteNumber; }
        void NoteNumber(_In_ uint8_t value) { m_noteNumber = value; }

        uint8_t GroupIndex() { return m_groupIndex; }
        void GroupIndex(_In_ uint8_t value) { m_groupIndex = value; }

        uint8_t ChannelIndex() { return m_channelIndex; }
        void ChannelIndex(_In_ uint8_t value) { m_channelIndex = value; }

        winrt::Microsoft::Windows::Devices::Midi2::MidiEndpointConnection Connection() { return m_connection; }
        void Connection(_In_ winrt::Microsoft::Windows::Devices::Midi2::MidiEndpointConnection const& value) { m_connection = value; }

        void NoteOn();
        void NoteOff();

    private:
        uint8_t m_noteNumber{};
        uint8_t m_groupIndex{};
        uint8_t m_channelIndex{};
        
        winrt::Microsoft::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };

    };
}
namespace winrt::virtual_device_app_winui::factory_implementation
{
    struct Note : NoteT<Note, implementation::Note>
    {
    };
}