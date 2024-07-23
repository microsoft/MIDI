// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"
#include "MidiVirtualDevice.h"
#include "MidiVirtualDevice.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    _Use_decl_annotations_
    void MidiVirtualDevice::InternalInitialize(
        _In_ winrt::hstring const& deviceEndpointDeviceId,
        _In_ virt::MidiVirtualDeviceCreationConfig const& config
    ) noexcept
    {
        m_deviceEndpointDeviceId = deviceEndpointDeviceId;

        m_declaredDeviceIdentity = config.DeclaredDeviceIdentity();
        m_declaredEndpointInfo = config.DeclaredEndpointInfo();

        m_name = L"Virtual: " + m_declaredEndpointInfo.Name;
        m_id = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

        m_associationId = config.AssociationId();
    }


    _Use_decl_annotations_
    bool MidiVirtualDevice::UpdateFunctionBlock(midi2::MidiFunctionBlock const& block) noexcept
    {
        // If blocks are static, return false. By spec, they are not allowed to be updated.
        if (m_declaredEndpointInfo.HasStaticFunctionBlocks)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Attempt to update static function blocks in a virtual device", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        // check to see if this is an existing block number. If not, fail. Devices are
        // not allowed to change the number of function blocks they have, per the MIDI 2 spec
        if (!m_functionBlocks.HasKey(block.Number()))
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Attempt to update a function block which wasn't declared in advance.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

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
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error sending function block info notification message.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        // check to see if the function block name changed. If so, send out a name notification

        auto existingBlock = m_functionBlocks.Lookup(block.Number());

        if (existingBlock.Name() != block.Name())
        {
            if (!SendFunctionBlockNameNotificationMessages(block))
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error sending function block name notification messages.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::UpdateEndpointName(winrt::hstring const& name) noexcept
    {
        auto cleanedName = internal::TrimmedHStringCopy(name);

        if (m_declaredEndpointInfo.Name != cleanedName)
        {
            if (!SendEndpointNameNotificationMessages(cleanedName))
            {
                //m_declaredEndpointInfo.Name = cleanedName;

                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error sending endpoint name notification messages.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return false;
            }
            else
            {
                m_declaredEndpointInfo.Name == cleanedName;
            }
        }

        return true;
    }


    _Use_decl_annotations_
    void MidiVirtualDevice::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept
    {
        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiVirtualDevice::OnEndpointConnectionOpened() noexcept
    {
        // nothing to do here
    }

    void MidiVirtualDevice::Cleanup() noexcept
    {
        // nothing to do here
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::SendFunctionBlockInfoNotificationMessage(midi2::MidiFunctionBlock const& fb) noexcept
    {
        auto functionBlockNotification = msgs::MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
            MidiClock::TimestampConstantSendImmediately(),
            true,
            fb.Number(),
            fb.UIHint(),
            fb.RepresentsMidi10Connection(),
            fb.Direction(),
            fb.FirstGroupIndex(),
            fb.GroupCount(),
            fb.MidiCIMessageVersionFormat(),
            fb.MaxSystemExclusive8Streams()
        );

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(functionBlockNotification)))
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"SendSingleMessagePacket failed.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::SendFunctionBlockNameNotificationMessages(midi2::MidiFunctionBlock const& fb) noexcept
    {
        if (fb.Name().empty()) return false;

        auto nameMessages = msgs::MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
            MidiClock::TimestampConstantSendImmediately(),
            fb.Number(),
            fb.Name()
        );

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"SendMultipleMessagesPacketList failed.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::SendEndpointNameNotificationMessages(winrt::hstring const& name) noexcept
    {
        if (name.empty()) return false;

        if (m_endpointConnection == nullptr) return false;

        auto nameMessages = msgs::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
            MidiClock::TimestampConstantSendImmediately(),
            name
        );

        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"SendMultipleMessagesPacketList failed.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        return true;
    }


    _Use_decl_annotations_
    void MidiVirtualDevice::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)  noexcept
    {
        bool handled = false;

        if (args.MessageType() == MidiMessageType::Stream128)
        {
            midi2::MidiMessage128 message{};

            if (args.FillMessage128(message))
            {
                // if a endpoint discovery request, handle it with the data we have
                if (internal::MessageIsEndpointDiscoveryRequest(message.Word0()))
                {
                    uint8_t filterFlags = internal::GetEndpointDiscoveryMessageFilterFlagsFromSecondWord(message.Word1());

                    if (internal::EndpointDiscoveryFilterRequestsEndpointInfoNotification(filterFlags))
                    {
                        // send endpoint info notification

                        auto notification = msgs::MidiStreamMessageBuilder::BuildEndpointInfoNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            MIDI_PREFERRED_UMP_VERSION_MAJOR,
                            MIDI_PREFERRED_UMP_VERSION_MINOR,
                            m_declaredEndpointInfo.HasStaticFunctionBlocks,
                            (uint8_t)m_functionBlocks.Size(),
                            m_declaredEndpointInfo.SupportsMidi20Protocol,
                            m_declaredEndpointInfo.SupportsMidi10Protocol,
                            m_declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps,
                            m_declaredEndpointInfo.SupportsSendingJitterReductionTimestamps
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(notification)))
                        {
                            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"SendSingleMessagePacket failed - sending endpoint info notification.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                            );
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsDeviceIdentityNotification(filterFlags))
                    {
                     //   internal::LogInfo(__FUNCTION__, L"Building/Sending Device Identity Notification");

                        // send device identity notification

                        // TODO: Need to validate no indexes out of bounds here

                        auto identityNotification = msgs::MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            m_declaredDeviceIdentity.SystemExclusiveIdByte1,   // byte 1
                            m_declaredDeviceIdentity.SystemExclusiveIdByte2,   // byte 2
                            m_declaredDeviceIdentity.SystemExclusiveIdByte3,   // byte 3
                            m_declaredDeviceIdentity.DeviceFamilyLsb,
                            m_declaredDeviceIdentity.DeviceFamilyMsb,
                            m_declaredDeviceIdentity.DeviceFamilyModelNumberLsb,
                            m_declaredDeviceIdentity.DeviceFamilyModelNumberMsb,
                            m_declaredDeviceIdentity.SoftwareRevisionLevelByte1,                 // byte 1
                            m_declaredDeviceIdentity.SoftwareRevisionLevelByte2,                 // byte 2
                            m_declaredDeviceIdentity.SoftwareRevisionLevelByte3,                 // byte 3
                            m_declaredDeviceIdentity.SoftwareRevisionLevelByte4                  // byte 4
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(identityNotification)))
                        {
                            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"SendSingleMessagePacket failed - sending device identity notification.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                            );

                        }

                    }

                    if (internal::EndpointDiscoveryFilterRequestsEndpointNameNotification(filterFlags))
                    {
                        // send endpoint name notification messages

                        if (!m_declaredEndpointInfo.Name.empty())
                        {
                            auto nameMessages = msgs::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_declaredEndpointInfo.Name
                            );

                            if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(nameMessages.GetView())))
                            {
                                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                                TraceLoggingWrite(
                                    Midi2SdkTelemetryProvider::Provider(),
                                    MIDI_SDK_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                    TraceLoggingWideString(L"SendMultipleMessagesPacketList failed - sending endpoint name notification list.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                                );
                            }
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsProductInstanceIdNotification(filterFlags))
                    {
                        // send product instance id notification messages

                        if (!m_declaredEndpointInfo.ProductInstanceId.empty())
                        {
                            auto instanceIdMessages = msgs::MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_declaredEndpointInfo.ProductInstanceId
                            );

                            if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendMultipleMessagesPacketList(instanceIdMessages.GetView())))
                            {
                                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                                TraceLoggingWrite(
                                    Midi2SdkTelemetryProvider::Provider(),
                                    MIDI_SDK_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                    TraceLoggingWideString(L"SendMultipleMessagesPacketList failed - sending product instance id messages.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                                );
                            }
                        }
                    }

                    if (internal::EndpointDiscoveryFilterRequestsStreamConfigurationNotification(filterFlags))
                    {
                        uint8_t protocol{ (uint8_t)midi2::MidiProtocol::Midi1 };

                        if (m_declaredEndpointInfo.SupportsMidi20Protocol)
                        {
                            protocol = (uint8_t)midi2::MidiProtocol::Midi2;
                        }

                        auto streamConfigurationNotification = msgs::MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(
                            MidiClock::TimestampConstantSendImmediately(),
                            protocol,
                            m_declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps,
                            m_declaredEndpointInfo.SupportsSendingJitterReductionTimestamps
                        );

                        if (midi2::MidiEndpointConnection::SendMessageFailed(m_endpointConnection.SendSingleMessagePacket(streamConfigurationNotification)))
                        {
                            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"SendSingleMessagePacket failed - sending device identity notification.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                            );
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
                        // send all function blocks

                        for (uint8_t i = 0; i < (uint8_t)m_functionBlocks.Size(); i++)
                        {
                            if (requestInfo) SendFunctionBlockInfoNotificationMessage(m_functionBlocks.Lookup(i));

                            if (requestName) SendFunctionBlockNameNotificationMessages(m_functionBlocks.Lookup(i));
                        }
                    }
                    else
                    {
                        // send single requested function block
                        if (m_functionBlocks.HasKey(fbNumber))
                        {
                            auto fb = m_functionBlocks.Lookup(fbNumber);

                            if (requestInfo) SendFunctionBlockInfoNotificationMessage(fb);

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
                    auto reqArgs = winrt::make_self<implementation::MidiStreamConfigRequestReceivedEventArgs>();

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
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error filling message type.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );
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




    //_Use_decl_annotations_
    //void MidiVirtualDevice::InternalSetDeviceDefinition(
    //    _In_ midi2::MidiVirtualDeviceDefinition definition)
    //{
    //    internal::LogInfo(__FUNCTION__, L"Enter");

    //    try
    //    {
    //        // populate all the views, properties, blocks, etc.

    //        m_areFunctionBlocksStatic = definition.AreFunctionBlocksStatic();

    //        for (uint8_t i = 0; i < definition.FunctionBlocks().Size(); i++)
    //        {
    //            auto fb = definition.FunctionBlocks().GetAt(i);

    //            // this is required, so we enforce it here
    //            fb.Number(i);

    //            // TODO: Set the fb as read-only
    //            

    //             
    //            // add the block
    //            m_functionBlocks.Insert(i, fb);
    //        }

    //        m_endpointName = definition.EndpointName();
    //        m_endpointProductInstanceId = definition.EndpointProductInstanceId();


    //        // TODO: All the other stuff we'll want to report on



    //        m_virtualEndpointDeviceDefinition = definition;

    //    }
    //    catch (...)
    //    {
    //        // todo log
    //        internal::LogGeneralError(__FUNCTION__, L"Exception attempting to set device definition");

    //    }

    //}

}
