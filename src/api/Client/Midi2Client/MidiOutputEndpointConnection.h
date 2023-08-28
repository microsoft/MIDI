// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include <pch.h>

#include "MidiOutputEndpointConnection.g.h"
#include "MidiEndpointConnection.h"
#include "midi_service_interface.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

#include "InternalMidiMessageSenderHelper.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<
        MidiOutputEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection>
    {
        MidiOutputEndpointConnection() = default;
        ~MidiOutputEndpointConnection();

        static hstring GetDeviceSelector() noexcept { return L"System.Devices.InterfaceClassGuid:=\"{3705DC2B-17A7-4452-98CE-BF12C6F48A0B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"; }

        _Success_(return == true)
        bool SendUmp(
            _In_ winrt::Windows::Devices::Midi2::IMidiUmp const& ump);

        _Success_(return == true)
        bool SendUmpWordArray(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ array_view<uint32_t const> words, 
            _In_ uint32_t const wordCount);

        _Success_(return == true)
        bool SendUmpBuffer(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ winrt::Windows::Foundation::IMemoryBuffer const& buffer, 
            _In_ uint32_t const byteOffset,
            _In_ uint32_t const byteLength);

        _Success_(return == true)
        bool SendUmpWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0);

        _Success_(return == true)
        bool SendUmpWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1);
        
        _Success_(return == true)
        bool SendUmpWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2);

        _Success_(return == true)
        bool SendUmpWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2,
            _In_ uint32_t const word3);

        _Success_(return == true)
            bool InternalInitialize(
                _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction);

        _Success_(return == true)
            bool Open();


    private:
        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction{ nullptr };
        winrt::com_ptr<IMidiOut> m_endpointInterface{ nullptr };

        internal::InternalMidiMessageSenderHelper<IMidiOut> m_messageSenderHelper;



        _Success_(return == true)
            bool ActivateMidiStream(
                _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
                _In_ const IID & iid,
                _Out_ void** iface);
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<MidiOutputEndpointConnection, implementation::MidiOutputEndpointConnection>
    {
    };
}
