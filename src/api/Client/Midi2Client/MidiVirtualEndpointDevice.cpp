// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiVirtualEndpointDevice.h"
#include "MidiVirtualEndpointDevice.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::UpdateFunctionBlock(midi2::MidiFunctionBlock const& block) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        UNREFERENCED_PARAMETER(block);

        // TODO
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::UpdateEndpointName(winrt::hstring const& name) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        UNREFERENCED_PARAMETER(name);

        // TODO
    }


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiVirtualEndpointDevice::OnEndpointConnectionOpened() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // nothing to do here
    }

    void MidiVirtualEndpointDevice::Cleanup() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // nothing to do here
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::SendFunctionBlockInfoNotificationMessage(midi2::MidiFunctionBlock const& fb) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        auto functionBlockNotification = midi2::MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
            MidiClock::TimestampConstantSendImmediately(),
            true,
            fb.Number(),
            fb.UIHint(),
            fb.Midi10Connection(),
            fb.Direction(),
            fb.FirstGroupIndex(),
            fb.GroupCount(),
            fb.MidiCIMessageVersionFormat(),
            fb.MaxSystemExclusive8Streams()
        );

        // TODO: Log if failed
        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(functionBlockNotification)))
        {
            internal::LogGeneralError(__FUNCTION__, L"SendMessagePacket failed");
        }
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::SendFunctionBlockNameNotificationMessages(midi2::MidiFunctionBlock const& fb) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (fb.Name() == L"") return;

        auto nameMessages = midi2::MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
            MidiClock::TimestampConstantSendImmediately(),
            fb.Number(),
            fb.Name()
        );

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
        {
            internal::LogGeneralError(__FUNCTION__, L"SendMessagePacketList failed");
        }
    }


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)  noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        bool handled = false;

        if (args.MessageType() == MidiMessageType::Stream128)
        {
            MidiMessage128 message{};
            if (args.FillMessage128(message))
            {
                // if a endpoint discovery request, handle it with the data we have
                if (internal::MessageIsEndpointDiscoveryRequest(message.Word0()))
                {
                    uint8_t filterFlags = internal::GetEndpointDiscoveryMessageFilterFlagsFromSecondWord(message.Word1());

                    if (internal::EndpointDiscoveryFilterRequestsEndpointInfoNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Sending Endpoint Info Notification");

                        // send endpoint info notification

                        auto notification = midi2::MidiStreamMessageBuilder::BuildEndpointInfoNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            MIDI_PREFERRED_UMP_VERSION_MAJOR,
                            MIDI_PREFERRED_UMP_VERSION_MINOR,
                            m_areFunctionBlocksStatic,
                            (uint8_t)m_functionBlocks.Size(),
                            true,   // TODO: Pull from properties supports midi 2.0
                            true,   // TODO: pull from properties supports midi 1.0
                            false,  // todo: pull from default JR timestamp handling
                            false   // todo: pull from jr timestamp handling
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(notification)))
                        {
                            internal::LogGeneralError(__FUNCTION__, L"SendMessagePacket failed - sending endpoint info notification");
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsDeviceIdentityNotification(filterFlags))
                    {
                        // send device identity notification
                    }

                    if (internal::EndpointDiscoveryFilterRequestsEndpointNameNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Sending Endpoint Name Notification");

                        // send endpoint name notification messages

                        if (!m_endpointName.empty())
                        {
                            auto nameMessages = midi2::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_endpointName
                            );

                            if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
                            {
                                internal::LogGeneralError(__FUNCTION__, L"SendMessagePacketList failed - sending endpoint name notification list");
                            }
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsProductInstanceIdNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Sending Endpoint Product Instance Id Notification");

                        // send product instance id notification messages

                        if (!m_endpointProductInstanceId.empty())
                        {
                            auto instanceIdMessages = midi2::MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_endpointProductInstanceId
                            );

                            if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(instanceIdMessages.GetView())))
                            {
                                internal::LogGeneralError(__FUNCTION__, L"SendMessagePacketList failed - sending product instance id messages");
                            }
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsStreamConfigurationNotification(filterFlags))
                    {
                        // send stream configuration message
                    }
                }
                else if (internal::MessageIsFunctionBlockDiscoveryRequest(message.Word0()))
                {
                    uint8_t filterFlags = internal::GetFunctionBlockDiscoveryMessageFilterFlagsFromFirstWord(message.Word0());

                    bool requestInfo = internal::FunctionBlockDiscoveryFilterRequestsInfoNotification(filterFlags);
                    bool requestName = internal::FunctionBlockDiscoveryFilterRequestsNameNotification(filterFlags);

                    uint8_t fbNumber = internal::GetFunctionBlockNumberFromFunctionBlockDiscoveryRequestFirstWord(message.Word0());

                    if (fbNumber == MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS)
                    {
                        internal::LogInfo(__FUNCTION__, L"Sending All function blocks, as requested");

                        // send all function blocks

                        for (uint8_t i = 0; i < (uint8_t)m_functionBlocks.Size(); i++)
                        {
                            internal::LogInfo(__FUNCTION__, L"Sending function block info message");
                            if (requestInfo) SendFunctionBlockInfoNotificationMessage(m_functionBlocks.Lookup(i));

                            internal::LogInfo(__FUNCTION__, L"Sending function block name messages");
                            if (requestName) SendFunctionBlockNameNotificationMessages(m_functionBlocks.Lookup(i));
                        }
                    }
                    else
                    {
                        // send single requested function block
                        internal::LogInfo(__FUNCTION__, L"Sending only a single function block, as requested");

                        if (m_functionBlocks.HasKey(fbNumber))
                        {
                            auto fb = m_functionBlocks.Lookup(fbNumber);

                            internal::LogInfo(__FUNCTION__, L"Sending function block info message");
                            if (requestInfo) SendFunctionBlockInfoNotificationMessage(fb);

                            internal::LogInfo(__FUNCTION__, L"Sending function block name messages");
                            if (requestName) SendFunctionBlockNameNotificationMessages(fb);
                        }
                        else
                        {
                            // invalid fb number request
                            handled = false;
                        }
                    }
                }

                else
                {
                    // something else
                }



            }
            else
            {
                // something went wrong filling this message type
                internal::LogGeneralError(__FUNCTION__, L"Error filling message type");
            }
            

        }
        else
        {
            // not a stream message. Ignore


        }


        if (handled && SuppressHandledMessages())
        {
            skipFurtherListeners = true;
            skipMainMessageReceivedEvent = true;
        }
        else
        {
            skipFurtherListeners = false;
            skipMainMessageReceivedEvent = false;
        }

    }




    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::InternalSetDeviceDefinition(
        _In_ midi2::MidiVirtualEndpointDeviceDefinition definition)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        try
        {
            // populate all the views, properties, blocks, etc.

            m_areFunctionBlocksStatic = definition.AreFunctionBlocksStatic();

            for (uint8_t i = 0; i < definition.FunctionBlocks().Size(); i++)
            {
                auto fb = definition.FunctionBlocks().GetAt(i);

                // this is required, so we enforce it here
                fb.Number(i);

                // TODO: Set the fb as read-only
                

                 
                // add the block
                m_functionBlocks.Insert(i, fb);
            }

            m_endpointName = definition.EndpointName();
            m_endpointProductInstanceId = definition.EndpointProductInstanceId();


            // TODO: All the other stuff we'll want to report on



            m_virtualEndpointDeviceDefinition = definition;

        }
        catch (...)
        {
            // todo log
            internal::LogGeneralError(__FUNCTION__, L"Exception attempting to set device definition");

        }

    }

}
