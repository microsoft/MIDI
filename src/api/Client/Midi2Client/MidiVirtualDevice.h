// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiVirtualDeviceResponder.g.h"

#include "string_util.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiVirtualDeviceResponder : MidiVirtualDeviceResponderT<MidiVirtualDeviceResponder>
    {
        MidiVirtualDeviceResponder() = default;

        winrt::hstring Id() const noexcept { return m_id; }
        void Id(_In_ winrt::hstring const& value) noexcept { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }


        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable const& value) { m_tag = value; }

        midi2::MidiEndpointConnection Connection() const noexcept { return m_endpointConnection; }
        void Connection(_In_ midi2::MidiEndpointConnection const& value) noexcept { m_endpointConnection = value; }




        collections::IMapView<uint8_t, midi2::MidiFunctionBlock> FunctionBlocks() { return m_functionBlocks.GetView(); }

        bool AddFunctionBlock(_In_ midi2::MidiFunctionBlock const& block);
        void UpdateFunctionBlock(_In_ midi2::MidiFunctionBlock const& block);
        void RemoveFunctionBlock(_In_ uint8_t functionBlockNumber);

        //midi2::MidiEndpointInformation EndpointInformation();
        //void EndpointInformation(_In_ midi2::MidiEndpointInformation const& value);

        bool SuppressHandledMessages() { return m_suppressHandledMessages; }
        void SuppressHandledMessages(_In_ bool const value) { m_suppressHandledMessages = value; }

        void Initialize();
        void OnEndpointConnectionOpened();
        void Cleanup();

        void ProcessIncomingMessage(
            _In_ midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent);


    private:
        winrt::hstring m_id{};
        winrt::hstring m_name{};
        bool m_enabled{ true };
        foundation::IInspectable m_tag{ nullptr };
        midi2::MidiEndpointConnection m_endpointConnection{ nullptr };

        bool m_suppressHandledMessages{ true };

        collections::IMap<uint8_t, midi2::MidiFunctionBlock> m_functionBlocks { winrt::single_threaded_map<uint8_t, midi2::MidiFunctionBlock>() };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiVirtualDeviceResponder : MidiVirtualDeviceResponderT<MidiVirtualDeviceResponder, implementation::MidiVirtualDeviceResponder>
    {
    };
}
