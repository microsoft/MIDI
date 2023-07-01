// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiUmpWithTimestamp.h"
#include "MidiUmpWithTimestamp.g.cpp"

#include "ump_helpers.h"

using namespace ::Microsoft::Devices::Midi2::Internal;

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp MidiUmpWithTimestamp::FromUmp32(winrt::Microsoft::Devices::Midi2::MidiUmp32 const& ump)
    {
        auto umpClass = winrt::make<MidiUmpWithTimestamp>();
        umpClass.Timestamp(ump.Timestamp);
        umpClass.Words().Append(ump.Word1);

        return umpClass;

    }
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp MidiUmpWithTimestamp::FromUmp64(winrt::Microsoft::Devices::Midi2::MidiUmp64 const& ump)
    {
        auto umpClass = winrt::make<MidiUmpWithTimestamp>();
        umpClass.Timestamp(ump.Timestamp);
        umpClass.Words().Append(ump.Word1);
        umpClass.Words().Append(ump.Word2);

        return umpClass;
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp MidiUmpWithTimestamp::FromUmp96(winrt::Microsoft::Devices::Midi2::MidiUmp96 const& ump)
    {
        auto umpClass = winrt::make<MidiUmpWithTimestamp>();
        umpClass.Timestamp(ump.Timestamp);
        umpClass.Words().Append(ump.Word1);
        umpClass.Words().Append(ump.Word2);
        umpClass.Words().Append(ump.Word3);

        return umpClass;
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp MidiUmpWithTimestamp::FromUmp128(winrt::Microsoft::Devices::Midi2::MidiUmp128 const& ump)
    {
        auto umpClass = winrt::make<MidiUmpWithTimestamp>();
        umpClass.Timestamp(ump.Timestamp);
        umpClass.Words().Append(ump.Word1);
        umpClass.Words().Append(ump.Word2);
        umpClass.Words().Append(ump.Word3);
        umpClass.Words().Append(ump.Word4);

        return umpClass;
    }
    uint64_t MidiUmpWithTimestamp::Timestamp()
    {
        return _timestamp;
    }
    void MidiUmpWithTimestamp::Timestamp(uint64_t value)
    {
        _timestamp = value;
    }
    winrt::Windows::Foundation::Collections::IVector<uint32_t> MidiUmpWithTimestamp::Words()
    {
        return _words;
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpMessageType MidiUmpWithTimestamp::MessageType()
    {
        winrt::check_bool(bool(_words.Size() > 0));

        return (MidiUmpMessageType)GetUmpMessageTypeFromFirstWord(_words.GetAt(0));

    }


    hstring MidiUmpWithTimestamp::ToString()
    {
        // TODO: Have this use the localized information for the message type, not the generic info below

        switch (_words.Size())
        {
        case 0:
            return L"Uninitialized UMP";
        case 1:
            return L"UMP 32";
        case 2:
            return L"UMP 64";
        case 3:
            return L"UMP 96";
        case 4:
            return L"UMP 128";
        default:
            return L"Invalid UMP";
        }
    }
}
