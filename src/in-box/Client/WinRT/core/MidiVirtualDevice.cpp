// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"
#include "MidiVirtualDevice.h"
#include "Transports.Virtual.MidiVirtualDevice.g.cpp"

#include "MidiStreamMessageBuilder.h"
#include "MidiStreamConfigRequestReceivedEventArgs.h"
#include "MidiVirtualDeviceClientEndpointInUseChangedEventArgs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Virtual::implementation
{
    _Use_decl_annotations_
    void MidiVirtualDevice::InternalInitialize(
        _In_ winrt::hstring const& deviceEndpointDeviceId,
        _In_ virt::MidiVirtualDeviceCreationConfig const& config
    ) noexcept
    {
        try
        {
            m_deviceEndpointDeviceId = deviceEndpointDeviceId;

            m_declaredDeviceIdentity = config.DeclaredDeviceIdentity();
            m_declaredEndpointInfo = config.DeclaredEndpointInfo();

            m_name = internal::ResourceGetHString(IDS_DISPLAY_VIRTUAL_DEVICE_NAME_PREFIX) + m_declaredEndpointInfo.Name();
            m_id = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

            m_associationId = config.AssociationId();

            for (auto const& fb : config.FunctionBlocks())
            {
                m_functionBlocks.Insert(fb.Number(), fb);
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error initializing virtual device.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception initializing virtual device.");
        }
    }


    _Use_decl_annotations_
    bool MidiVirtualDevice::UpdateFunctionBlock(midi2enum::MidiFunctionBlock const& updatedBlock) noexcept
    {
        try
        {
            // If blocks are static, return false. By spec, they are not allowed to be updated.
            if (m_declaredEndpointInfo.HasStaticFunctionBlocks())
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
            if (!m_functionBlocks.HasKey(updatedBlock.Number()))
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

            auto oldBlock = m_functionBlocks.Lookup(updatedBlock.Number());

            if (oldBlock == nullptr)
            {
                return false;
            }

            // Update the block info and send the notification messages
            if (SendFunctionBlockInfoNotificationMessage(updatedBlock))
            {
                if (oldBlock.Name() != updatedBlock.Name())
                {
                    // only send a name notification if the name has changed

                    if (!SendFunctionBlockNameNotificationMessages(updatedBlock))
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

                m_functionBlocks.Insert(updatedBlock.Number(), updatedBlock);
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


            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error updating function block.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception updating function block.");
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::UpdateEndpointName(winrt::hstring const& name) noexcept
    {
        try
        {
            auto cleanedName = internal::TrimmedHStringCopy(name);

            if (m_declaredEndpointInfo.Name() != cleanedName)
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
                    m_declaredEndpointInfo.Name(cleanedName);
                }
            }

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error updating endpoint name.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception updating endpoint name.");
            return false;
        }
    }


    _Use_decl_annotations_
    void MidiVirtualDevice::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept
    {
        try
        {
            m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error initializing virtual device connection.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception initializing virtual device connection.");
        }
    }

    void MidiVirtualDevice::OnEndpointConnectionOpened() noexcept
    {
        // The client-visible endpoint only exists once this connection is open, so this is the
        // earliest point the transport can report anything about it.
        StartClientEndpointInUseWatcher();
    }

    void MidiVirtualDevice::Cleanup() noexcept
    {
        StopClientEndpointInUseWatcher();

        m_streamConfigurationRequestReceivedEvent.clear();
        m_clientEndpointInUseChangedEvent.clear();
    }


    void MidiVirtualDevice::StartClientEndpointInUseWatcher() noexcept
    {
        try
        {
            if (m_deviceEndpointDeviceId.empty()) return;

            // A transport which does not report this leaves the watcher unstarted rather than
            // raising events which would always read false.
            if (!svc::MidiServiceTransportPluginConfigManager::QueryCapability(
                    virt::MidiVirtualDeviceManager::TransportId(),
                    MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CLIENT_ENDPOINT_IN_USE_NOTIFICATION))
            {
                return;
            }

            auto properties = winrt::single_threaded_vector<winrt::hstring>();
            properties.Append(STRING_PKEY_MIDI_VirtualMidiClientEndpointInUse);

            // scoped to this one endpoint, so no other endpoint's churn reaches us
            auto selector = L"System.Devices.DeviceInstanceId:=\"" + m_deviceEndpointDeviceId + L"\"";

            m_clientEndpointInUseWatcher = enumeration::DeviceInformation::CreateWatcher(
                winrt::hstring{ L"System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True" },
                properties,
                enumeration::DeviceInformationKind::DeviceInterface);

            if (m_clientEndpointInUseWatcher == nullptr) return;

            // A watcher does not deliver Updated unless Added and Removed are also subscribed.
            m_watcherAddedToken = m_clientEndpointInUseWatcher.Added(
                [this](enumeration::DeviceWatcher const&, enumeration::DeviceInformation const& info)
                {
                    if (m_watcherShuttingDown) return;

                    if (_wcsicmp(info.Id().c_str(), m_deviceEndpointDeviceId.c_str()) != 0) return;

                    if (auto value = info.Properties().TryLookup(STRING_PKEY_MIDI_VirtualMidiClientEndpointInUse))
                    {
                        m_isClientEndpointInUse = winrt::unbox_value_or<bool>(value, false);
                    }
                });

            m_watcherUpdatedToken = m_clientEndpointInUseWatcher.Updated(
                [this](enumeration::DeviceWatcher const&, enumeration::DeviceInformationUpdate const& update)
                {
                    HandleClientEndpointInUseProperty(update);
                });

            m_watcherRemovedToken = m_clientEndpointInUseWatcher.Removed(
                [this](enumeration::DeviceWatcher const&, enumeration::DeviceInformationUpdate const& update)
                {
                    if (m_watcherShuttingDown) return;
                    if (update == nullptr) return;
                    if (_wcsicmp(update.Id().c_str(), m_deviceEndpointDeviceId.c_str()) != 0) return;

                    // No further update can arrive once the endpoint is gone, so clear the cached
                    // value rather than leave it reading true forever. Deliberately silent: the
                    // device is being torn down and raising into a disposing object helps nobody.
                    m_isClientEndpointInUse = false;
                });

            m_clientEndpointInUseWatcher.Start();
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error starting client endpoint in-use watcher.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception starting client endpoint in-use watcher.");
        }
    }


    void MidiVirtualDevice::StopClientEndpointInUseWatcher() noexcept
    {
        // Set before revoking, so a handler already running sees it and does nothing further.
        m_watcherShuttingDown = true;

        try
        {
            if (m_clientEndpointInUseWatcher == nullptr) return;

            if (m_watcherAddedToken) m_clientEndpointInUseWatcher.Added(m_watcherAddedToken);
            if (m_watcherUpdatedToken) m_clientEndpointInUseWatcher.Updated(m_watcherUpdatedToken);
            if (m_watcherRemovedToken) m_clientEndpointInUseWatcher.Removed(m_watcherRemovedToken);

            auto status = m_clientEndpointInUseWatcher.Status();

            if (status == enumeration::DeviceWatcherStatus::Started ||
                status == enumeration::DeviceWatcherStatus::EnumerationCompleted)
            {
                m_clientEndpointInUseWatcher.Stop();
            }
        }
        catch (...)
        {
            // the endpoint may already be gone, which is a normal way for this to end
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"Exception stopping client endpoint in-use watcher.");
        }

        m_clientEndpointInUseWatcher = nullptr;
        m_isClientEndpointInUse = false;
    }


    _Use_decl_annotations_
    void MidiVirtualDevice::HandleClientEndpointInUseProperty(enumeration::DeviceInformationUpdate const& update) noexcept
    {
        try
        {
            if (m_watcherShuttingDown) return;
            if (update == nullptr) return;

            if (_wcsicmp(update.Id().c_str(), m_deviceEndpointDeviceId.c_str()) != 0) return;

            auto value = update.Properties().TryLookup(STRING_PKEY_MIDI_VirtualMidiClientEndpointInUse);

            if (value == nullptr) return;

            auto inUse = winrt::unbox_value_or<bool>(value, false);

            // Only report real transitions. A property write which does not change the value
            // still produces an update.
            if (m_isClientEndpointInUse.exchange(inUse) == inUse) return;

            auto args = winrt::make_self<implementation::MidiVirtualDeviceClientEndpointInUseChangedEventArgs>();
            args->InternalInitialize(inUse);

            m_clientEndpointInUseChangedEvent(*this, *args);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error handling client endpoint in-use update.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception handling client endpoint in-use update.");
        }
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::SendFunctionBlockInfoNotificationMessage(midi2enum::MidiFunctionBlock const& fb) noexcept
    {
        try
        {
            if (m_endpointConnection == nullptr || !m_endpointConnection.IsOpen())
            {
                return false;
            }

            auto functionBlockNotification = msgs::MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
                MidiClock::TimestampConstantSendImmediately(),
                fb.IsActive(),
                fb.Number(),
                fb.UIHint(),
                fb.RepresentsMidi10Connection(),
                fb.Direction(),
                fb.FirstGroup().Index(),
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error sending function block info notification message.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception sending function block info notification message.");
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::SendFunctionBlockNameNotificationMessages(midi2enum::MidiFunctionBlock const& fb) noexcept
    {
        try
        {
            if (m_endpointConnection == nullptr || !m_endpointConnection.IsOpen())
            {
                return false;
            }

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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error sending function block name notification messages.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception sending function block name notification messages.");
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiVirtualDevice::SendEndpointNameNotificationMessages(winrt::hstring const& name) noexcept
    {
        try
        {
            if (m_endpointConnection == nullptr || !m_endpointConnection.IsOpen())
            {
                return false;
            }

            if (name.empty()) return false;

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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error sending endpoint name notification messages.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception sending endpoint name notification messages.");
            return false;
        }
    }


    _Use_decl_annotations_
    void MidiVirtualDevice::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)  noexcept
    {
        try
        {
            bool handled = false;

            //OutputDebugString(L"MIDI SDK: MidiVirtualDevice::ProcessIncomingMessage\n");

            if (args.MessageType() == MidiMessageType::Stream128)
            {
                midi2::MidiMessage128 message{};

                if (args.FillMessage128(message))
                {
                    // if an endpoint discovery request, handle it with the data we have
                    if (internal::MessageIsEndpointDiscoveryRequest(message.Word0()))
                    {
                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Endpoint discovery request received.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                        );

                        //OutputDebugString(L"MIDI SDK: Incoming message is endpoint discovery request\n");

                        uint8_t filterFlags = internal::GetEndpointDiscoveryMessageFilterFlagsFromSecondWord(message.Word1());

                        if (internal::EndpointDiscoveryFilterRequestsEndpointInfoNotification(filterFlags))
                        {
                            // because we reuse the declaredendpointinfo type, it has a redundant field
                            // which can cause problems during discovery (mostly a long timeout). So
                            // just update the field here.
                            m_declaredEndpointInfo.DeclaredFunctionBlockCount(static_cast<uint8_t>(m_functionBlocks.Size()));

                            // send endpoint info notification

                            auto notification = msgs::MidiStreamMessageBuilder::BuildEndpointInfoNotificationMessage(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_declaredEndpointInfo.SpecificationVersionMajor(),
                                m_declaredEndpointInfo.SpecificationVersionMinor(),
                                m_declaredEndpointInfo.HasStaticFunctionBlocks(),
                                m_declaredEndpointInfo.DeclaredFunctionBlockCount(),
                                m_declaredEndpointInfo.SupportsMidi20Protocol(),
                                m_declaredEndpointInfo.SupportsMidi10Protocol(),
                                m_declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps(),
                                m_declaredEndpointInfo.SupportsSendingJitterReductionTimestamps()
                            );

                            //OutputDebugString(L"MIDI SDK: Responding with Endpoint Info Notification\n");

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
                            // send device identity notification

                            auto identityNotification = msgs::MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
                                MidiClock::TimestampConstantSendImmediately(),
                                m_declaredDeviceIdentity.SystemExclusiveId()[0],   // byte 1
                                m_declaredDeviceIdentity.SystemExclusiveId()[1],   // byte 2
                                m_declaredDeviceIdentity.SystemExclusiveId()[2],   // byte 3
                                m_declaredDeviceIdentity.DeviceFamilyLsb(),
                                m_declaredDeviceIdentity.DeviceFamilyMsb(),
                                m_declaredDeviceIdentity.DeviceFamilyModelNumberLsb(),
                                m_declaredDeviceIdentity.DeviceFamilyModelNumberMsb(),
                                m_declaredDeviceIdentity.SoftwareRevisionLevel()[0],                 // byte 1
                                m_declaredDeviceIdentity.SoftwareRevisionLevel()[1],                 // byte 2
                                m_declaredDeviceIdentity.SoftwareRevisionLevel()[2],                 // byte 3
                                m_declaredDeviceIdentity.SoftwareRevisionLevel()[3]                  // byte 4
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

                            if (!m_declaredEndpointInfo.Name().empty())
                            {
                                auto nameMessages = msgs::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
                                    MidiClock::TimestampConstantSendImmediately(),
                                    m_declaredEndpointInfo.Name()
                                );

                                //OutputDebugString(L"MIDI SDK: Responding with Endpoint Name Notification\n");

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

                            if (!m_declaredEndpointInfo.ProductInstanceId().empty())
                            {
                                auto instanceIdMessages = msgs::MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
                                    MidiClock::TimestampConstantSendImmediately(),
                                    m_declaredEndpointInfo.ProductInstanceId()
                                );

                                //OutputDebugString(L"MIDI SDK: Responding with Product Instance Id Notification\n");

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
                            uint8_t protocol{ (uint8_t)midi2enum::MidiProtocol::Midi1 };

                            if (m_declaredEndpointInfo.SupportsMidi20Protocol())
                            {
                                protocol = (uint8_t)midi2enum::MidiProtocol::Midi2;
                            }

                            auto streamConfigurationNotification = msgs::MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(
                                MidiClock::TimestampConstantSendImmediately(),
                                protocol,
                                m_declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps(),
                                m_declaredEndpointInfo.SupportsSendingJitterReductionTimestamps()
                            );

                            //OutputDebugString(L"MIDI SDK: Responding with Stream Configuration Notification\n");

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

                        handled = true;
                    }
                    else if (internal::MessageIsFunctionBlockDiscoveryRequest(message.Word0()))
                    {
                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Function block discovery request received.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                        );

                        //OutputDebugString(L"MIDI SDK: Incoming message is function block discovery request\n");

                        uint8_t filterFlags = internal::GetFunctionBlockDiscoveryMessageFilterFlagsFromFirstWord(message.Word0());

                        bool requestInfo = internal::FunctionBlockDiscoveryFilterRequestsInfoNotification(filterFlags);
                        bool requestName = internal::FunctionBlockDiscoveryFilterRequestsNameNotification(filterFlags);

                        uint8_t fbNumber = internal::GetFunctionBlockNumberFromFunctionBlockDiscoveryRequestFirstWord(message.Word0());

                        if (fbNumber == MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS)
                        {
                            // send all function blocks

                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"Sending ALL function blocks, as requested.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                                TraceLoggingUInt8(m_declaredEndpointInfo.DeclaredFunctionBlockCount(), "Declared FB Count"),
                                TraceLoggingUInt32(m_functionBlocks.Size(), "Actual FB count")
                            );

                            for (uint8_t i = 0; i < min(m_declaredEndpointInfo.DeclaredFunctionBlockCount(), (uint8_t)m_functionBlocks.Size()); i++)
                            {
                               // OutputDebugString(L"MIDI SDK: Responding with Function Block Info notification\n");

                                if (requestInfo) SendFunctionBlockInfoNotificationMessage(m_functionBlocks.Lookup(i));

                                //OutputDebugString(L"MIDI SDK: Responding with Function Block Name notification\n");

                                if (requestName) SendFunctionBlockNameNotificationMessages(m_functionBlocks.Lookup(i));
                            }

                            handled = true;
                        }
                        else
                        {
                            TraceLoggingWrite(
                                Midi2SdkTelemetryProvider::Provider(),
                                MIDI_SDK_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                                TraceLoggingWideString(L"Sending single requested function block.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                                TraceLoggingUInt8(fbNumber, "Function block number")
                            );

                            // send single requested function block
                            if (m_functionBlocks.HasKey(fbNumber))
                            {
                                auto fb = m_functionBlocks.Lookup(fbNumber);

                                //OutputDebugString(L"MIDI SDK: Responding with Function Block Info notification\n");

                                if (requestInfo) SendFunctionBlockInfoNotificationMessage(fb);

                                //OutputDebugString(L"MIDI SDK: Responding with Function Block Name notification\n");

                                if (requestName) SendFunctionBlockNameNotificationMessages(fb);

                                handled = true;
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

                        auto protocol = (midi2enum::MidiProtocol)MIDIWORDBYTE3(message.Word0());
                        bool rxjr = (bool)((message.Word0() & 0x00000002) == 0x00000002);
                        bool txjr = (bool)((message.Word0() & 0x00000001) == 0x00000001);

                        reqArgs->InternalInitialize(message.Timestamp(), protocol, rxjr, txjr);

                        if (m_streamConfigurationRequestReceivedEvent)
                        {
                            m_streamConfigurationRequestReceivedEvent(*this, *reqArgs);

                            handled = true;
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
                //skipMainMessageReceivedEvent = false; // don't need to set this. It may have already been set to true by a previous listener.
            }

        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error processing incoming message.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception processing incoming message.");
        }
    }
}
