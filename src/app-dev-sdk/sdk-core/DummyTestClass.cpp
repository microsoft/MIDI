#include "pch.h"
#include "DummyTestClass.h"
#include "DummyTestClass.g.cpp"

#include <random>
#include "midi_timestamp.h"
#include "ump_helpers.h"

using namespace Microsoft::Devices::Midi2::Internal::Shared;
using namespace Microsoft::Devices::Midi2::Internal;

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiUmpMessageType DummyTestClass::GetSomeStaticValue()
    {
        return MidiUmpMessageType::Midi2ChannelVoice64;
    }
    uint32_t DummyTestClass::SomeProperty()
    {
        return _someProperty;
    }
    void DummyTestClass::SomeProperty(uint32_t value)
    {
        _someProperty = value;
    }
    uint32_t DummyTestClass::SomeReadOnlyProperty()
    {
        return (uint32_t)_umps.Size();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp> DummyTestClass::Umps()
    {
        return _umps;
    }

    // the messages this creates are valid only in that the size matches the message type. Otherwise, it's garbage data
    void DummyTestClass::LoadDummyUmps(uint32_t count)
    {
        // gen a decent spread of random numbers so we can pick the right message types

        std::random_device rand;
        std::mt19937 engine(rand());
        std::uniform_int_distribution<> distribution(0x0, 0xF);


        for (int i = 0; i < count; i++)
        {
            uint8_t messageType = distribution(engine);

            switch (GetUmpLengthInMidiWordsFromMessageType(messageType))
            {
            case 1:
                {
                    MidiUmp32 umpStruct;
                    umpStruct.Timestamp = GetCurrentMidiTimestamp();
                    umpStruct.Word1 = 0x0ABCDEF9 | (messageType << 28);

                    _umps.Append(MidiUmpWithTimestamp::FromUmp32(umpStruct));
                }
                break;
            case 2:
                {
                    MidiUmp64 umpStruct;
                    umpStruct.Timestamp = GetCurrentMidiTimestamp();
                    umpStruct.Word1 = 0x09876541 | (messageType << 28);
                    umpStruct.Word2 = 0xAAAABBB1;

                    _umps.Append(MidiUmpWithTimestamp::FromUmp64(umpStruct));
                }
                break;
            case 3:
                {
                    MidiUmp96 umpStruct;
                    umpStruct.Timestamp = GetCurrentMidiTimestamp();
                    umpStruct.Word1 = 0x0ABCDEF3 | (messageType << 28);
                    umpStruct.Word2 = 0xCCCCDDD3;
                    umpStruct.Word3 = 0x99997773;

                    _umps.Append(MidiUmpWithTimestamp::FromUmp96(umpStruct));
                }
                break;
            case 4:
                {
                    MidiUmp128 umpStruct;
                    umpStruct.Timestamp = GetCurrentMidiTimestamp();
                    umpStruct.Word1 = 0x0FAFBFC1 | (messageType << 28);
                    umpStruct.Word2 = 0xEEEEFFF1;
                    umpStruct.Word3 = 0x66665551;
                    umpStruct.Word4 = 0x88889901;

                    _umps.Append(MidiUmpWithTimestamp::FromUmp128(umpStruct));
                }
                break;
            }

        }
    }

}
