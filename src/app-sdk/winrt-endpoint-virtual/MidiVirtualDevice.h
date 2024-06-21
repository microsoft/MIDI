// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#pragma once
#include "MidiVirtualDevice.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    struct MidiVirtualDevice : MidiVirtualDeviceT<MidiVirtualDevice>
    {
        MidiVirtualDevice() = default;

        // plugin Id
        winrt::guid PluginId() const noexcept { return m_id; }

        // plugin name
        winrt::hstring PluginName() const noexcept { return m_name; }
        void PluginName(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        // plugin isEnabled
        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }

        // plugin tag
        foundation::IInspectable PluginTag() const noexcept { return m_tag; }
        void PluginTag(_In_ foundation::IInspectable const& value) { m_tag = value; }

        winrt::event_token StreamConfigRequestReceived(_In_ foundation::TypedEventHandler<virt::MidiVirtualDevice, virt::MidiStreamConfigRequestReceivedEventArgs> const& handler)
        {
            return m_streamConfigurationRequestReceivedEvent.add(handler);
        }

        void StreamConfigRequestReceived(winrt::event_token const& token) noexcept
        {
            if (m_streamConfigurationRequestReceivedEvent) m_streamConfigurationRequestReceivedEvent.remove(token);
        }


        bool SuppressHandledMessages() { return m_suppressHandledMessages; }
        void SuppressHandledMessages(_In_ bool const value) { m_suppressHandledMessages = value; }

        // TODO: This should be the calcualted final name
        winrt::hstring EndpointName() const noexcept { return m_declaredEndpointInfo.Name; }
        winrt::hstring EndpointProductInstanceId() const noexcept { return m_declaredEndpointInfo.ProductInstanceId; }

        bool AreFunctionBlocksStatic() { return m_declaredEndpointInfo.HasStaticFunctionBlocks; }
        collections::IMapView<uint8_t, midi2::MidiFunctionBlock> FunctionBlocks() noexcept { return m_functionBlocks.GetView(); }

        bool UpdateFunctionBlock(_In_ midi2::MidiFunctionBlock const& block) noexcept;
        bool UpdateEndpointName(_In_ winrt::hstring const& name) noexcept;


        winrt::hstring DeviceEndpointDeviceId() const noexcept{ return m_deviceEndpointDeviceId; }
        //winrt::hstring ClientEndpointDeviceId() const noexcept{ return m_clientEndpointDeviceId; }


        void Initialize(_In_ midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept;
        void OnEndpointConnectionOpened()  noexcept;
        void Cleanup()  noexcept;

        void ProcessIncomingMessage(
            _In_ midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent)  noexcept;



        void InternalInitialize(
            _In_ winrt::hstring const& deviceEndpointDeviceId, 
            _In_ virt::MidiVirtualDeviceCreationConfig const& config
            ) noexcept;

    private:
        //virt::IMidiVirtualDeviceCreationConfiguration m_virtualDeviceConfiguration{ nullptr };

        midi2::MidiDeclaredEndpointInfo m_declaredEndpointInfo{};
        midi2::MidiDeclaredDeviceIdentity m_declaredDeviceIdentity{};

        bool SendFunctionBlockInfoNotificationMessage(_In_ midi2::MidiFunctionBlock const& fb) noexcept;
        bool SendFunctionBlockNameNotificationMessages(_In_ midi2::MidiFunctionBlock const& fb) noexcept;
        bool SendEndpointNameNotificationMessages(_In_ winrt::hstring const& name) noexcept;

        //void MidiVirtualEndpointDevice::QueueWorker();

        //void EnqueueOutgoingMessage(_In_ internal::PackedUmp128 const& message);

        //std::atomic<bool> m_continueProcessing{ true };
        //wil::slim_event_manual_reset m_messageProcessorWakeup;
        //std::queue<internal::PackedUmp128> m_outgoingMessageQueue;
        //std::mutex m_queueMutex;
        //std::thread m_queueWorkerThread;


        winrt::guid m_id{ foundation::GuidHelper::CreateNewGuid() };                         // plugin id
        winrt::hstring m_name{};                    // plugin name, not the endpointdevice name
        bool m_enabled{ true };                     // plugin enabled, not the endpointdevice enabled
        foundation::IInspectable m_tag{ nullptr };  // plugin tag, not the endpointdevice tag

        midi2::MidiEndpointConnection m_endpointConnection{ nullptr };

        winrt::hstring m_deviceEndpointDeviceId{};
        //winrt::hstring m_clientEndpointDeviceId{};

        bool m_isMidiCIDevice{ false };
        //winrt::hstring m_endpointName{};
        //winrt::hstring m_endpointProductInstanceId{};

        //bool m_areFunctionBlocksStatic{ false };

        bool m_suppressHandledMessages{ true };

        collections::IMap<uint8_t, midi2::MidiFunctionBlock> m_functionBlocks { winrt::single_threaded_map<uint8_t, midi2::MidiFunctionBlock>() };

        winrt::event<foundation::TypedEventHandler<virt::MidiVirtualDevice, virt::MidiStreamConfigRequestReceivedEventArgs>> m_streamConfigurationRequestReceivedEvent;
    };
}

