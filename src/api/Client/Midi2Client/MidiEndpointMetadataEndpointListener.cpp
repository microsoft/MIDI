// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiEndpointMetadataEndpointListener.h"
#include "MidiEndpointMetadataEndpointListener.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    void MidiEndpointMetadataEndpointListener::Initialize()
    {
    }

    void MidiEndpointMetadataEndpointListener::OnEndpointConnectionOpened()
    {
    }

    void MidiEndpointMetadataEndpointListener::Cleanup()
    {
    }

    _Use_decl_annotations_
    void MidiEndpointMetadataEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)
    {
        auto endpointNameMessages = winrt::single_threaded_vector<midi2::MidiMessage128>();
        auto productInstanceIdMessages = winrt::single_threaded_vector<midi2::MidiMessage128>();

        if (args.MessageType() == MidiMessageType::Stream128)
        {
            uint32_t word0 = args.PeekFirstWord();

            // Handle Device Identity Notification Message

            if (internal::GetStatusFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION &&
                internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STANDARD_FORM0)
            {
                // TODO: Handle device identity notification


                skipFurtherListeners = true;
                skipMainMessageReceivedEvent = true;
            }

            else if (internal::GetStatusFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION)
            {
                // Handle function block name notifications. Need to pile these up until we get the ending form

                if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_START)
                {
                    // start a new name. Empty out the vector in case it had anything in it
                    productInstanceIdMessages.Clear();
                    productInstanceIdMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE)
                {
                    // continue the name
                    productInstanceIdMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                {
                    // ending form. Process the name

                    productInstanceIdMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    auto id = MidiStreamMessageBuilder::ParseProductInstanceIdNotificationMessages(productInstanceIdMessages);

                    //  save name information to the cache

                    MidiEndpointMetadataCache::AddOrUpdateData(
                        m_inputConnection.InputEndpointDeviceId(),
                        MIDI_CACHE_PROPERTY_ENDPOINT_PRODUCT_INSTANCE_ID,
                        id
                        );

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE)
                {
                    // complete in one packet

                    productInstanceIdMessages.Clear();
                    productInstanceIdMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    auto id = MidiStreamMessageBuilder::ParseProductInstanceIdNotificationMessages(productInstanceIdMessages);

                    // save name information to the cache

                    MidiEndpointMetadataCache::AddOrUpdateData(
                        m_inputConnection.InputEndpointDeviceId(),
                        MIDI_CACHE_PROPERTY_ENDPOINT_PRODUCT_INSTANCE_ID,
                        id
                        );

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }
            }

            else if (internal::GetStatusFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION)
            {
                // Handle function block name notifications. Need to pile these up until we get the ending form

                if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_START)
                {
                    // start a new name. Empty out the vector in case it had anything in it
                    endpointNameMessages.Clear();
                    endpointNameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;

                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE)
                {
                    // continue the name
                    endpointNameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                {
                    // ending form. Process the name

                    endpointNameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    auto endpointName = MidiStreamMessageBuilder::ParseProductInstanceIdNotificationMessages(endpointNameMessages);

                    // save name to the cache

                    MidiEndpointMetadataCache::AddOrUpdateData(
                        m_inputConnection.InputEndpointDeviceId(),
                        MIDI_CACHE_PROPERTY_ENDPOINT_NAME,
                        endpointName
                        );

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }

                else if (internal::GetFormFromStreamMessageFirstWord(word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE)
                {
                    // complete in one packet

                    endpointNameMessages.Clear();
                    endpointNameMessages.Append(args.GetMessagePacket().as<midi2::MidiMessage128>());

                    auto endpointName = MidiStreamMessageBuilder::ParseProductInstanceIdNotificationMessages(endpointNameMessages);

                    // save name to the cache

                    MidiEndpointMetadataCache::AddOrUpdateData(
                        m_inputConnection.InputEndpointDeviceId(),
                        MIDI_CACHE_PROPERTY_ENDPOINT_NAME,
                        endpointName
                        );

                    skipFurtherListeners = true;
                    skipMainMessageReceivedEvent = true;
                }
            }



        }
        
    }

}
