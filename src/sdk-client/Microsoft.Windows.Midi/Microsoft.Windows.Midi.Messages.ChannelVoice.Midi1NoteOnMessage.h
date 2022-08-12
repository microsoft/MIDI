#pragma once
#include "Microsoft.Windows.Midi.Messages.ChannelVoice.Midi1NoteOnMessage.g.h"
#include "Microsoft.Windows.Midi.Messages.ChannelVoice.Midi1ChannelVoiceMessage.h"

// WARNING: This file is automatically generated by a tool. Do not directly
// add this file to your project, as any changes you make will be lost.
// This file is a stub you can use as a starting point for your implementation.
//
// To add a copy of this file to your project:
//   1. Copy this file from its original location to the location where you store 
//      your other source files (e.g. the project root). 
//   2. Add the copied file to your project. In Visual Studio, you can use 
//      Project -> Add Existing Item.
//   3. Delete this comment and the 'static_assert' (below) from the copied file.
//      Do not modify the original file.
//
// To update an existing file in your project:
//   1. Copy the relevant changes from this file and merge them into the copy 
//      you made previously.
//    
// This assertion helps prevent accidental modification of generated files.
//static_assert(false, "This file is generated by a tool and will be overwritten. Open this error and view the comment for assistance.");

namespace winrt::Microsoft::Windows::Midi::Messages::ChannelVoice::implementation
{
    struct Midi1NoteOnMessage : Midi1NoteOnMessageT<Midi1NoteOnMessage, Microsoft::Windows::Midi::Messages::ChannelVoice::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1NoteOnMessage() = default;

        static winrt::Microsoft::Windows::Midi::Messages::ChannelVoice::Midi1NoteOnMessage FromUmp(winrt::Microsoft::Windows::Midi::Messages::Ump32 const& ump);
        static winrt::Microsoft::Windows::Midi::Messages::ChannelVoice::Midi1NoteOnMessage FromWord(uint32_t word0);
        static winrt::Microsoft::Windows::Midi::Messages::ChannelVoice::Midi1NoteOnMessage FromMidi1Bytes(uint8_t group, uint8_t midi1Byte0, uint8_t midi1Byte1, uint8_t midi1Byte2);
        uint8_t NoteNumber();
        void NoteNumber(uint8_t value);
        uint8_t Velocity();
        void Velocity(uint8_t value);
    };
}
namespace winrt::Microsoft::Windows::Midi::Messages::ChannelVoice::factory_implementation
{
    struct Midi1NoteOnMessage : Midi1NoteOnMessageT<Midi1NoteOnMessage, implementation::Midi1NoteOnMessage>
    {
    };
}
