// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiOutput.h"
#include "StringResources.h"

namespace res = ::midikeyboard::resources;

namespace midikeyboard
{
    namespace
    {
        // Message type 4, MIDI 2.0 channel voice
        constexpr uint32_t Midi2ChannelVoiceMessageType = 0x4;

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusPitchBend = 0xE;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusRegisteredPerNoteController = 0x0;

        constexpr uint8_t ControllerAllNotesOff = 123;

        // the MIDI 1.0 convention of 64 for "no release velocity information", scaled up
        constexpr uint16_t DefaultReleaseVelocity = 0x8000;

        // These become the MIDI port names other applications remember in their own projects,
        // so they are deliberately fixed rather than localized, and short enough that the
        // service can build a MIDI 1.0 port name from them.
        constexpr wchar_t VirtualEndpointName[] = L"Windows MIDI Keyboard";
        constexpr wchar_t VirtualFunctionBlockName[] = L"Keys";
        constexpr wchar_t VirtualProductInstanceId[] = L"windows-midi-keyboard";
        constexpr wchar_t VirtualManufacturer[] = L"Microsoft";

        // The client endpoint is created by the service as the connection opens, so its id is
        // not available the instant the call returns.
        constexpr int32_t ClientEndpointIdAttempts = 20;
        constexpr int32_t ClientEndpointIdWaitMilliseconds = 50;
    }

    MidiOutput::~MidiOutput() noexcept
    {
        Disconnect();
    }

    bool MidiOutput::IsConnected() const noexcept
    {
        std::shared_lock lock{ m_lock };

        return m_connection != nullptr;
    }

    winrt::hstring MidiOutput::ClientEndpointDeviceId() const noexcept
    {
        std::shared_lock lock{ m_lock };

        return m_clientEndpointDeviceId;
    }

    ConnectResult MidiOutput::ConnectVirtualDevice() noexcept
    {
        try
        {
            Disconnect();

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                return ConnectResult::ServiceUnavailable;
            }

            if (!midi2virt::MidiVirtualDeviceManager::IsTransportAvailable())
            {
                return ConnectResult::VirtualDeviceFailed;
            }

            auto session = midi2::MidiSession::Create(res::GetString(L"AppDisplayName"));

            if (session == nullptr)
            {
                return ConnectResult::SessionFailed;
            }

            midi2enum::MidiDeclaredEndpointInfo endpointInfo{};

            endpointInfo.Name(VirtualEndpointName);
            endpointInfo.ProductInstanceId(VirtualProductInstanceId);
            endpointInfo.SupportsMidi10Protocol(true);
            endpointInfo.SupportsMidi20Protocol(true);
            endpointInfo.SupportsReceivingJitterReductionTimestamps(false);
            endpointInfo.SupportsSendingJitterReductionTimestamps(false);
            endpointInfo.HasStaticFunctionBlocks(true);
            endpointInfo.SpecificationVersionMajor(1);
            endpointInfo.SpecificationVersionMinor(1);

            midi2virt::MidiVirtualDeviceCreationConfig creationConfig{
                endpointInfo.Name(),
                res::GetString(L"VirtualDeviceDescription"),
                VirtualManufacturer,
                endpointInfo };

            midi2enum::MidiFunctionBlock block{};

            block.Number(0);
            block.IsActive(true);
            block.Name(VirtualFunctionBlockName);
            block.FirstGroup(midi2::MidiGroup{ VirtualDeviceGroupIndex });
            block.GroupCount(1);

            // bidirectional so a host can talk back to the keyboard, and hinted the same way so
            // the customer's other applications show it as both a source and a destination
            block.Direction(midi2enum::MidiFunctionBlockDirection::Bidirectional);
            block.UIHint(midi2enum::MidiFunctionBlockUIHint::Bidirectional);

            creationConfig.FunctionBlocks().Append(block);

            auto virtualDevice = midi2virt::MidiVirtualDeviceManager::CreateVirtualDevice(creationConfig);

            if (virtualDevice == nullptr)
            {
                return ConnectResult::VirtualDeviceFailed;
            }

            auto connection = session.CreateEndpointConnection(virtualDevice.DeviceEndpointDeviceId());

            if (connection == nullptr)
            {
                return ConnectResult::ConnectionFailed;
            }

            // this is what associates the virtual device with the connection; the client
            // endpoint other applications see is created when the connection opens
            connection.AddMessageProcessingPlugin(virtualDevice);

            if (!connection.Open())
            {
                return ConnectResult::ConnectionFailed;
            }

            winrt::hstring clientEndpointDeviceId{};

            for (int32_t attempt = 0; attempt < ClientEndpointIdAttempts; attempt++)
            {
                clientEndpointDeviceId = midi2virt::MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(
                    virtualDevice.AssociationId());

                if (!clientEndpointDeviceId.empty())
                {
                    break;
                }

                ::Sleep(ClientEndpointIdWaitMilliseconds);
            }

            {
                std::unique_lock lock{ m_lock };

                m_session = session;
                m_virtualDevice = virtualDevice;
                m_connection = connection;
                m_clientEndpointDeviceId = clientEndpointDeviceId;
            }

            MIDI_KEYBOARD_LOG_INFO_WITH_ENDPOINT(
                L"Virtual device created.", clientEndpointDeviceId.c_str());

            return ConnectResult::Success;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to create the virtual device.")

        return ConnectResult::VirtualDeviceFailed;
    }

