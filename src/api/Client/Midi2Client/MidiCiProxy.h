// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiCiProxy.g.h"

#include "string_util.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiCiProxy : MidiCiProxyT<MidiCiProxy>
    {
        MidiCiProxy() = default;

        hstring Id() const noexcept { return m_id; }
        void Id(_In_ hstring const& value) noexcept { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        hstring Name() const noexcept { return m_name; }
        void Name(_In_ hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }

        winrt::Windows::Foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ winrt::Windows::Foundation::IInspectable const& value) { m_tag = value; }

        winrt::Windows::Devices::Midi2::IMidiInputConnection InputConnection() const noexcept { return m_inputConnection; }
        void InputConnection(_In_ winrt::Windows::Devices::Midi2::IMidiInputConnection const& value) noexcept { m_inputConnection = value; }

        winrt::Windows::Devices::Midi2::IMidiOutputConnection OutputConnection() const noexcept { return m_outputConnection; }
        void OutputConnection(_In_ winrt::Windows::Devices::Midi2::IMidiOutputConnection const& value) noexcept { m_outputConnection = value; }

        void ProcessIncomingMessage(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent);

        winrt::Windows::Foundation::IAsyncAction ProcessIncomingMessageAsync(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs args);


        void Initialize();
        void Cleanup();


    private:
        hstring m_id{};
        hstring m_name{};
        bool m_enabled{ false };
        IInspectable m_tag{ nullptr };
        IMidiInputConnection m_inputConnection;
        IMidiOutputConnection m_outputConnection;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiCiProxy : MidiCiProxyT<MidiCiProxy, implementation::MidiCiProxy>
    {
    };
}
