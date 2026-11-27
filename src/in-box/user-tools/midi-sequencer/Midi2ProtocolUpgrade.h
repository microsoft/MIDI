// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// MIDI 1.0 channel voice messages (UMP type 2) as MIDI 2.0 ones (type 4), for files that must hold
// only MIDI 2.0 Protocol messages, such as MIDI Clip Files. The translation is libmidi2's, which
// follows the default translation in M2-104-UM appendix D.3:
//
//   - Bank select (CC 0 and CC 32) goes into the program change after it, with the bank valid bit
//     set. libmidi2 sets it only when both bank controllers came first.
//   - RPN and NRPN runs (CC 101, 100, 99, 98, 6 and 38) become registered and assignable
//     controllers.
//   - None of those controllers is kept on its own, and neither is the RPN null function.
//
// Every message that isn't MIDI 1.0 channel voice passes through unchanged.

#include <sal.h>

#include <memory>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    class Midi2ProtocolUpgrade
    {
    public:
        Midi2ProtocolUpgrade();
        ~Midi2ProtocolUpgrade();

        Midi2ProtocolUpgrade(Midi2ProtocolUpgrade const&) = delete;
        Midi2ProtocolUpgrade& operator=(Midi2ProtocolUpgrade const&) = delete;

        // Sorts the events by tick, then translates them in that order. Bank selects and RPN
        // selections carry over to the next call, so call it for the parts of one file in order.
        // A data entry that never got its CC 38 goes out at the tick of its CC 6, once the next
        // message on its channel arrives or the call ends.
        void Upgrade(_Inout_ std::vector<ClipEvent>& events);

    private:
        struct State;
        std::unique_ptr<State> m_state;
    };
}
