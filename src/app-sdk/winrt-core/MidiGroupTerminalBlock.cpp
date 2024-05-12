// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiGroupTerminalBlock.h"
#include "MidiGroupTerminalBlock.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    // this conversion will never be 100% because the GTBs just contain slightly
    // different data than Function Blocks. However, it will provide enough information
    // to enable consuming code to represent the GTB properly in the absence of
    // actual function blocks for the endpoint
    midi2::MidiFunctionBlock MidiGroupTerminalBlock::AsEquivalentFunctionBlock()
    {
        auto fb = winrt::make_self<implementation::MidiFunctionBlock>();

        fb->InternalSetisReadOnly(false);

        fb->IsActive(true);
        fb->Number(m_block.Number);
        fb->Name(m_block.Name.c_str());
        fb->FirstGroupIndex(m_block.FirstGroupIndex);
        fb->GroupCount(m_block.GroupCount);

        switch ((midi2::MidiGroupTerminalBlockDirection)m_block.Direction)
        {
        case midi2::MidiGroupTerminalBlockDirection::Bidirectional:
            fb->Direction(midi2::MidiFunctionBlockDirection::Bidirectional);
            break;

        case midi2::MidiGroupTerminalBlockDirection::BlockInput:
            fb->Direction(midi2::MidiFunctionBlockDirection::BlockInput);
            break;

        case midi2::MidiGroupTerminalBlockDirection::BlockOutput:
            fb->Direction(midi2::MidiFunctionBlockDirection::BlockOutput);
            break;
        }

        switch ((midi2::MidiGroupTerminalBlockProtocol)m_block.Protocol)
        {
        case midi2::MidiGroupTerminalBlockProtocol::Unknown:
            // Unknown is a tough one. We don't do CI negotiation as it is deprecated.
            // We make a guess, but really, the device should present itself properly 
            // through declaring the protocol in the GTB, or even better, giving us 
            // actual function blocks to use. 
            if (m_block.GroupCount == 1)
            {
                // No SysEx 8 and we treat as MIDI 1.0 bandwidth restricted
                fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted);
                fb->MaxSystemExclusive8Streams(0); 
            }
            else
            {
                // We're assuming a basic MIDI 2.0 endpoint here because more than 1 group
                fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
                fb->MaxSystemExclusive8Streams(1);
            }
            break;

        case midi2::MidiGroupTerminalBlockProtocol::Midi1Message64:
            // This is MIDI 1.0, but supports 64 bit SysEx7 messages
            fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted);
            fb->MaxSystemExclusive8Streams(0); // MIDI 1.0 protocol, so no SysEx 8. This is a huge assumption and is likely to be incorrect.
            break;

        case midi2::MidiGroupTerminalBlockProtocol::Midi1Message64WithJitterReduction:
            // This is MIDI 1.0, but supports 64 bit SysEx7 messages
            // we don't support JR timestamps unless they are negotiated through UMP endpoint discovery/protocol negotiation
            fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted);
            fb->MaxSystemExclusive8Streams(0); // MIDI 1.0 protocol, so no SysEx 8
            break;

        case midi2::MidiGroupTerminalBlockProtocol::Midi1Message128:
            fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted);
            fb->MaxSystemExclusive8Streams(0); // MIDI 1.0 protocol, so no SysEx 8
            break;


        case midi2::MidiGroupTerminalBlockProtocol::Midi1Message128WithJitterReduction:
            // we don't support JR timestamps unless they are negotiated through UMP endpoint discovery/protocol negotiation
            fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted);
            fb->MaxSystemExclusive8Streams(0); // MIDI 1.0 protocol, so no SysEx 8
            break;

        // Most of these devices will implement function blocks
        case midi2::MidiGroupTerminalBlockProtocol::Midi2:
            fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
            fb->MaxSystemExclusive8Streams(1);  // guess
            break;

        case midi2::MidiGroupTerminalBlockProtocol::Midi2WithJitterReduction:
            fb->RepresentsMidi10Connection(midi2::MidiFunctionBlockRepresentsMidi10Connection::Not10);
            fb->MaxSystemExclusive8Streams(1);  // guess
            break;
        }

        fb->UIHint(midi2::MidiFunctionBlockUIHint::Unknown);

        fb->InternalSetisReadOnly(true);

        return *fb;
    }


    
    _Use_decl_annotations_
    bool MidiGroupTerminalBlock::InternalUpdateFromPropertyData(internal::GroupTerminalBlockInternal block)
    {
        m_block = block;

        return true;
    }


    // Note: 0x0000 is unknown or not fixed
    // 0x0001 is 31.25kb/s
    // all others are 4kb/second * value

    _Use_decl_annotations_
    uint32_t MidiGroupTerminalBlock::CalculateBandwidth(uint16_t gtbBandwidthValue)
    {
        if (gtbBandwidthValue == 0)
        {
            return (uint32_t)0;
        }
        else if (gtbBandwidthValue == 0x0001)
        {
            return (uint32_t)31250;
        }
        else
        {
            // 4000, not 4096, for 4k bps
            return (uint32_t)(gtbBandwidthValue * 4000);
        }
    }

    uint32_t MidiGroupTerminalBlock::CalculatedMaxDeviceInputBandwidthBitsPerSecond()
    {
        return CalculateBandwidth(m_block.MaxInputBandwidth);
    }

    uint32_t MidiGroupTerminalBlock::CalculatedMaxDeviceOutputBandwidthBitsPerSecond()
    {
        return CalculateBandwidth(m_block.MaxOutputBandwidth);
    }


}
