#pragma once
#include "MidiFunctionBlock.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock>
    {
        MidiFunctionBlock() = default;

        uint8_t Number();
        hstring Name();
        winrt::Microsoft::Devices::Midi2::MidiFunctionBlockDirection Direction();
        winrt::Microsoft::Devices::Midi2::MidiFunctionBlockUIHint UIHint();
        bool IsMidi10Connection();
        bool IsBandwidthRestricted();
        winrt::Microsoft::Devices::Midi2::MidiGroupList IncludedGroups();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestampList OriginalResponses();
        uint8_t MidiCIMessageVersionFormat();
        uint8_t MaxSysEx8Streams();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock, implementation::MidiFunctionBlock>
    {
    };
}
