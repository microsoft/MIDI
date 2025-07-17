// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI1_MESSAGE_DEFS_H
#define MIDI1_MESSAGE_DEFS_H

// Channel voice messages include the channel as the low nibble of the status byte

#define MIDI_NOTEOFF                0x80    // Note-off:                3 bytes
#define MIDI_NOTEON                 0x90    // Note-on:                 3 bytes
#define MIDI_POLYAFTERTOUCH         0xa0    // Polyphonic Aftertouch:   3 bytes
#define MIDI_CONTROLCHANGE          0xb0    // Control/Mode Change:     3 bytes
#define MIDI_PROGRAMCHANGE          0xc0    // Program Change:          2 bytes
#define MIDI_MONOAFTERTOUCH         0xd0    // Channel Aftertouch       2 bytes
#define MIDI_PITCHBEND              0xe0    // Pitch bend               3 bytes

// these messages do not include the channel in the lower nibble. 
// That's part of the status code itself.

#define MIDI_SYSEX                  0xf0    // SysEx start              1 byte + data stream
#define MIDI_TIMECODE               0xf1    // Timecode quarter frame   2 bytes
#define MIDI_SONGPOSITIONPOINTER    0xf2    // Song position pointer    3 bytes
#define MIDI_SONGSELECT             0xf3    // Song Select              2 bytes

#define MIDI_TUNEREQUEST            0xf6    // Tune request             1 byte
#define MIDI_EOX                    0xf7    // End of SysEx             1 byte
#define MIDI_TIMINGCLOCK            0xf8    // Timing clock             1 byte
#define MIDI_START                  0xfa    // Start playing            1 byte
#define MIDI_CONTINUE               0xfb    // Continue playing         1 byte
#define MIDI_STOP                   0xfc    // Stop playing             1 byte
#define MIDI_ACTIVESENSE            0xfe    // Active Sense             1 byte
#define MIDI_RESET                  0xff    // System Reset             1 byte

#define MIDI_STATUSBYTEFILTER       0x80    // filter for MIDI status byte to remove channel info
// a status byte must have the high bit set (0x80)
#define MIDI_SYSTEM_REALTIME_FILTER 0xf8    // filter for MIDI system realtime messages
                                            // a system realtime message must have all these bits set


// For reference, http://midi.teragonaudio.com/tech/midispec.htm
// and https://midi.org/expanded-midi-1-0-messages-list

#define MIDI_MESSAGE_IS_THREE_BYTES(status) ( \
                    ((status & 0xF0) == MIDI_NOTEOFF) || \
                    ((status & 0xF0) == MIDI_NOTEON) || \
                    ((status & 0xF0) == MIDI_POLYAFTERTOUCH) || \
                    ((status & 0xF0) == MIDI_CONTROLCHANGE) || \
                    ((status & 0xF0) == MIDI_PITCHBEND) || \
                    (status == MIDI_SONGPOSITIONPOINTER))

#define MIDI_MESSAGE_IS_TWO_BYTES(status) ( \
                    ((status & 0xF0) == MIDI_PROGRAMCHANGE) || \
                    ((status & 0xF0) == MIDI_MONOAFTERTOUCH) || \
                    (status == MIDI_TIMECODE) || \
                    (status == MIDI_SONGSELECT))

#define MIDI_MESSAGE_IS_ONE_BYTE(status) ( \
                    (status == MIDI_TUNEREQUEST) || \
                    (status == MIDI_TIMINGCLOCK) || \
                    (status == MIDI_START) || \
                    (status == MIDI_CONTINUE) || \
                    (status == MIDI_STOP) || \
                    (status == MIDI_ACTIVESENSE) || \
                    (status == MIDI_RESET) || \
                    (status == MIDI_EOX))


#define MIDI_MESSAGE_TERMINATES_RUNNING_STATUS(status) (\
                    status >= MIDI_SYSEX && \
                    status <= MIDI_EOX )

#define MIDI_STATUS_SUPPORTS_RUNNING_STATUS(status) (\
                    status >= MIDI_NOTEOFF && \
                    status < MIDI_SYSEX)

#define MIDI_BYTE_IS_STATUS_BYTE(b) (\
                    (b & MIDI_STATUSBYTEFILTER) == MIDI_STATUSBYTEFILTER)

#define MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(b) (\
                    (b == MIDI_TIMINGCLOCK) || \
                    (b == MIDI_START) || \
                    (b == MIDI_CONTINUE) || \
                    (b == MIDI_STOP) || \
                    (b == MIDI_ACTIVESENSE) || \
                    (b == MIDI_RESET) \
                    )

#define MIDI_BYTE_IS_SYSEX_START_STATUS(b) (\
                    b == MIDI_SYSEX)

#define MIDI_BYTE_IS_SYSEX_END_STATUS(b) (\
                    b == MIDI_EOX)

#define MIDI_BYTE_IS_DATA_BYTE(b) (\
                    (b &  MIDI_STATUSBYTEFILTER) == 0)

#define MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(b) (\
                    b < MIDI_SYSEX)
                    

#endif