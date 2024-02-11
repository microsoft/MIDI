// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiVirtualEndpointDevice.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiVirtualEndpointDevice : MidiVirtualEndpointDeviceT<MidiVirtualEndpointDevice>
    {
        MidiVirtualEndpointDevice() = default;

        // plugin Id
        winrt::guid Id() const noexcept { return m_id; }

        // plugin name
        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        // plugin isEnabled
        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }

        // plugin tag
        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable const& value) { m_tag = value; }

        winrt::event_token StreamConfigurationRequestReceived(_In_ foundation::TypedEventHandler<midi2::MidiVirtualEndpointDevice, midi2::MidiStreamConfigurationRequestReceivedEventArgs> const& handler)
        {
            return m_streamConfigurationRequestReceivedEvent.add(handler);
        }

        void StreamConfigurationRequestReceived(winrt::event_token const& token) noexcept
        {
            if (m_streamConfigurationRequestReceivedEvent) m_streamConfigurationRequestReceivedEvent.remove(token);
        }


        bool SuppressHandledMessages() { return m_suppressHandledMessages; }
        void SuppressHandledMessages(_In_ bool const value) { m_suppressHandledMessages = value; }

        winrt::hstring EndpointName() const noexcept { return m_endpointName; }
        winrt::hstring EndpointProductInstanceId() const noexcept { return m_endpointProductInstanceId; }

        bool AreFunctionBlocksStatic() { return m_areFunctionBlocksStatic; }
        collections::IMapView<uint8_t, midi2::MidiFunctionBlock> FunctionBlocks() noexcept { return m_functionBlocks.GetView(); }

        void UpdateFunctionBlock(_In_ midi2::MidiFunctionBlock const& block) noexcept;
        void UpdateEndpointName(_In_ winrt::hstring const& name) noexcept;


        midi2::MidiVirtualEndpointDeviceDefinition DeviceDefinition() { return m_virtualEndpointDeviceDefinition; }



        void Initialize(_In_ midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept;
        void OnEndpointConnectionOpened()  noexcept;
        void Cleanup()  noexcept;

        void InternalSetDeviceDefinition(_In_ midi2::MidiVirtualEndpointDeviceDefinition definition);

        void ProcessIncomingMessage(
            _In_ midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent)  noexcept;


    private:
        void SendFunctionBlockInfoNotificationMessage(_In_ midi2::MidiFunctionBlock const& fb) noexcept;
        void SendFunctionBlockNameNotificationMessages(_In_ midi2::MidiFunctionBlock const& fb) noexcept;
        

        midi2::MidiVirtualEndpointDeviceDefinition m_virtualEndpointDeviceDefinition{ nullptr };

        winrt::guid m_id{ foundation::GuidHelper::CreateNewGuid() };                         // plugin id
        winrt::hstring m_name{};                    // plugin name, not the endpointdevice name
        bool m_enabled{ true };                     // plugin enabled, not the endpointdevice enabled
        foundation::IInspectable m_tag{ nullptr };  // plugin tag, not the endpointdevice tag

        midi2::MidiEndpointConnection m_endpointConnection{ nullptr };

        bool m_isMidiCIDevice{ false };
        winrt::hstring m_endpointName{};
        winrt::hstring m_endpointProductInstanceId{};

        bool m_areFunctionBlocksStatic{ false };

        bool m_suppressHandledMessages{ true };

        collections::IMap<uint8_t, midi2::MidiFunctionBlock> m_functionBlocks { winrt::single_threaded_map<uint8_t, midi2::MidiFunctionBlock>() };

        winrt::event<foundation::TypedEventHandler<midi2::MidiVirtualEndpointDevice, midi2::MidiStreamConfigurationRequestReceivedEventArgs>> m_streamConfigurationRequestReceivedEvent;
    };
}