    _Use_decl_annotations_
    ConnectResult MidiOutput::ConnectEndpoint(std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            Disconnect();

            if (endpointDeviceId.empty())
            {
                return ConnectResult::NoEndpointChosen;
            }

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                return ConnectResult::ServiceUnavailable;
            }

            auto session = midi2::MidiSession::Create(res::GetString(L"AppDisplayName"));

            if (session == nullptr)
            {
                return ConnectResult::SessionFailed;
            }

            auto connection = session.CreateEndpointConnection(winrt::hstring{ endpointDeviceId });

            if (connection == nullptr || !connection.Open())
            {
                return ConnectResult::ConnectionFailed;
            }

            {
                std::unique_lock lock{ m_lock };

                m_session = session;
                m_connection = connection;
                m_clientEndpointDeviceId = {};
            }

            MIDI_KEYBOARD_LOG_INFO_WITH_ENDPOINT(L"Connected.", endpointDeviceId.c_str());

            return ConnectResult::Success;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to connect to the endpoint.")

        return ConnectResult::ConnectionFailed;
    }

    void MidiOutput::Disconnect() noexcept
    {
        std::unique_lock lock{ m_lock };

        DisconnectUnderLock();
    }

    void MidiOutput::DisconnectUnderLock() noexcept
    {
        try
        {
            if (m_session != nullptr && m_connection != nullptr)
            {
                m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to disconnect the endpoint connection.")

        m_connection = nullptr;
        m_virtualDevice = nullptr;
        m_clientEndpointDeviceId = {};

        try
        {
            if (m_session != nullptr)
            {
                m_session.Close();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to close the session.")

        m_session = nullptr;
    }

    _Use_decl_annotations_
    void MidiOutput::SendChannelVoiceMessage(
        uint8_t group,
        uint8_t status,
        uint8_t channel,
        uint16_t index,
        uint32_t data) noexcept
    {
        try
        {
            midi2::MidiEndpointConnection connection{ nullptr };

            {
                std::shared_lock lock{ m_lock };
                connection = m_connection;
            }

            if (connection == nullptr)
            {
                return;
            }

            uint32_t const word0 =
                (Midi2ChannelVoiceMessageType << 28) |
                (static_cast<uint32_t>(group & 0x0F) << 24) |
                (static_cast<uint32_t>(status & 0x0F) << 20) |
                (static_cast<uint32_t>(channel & 0x0F) << 16) |
                static_cast<uint32_t>(index);

            connection.SendSingleMessageWords(
                midi2::MidiClock::TimestampConstantSendImmediately(), word0, data);
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to send the message.")
    }

    _Use_decl_annotations_
    void MidiOutput::SendNoteOn(uint8_t group, uint8_t channel, uint8_t note, uint16_t velocity) noexcept
    {
        // index low byte is the attribute type, and the low half of the data word is the
        // attribute value; neither is used here
        SendChannelVoiceMessage(
            group, StatusNoteOn, channel,
            static_cast<uint16_t>(static_cast<uint16_t>(note & 0x7F) << 8),
            static_cast<uint32_t>(velocity) << 16);
    }

    _Use_decl_annotations_
    void MidiOutput::SendNoteOff(uint8_t group, uint8_t channel, uint8_t note) noexcept
    {
        SendChannelVoiceMessage(
            group, StatusNoteOff, channel,
            static_cast<uint16_t>(static_cast<uint16_t>(note & 0x7F) << 8),
            static_cast<uint32_t>(DefaultReleaseVelocity) << 16);
    }

    _Use_decl_annotations_
    void MidiOutput::SendPolyPressure(uint8_t group, uint8_t channel, uint8_t note, uint32_t pressure) noexcept
    {
        SendChannelVoiceMessage(
            group, StatusPolyPressure, channel,
            static_cast<uint16_t>(static_cast<uint16_t>(note & 0x7F) << 8),
            pressure);
    }

    _Use_decl_annotations_
    void MidiOutput::SendChannelPressure(uint8_t group, uint8_t channel, uint32_t pressure) noexcept
    {
        SendChannelVoiceMessage(group, StatusChannelPressure, channel, 0, pressure);
    }

    _Use_decl_annotations_
    void MidiOutput::SendPerNoteController(
        uint8_t group,
        uint8_t channel,
        uint8_t note,
        uint8_t controllerIndex,
        uint32_t value) noexcept
    {
        SendChannelVoiceMessage(
            group, StatusRegisteredPerNoteController, channel,
            static_cast<uint16_t>((static_cast<uint16_t>(note & 0x7F) << 8) | (controllerIndex & 0x7F)),
            value);
    }

    _Use_decl_annotations_
    void MidiOutput::SendControlChange(uint8_t group, uint8_t channel, uint8_t controllerIndex, uint32_t value) noexcept
    {
        SendChannelVoiceMessage(
            group, StatusControlChange, channel,
            static_cast<uint16_t>(static_cast<uint16_t>(controllerIndex & 0x7F) << 8),
            value);
    }

    _Use_decl_annotations_
    void MidiOutput::SendPitchBend(uint8_t group, uint8_t channel, uint32_t value) noexcept
    {
        SendChannelVoiceMessage(group, StatusPitchBend, channel, 0, value);
    }

    _Use_decl_annotations_
    void MidiOutput::SendAllNotesOff(uint8_t group, uint8_t channel) noexcept
    {
        SendControlChange(group, channel, ControllerAllNotesOff, 0);
        SendPitchBend(group, channel, 0x80000000);
    }
}
