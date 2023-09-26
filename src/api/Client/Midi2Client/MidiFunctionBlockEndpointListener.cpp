// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiFunctionBlockEndpointListener.h"
#include "MidiFunctionBlockEndpointListener.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiFunctionBlockEndpointListener::Initialize()
    {
    }

    _Use_decl_annotations_
    void MidiFunctionBlockEndpointListener::OnEndpointConnectionOpened()
    {
    }

    _Use_decl_annotations_
    void MidiFunctionBlockEndpointListener::Cleanup()
    {
    }

    _Use_decl_annotations_
    void MidiFunctionBlockEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = false;
        skipMainMessageReceivedEvent = false;

        auto nameMessages = winrt::single_threaded_vector<midi2::MidiMessage128>();


        if (args.MessageType() == MidiMessageType::Stream128)
        {
            uint32_t word0 = args.PeekFirstWord();

            if (internal::GetStatusFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION && 
                internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STANDARD_FORM0)
            {
                // TODO: Handle function block info notification

            }
            else if (internal::GetStatusFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION)
            {
                // Handle function block name notifications. Need to pile these up until we get the ending form

                if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_START)
                {
                    // start a new name. Empty out the vector in case it had anything in it
                    nameMessages.Clear();
                    nameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE)
                {
                    // continue the name
                    nameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());
                }

                else if(internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                {
                    // ending form. Process the name

                    nameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    auto name = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(nameMessages);

                    // todo: save name information to the cache
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE)
                {
                    // complete in one packet

                    nameMessages.Clear();
                    nameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    auto name = MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(nameMessages);

                    // todo: save name information to the cache
                }
            }
        }
    }
}
