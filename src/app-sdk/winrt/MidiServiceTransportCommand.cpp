// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceTransportCommand.h"
#include "ServiceConfig.MidiServiceTransportCommand.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    MidiServiceTransportCommand::MidiServiceTransportCommand(winrt::guid const& transportId) noexcept
    {
        m_transportId = transportId;
    }

    _Use_decl_annotations_
    MidiServiceTransportCommand::MidiServiceTransportCommand(
        winrt::guid const& transportId, 
        winrt::hstring const& verb) noexcept
        : MidiServiceTransportCommand(transportId)
    {
        m_verb = verb;
    }

    _Use_decl_annotations_
    MidiServiceTransportCommand::MidiServiceTransportCommand(
        winrt::guid const& transportId, 
        winrt::hstring const& verb, 
        collections::IMap<winrt::hstring, winrt::hstring> const& arguments) noexcept
        : MidiServiceTransportCommand(transportId, verb)
    {
        m_arguments = arguments;
    }




    winrt::hstring MidiServiceTransportCommand::GetConfigJson() const noexcept
    {
        // temp

        return L"";
    }
}
