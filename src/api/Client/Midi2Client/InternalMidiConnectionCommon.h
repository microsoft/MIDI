// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

namespace midi2 = ::winrt::Windows::Devices::Midi2;


namespace Windows::Devices::Midi2::Internal
{
    class InternalMidiConnectionCommon

    {
    public:
        winrt::hstring ConnectionId() const noexcept { return m_id; }
        bool IsOpen() const noexcept { return m_isOpen; }
        midi2::IMidiEndpointDefinedConnectionSettings Settings() const noexcept { return m_settings; }

        midi2::MidiEndpointConnectionSharing ActiveSharingMode() const noexcept { return m_activeSharingMode; }

        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable value) noexcept { m_tag = value; }

        winrt::Windows::Foundation::Collections::IVector<midi2::IMidiEndpointMessageProcessingPlugin> MessageProcessingPlugins() const noexcept
            { return m_messageProcessingPlugins; }


        void SetInputConnectionOnPlugins(_In_ midi2::IMidiInputConnection const inputConnection);
        void SetOutputConnectionOnPlugins(_In_ midi2::IMidiOutputConnection const outputConnection);

        void InitializePlugins();
        void CallOnConnectionOpenedOnPlugins();
        void CleanupPlugins();


    protected:
        void IsOpen(_In_ const bool value) noexcept { m_isOpen = value; }

        winrt::hstring m_id{};
        winrt::Windows::Foundation::IInspectable m_tag{ nullptr };
        midi2::MidiEndpointConnectionSharing m_activeSharingMode
            { midi2::MidiEndpointConnectionSharing::Unknown };

        midi2::IMidiEndpointDefinedConnectionSettings m_settings{ nullptr };

        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction{ nullptr };


        foundation::Collections::IVector<midi2::IMidiEndpointMessageProcessingPlugin>
            m_messageProcessingPlugins{ winrt::single_threaded_vector<midi2::IMidiEndpointMessageProcessingPlugin>() };

        _Success_(return == true)
        bool ActivateMidiStream(
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ const IID& iid,
            _Out_ void** iface);

    private:
        bool m_isOpen{ false };

    };

}
