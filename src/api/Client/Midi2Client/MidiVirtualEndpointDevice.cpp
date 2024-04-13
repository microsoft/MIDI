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
    bool MidiVirtualEndpointDevice::UpdateFunctionBlock(midi2::MidiFunctionBlock const& block) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // If blocks are static, return false. By spec, they are not allowed to be updated.
        if (m_virtualEndpointDeviceDefinition.AreFunctionBlocksStatic())
        {
            internal::LogGeneralError(__FUNCTION__, L"Attempt to update static function blocks in a virtual device.");
            return false;
        }

        // check to see if this is an existing block number. If not, fail. Devices are
        // not allowed to change the number of function blocks they have, per the MIDI 2 spec
        if (!m_functionBlocks.HasKey(block.Number()))
        {
            internal::LogGeneralError(__FUNCTION__, L"Attempt to update a function block which wasn't declared up-front.");
            return false;
        }

        // Update the block and send the notification messages
        if (SendFunctionBlockInfoNotificationMessage(block))
        {
            // this ignores that the name may not be set. It's an oversight in the block object design.
            m_functionBlocks.Insert(block.Number(), block);
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Error sending function block info notification message.");
            return false;
        }

        // check to see if the function block name changed. If so, send out a name notification

        auto existingBlock = m_functionBlocks.Lookup(block.Number());

        if (existingBlock.Name() != block.Name())
        {
            if (!SendFunctionBlockNameNotificationMessages(block))
            {
                internal::LogGeneralError(__FUNCTION__, L"Error sending function block name notification messages.");
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiVirtualEndpointDevice::UpdateEndpointName(winrt::hstring const& name) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        auto cleanedName = internal::TrimmedHStringCopy(name);

        // TODO: update the name and send the notification message

        if (m_endpointName != cleanedName)
        {
            if (!SendEndpointNameNotificationMessages(cleanedName))
            {
                internal::LogGeneralError(__FUNCTION__, L"Error sending endpoint name notification messages.");
                return false;
            }
        }

        return true;
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
    bool MidiVirtualEndpointDevice::SendFunctionBlockInfoNotificationMessage(midi2::MidiFunctionBlock const& fb) noexcept
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

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(functionBlockNotification)))
        {
            internal::LogGeneralError(__FUNCTION__, L"SendMessagePacket failed");
            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiVirtualEndpointDevice::SendFunctionBlockNameNotificationMessages(midi2::MidiFunctionBlock const& fb) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (fb.Name().empty()) return false;

        auto nameMessages = midi2::MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
            MidiClock::TimestampConstantSendImmediately(),
            fb.Number(),
            fb.Name()
        );

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
        {
            internal::LogGeneralError(__FUNCTION__, L"SendMultipleMessagesPacketList failed");
            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiVirtualEndpointDevice::SendEndpointNameNotificationMessages(winrt::hstring const& name) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        if (name.empty()) return false;

        auto nameMessages = midi2::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
            MidiClock::TimestampConstantSendImmediately(),
            name
        );

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
        {
            internal::LogGeneralError(__FUNCTION__, L"SendMultipleMessagesPacketList failed");
            return false;
        }

        return true;
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
                        internal::LogInfo(__FUNCTION__, L"Building/Sending Endpoint Info Notification");

                        // send endpoint info notification

                        auto notification = midi2::MidiStreamMessageBuilder::BuildEndpointInfoNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            MIDI_PREFERRED_UMP_VERSION_MAJOR,
                            MIDI_PREFERRED_UMP_VERSION_MINOR,
                            m_areFunctionBlocksStatic,
                            (uint8_t)m_functionBlocks.Size(),
                            m_virtualEndpointDeviceDefinition.SupportsMidi2ProtocolMessages(),
                            m_virtualEndpointDeviceDefinition.SupportsMidi1ProtocolMessages(),
                            m_virtualEndpointDeviceDefinition.SupportsReceivingJRTimestamps(),
                            m_virtualEndpointDeviceDefinition.SupportsSendingJRTimestamps()
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(notification)))
                        {
                            internal::LogGeneralError(__FUNCTION__, L"SendSingleMessagePacket failed - sending endpoint info notification");
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsDeviceIdentityNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Building/Sending Device Identity Notification");

                        // send device identity notification

                        // TODO: Need to validate no indexes out of bounds here

                        auto identityNotification = midi2::MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            m_virtualEndpointDeviceDefinition.DeviceManufacturerSystemExclusiveId().GetAt(0),   // byte 1
                            m_virtualEndpointDeviceDefinition.DeviceManufacturerSystemExclusiveId().GetAt(1),   // byte 2
                            m_virtualEndpointDeviceDefinition.DeviceManufacturerSystemExclusiveId().GetAt(2),   // byte 3
                            m_virtualEndpointDeviceDefinition.DeviceFamilyLsb(),
                            m_virtualEndpointDeviceDefinition.DeviceFamilyMsb(),
                            m_virtualEndpointDeviceDefinition.DeviceFamilyModelLsb(),
                            m_virtualEndpointDeviceDefinition.DeviceFamilyModelMsb(),
                            m_virtualEndpointDeviceDefinition.SoftwareRevisionLevel().GetAt(0),                 // byte 1
                            m_virtualEndpointDeviceDefinition.SoftwareRevisionLevel().GetAt(1),                 // byte 2
                            m_virtualEndpointDeviceDefinition.SoftwareRevisionLevel().GetAt(2),                 // byte 3
                            m_virtualEndpointDeviceDefinition.SoftwareRevisionLevel().GetAt(3)                  // byte 4
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(identityNotification)))
                        {
                            internal::LogGeneralError(__FUNCTION__, L"SendSingleMessagePacket failed - sending device identity notification");
                        }

                    }

                    if (internal::EndpointDiscoveryFilterRequestsEndpointNameNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Building/Sending Endpoint Name Notification");

                        // send endpoint name notification messages

                        if (!m_endpointName.empty())
                        {
                            auto nameMessages = midi2::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_endpointName
                            );

                            if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
                            {
                                internal::LogGeneralError(__FUNCTION__, L"SendMultipleMessagesPacketList failed - sending endpoint name notification list");
                            }
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsProductInstanceIdNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Building/Sending Endpoint Product Instance Id Notification");

                        // send product instance id notification messages

                        if (!m_endpointProductInstanceId.empty())
                        {
                            auto instanceIdMessages = midi2::MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_endpointProductInstanceId
                            );

                            if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(instanceIdMessages.GetView())))
                            {
                                internal::LogGeneralError(__FUNCTION__, L"SendMultipleMessagesPacketList failed - sending product instance id messages");
                            }
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsStreamConfigurationNotification(filterFlags))
                    {
                        internal::LogInfo(__FUNCTION__, L"Building/Sending Stream Configuration Notification");

                        uint8_t protocol{ (uint8_t)midi2::MidiProtocol::Midi1 };

                        if (m_virtualEndpointDeviceDefinition.SupportsMidi2ProtocolMessages())
                        {
                            protocol = (uint8_t)midi2::MidiProtocol::Midi2;
                        }

                        auto streamConfigurationNotification = midi2::MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            protocol,
                            m_virtualEndpointDeviceDefinition.SupportsReceivingJRTimestamps(),
                            m_virtualEndpointDeviceDefinition.SupportsSendingJRTimestamps()
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(streamConfigurationNotification)))
                        {
                            internal::LogGeneralError(__FUNCTION__, L"SendSingleMessagePacket failed - sending device identity notification");
                        }

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

                else if (internal::MessageIsStreamConfigurationRequest(message.Word0()))
                {
                    // raise stream configuration request message
                    auto reqArgs = winrt::make_self<implementation::MidiStreamConfigurationRequestReceivedEventArgs>();

                    auto protocol = (midi2::MidiProtocol)MIDIWORDBYTE3(message.Word0());
                    bool rxjr = (bool)((message.Word0() & 0x00000002) == 0x00000002);
                    bool txjr = (bool)((message.Word0() & 0x00000001) == 0x00000001);

                    reqArgs->InternalInitialize(message.Timestamp(), protocol, rxjr, txjr);

                    if (m_streamConfigurationRequestReceivedEvent)
                    {
                        m_streamConfigurationRequestReceivedEvent(*this, *reqArgs);
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
