// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: building a sequence in memory with MidiSequenceBuilder,
// and playing it.
//
// A MidiSequence usually comes from a file. The builder makes the same thing out
// of messages your application decides on: a phrase behind a button, a backing
// part worked out while someone plays, a test pattern. The player works with it
// without knowing where it came from, and so does anything that draws notes.
//
// This builds two sequences. The first counts in beats, so a tempo change moves
// everything after it. The second counts in microseconds, for the times when a
// pause has to last a set time, whatever the tempo.
//
// Both play on the General MIDI synthesizer that comes with Windows MIDI
// Services, so you will hear them on your default audio device.

#include <iostream>
#include <iomanip>
#include <array>
#include <thread>
#include <chrono>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Transports.Synth.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Sequencing.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Transports::Synth;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;
using namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing;


// The endpoint to play on. Leave empty to use the General MIDI synthesizer.
const winrt::hstring DestinationEndpointId = L"";

// A tick is the smallest step of musical time. 480 to the quarter note divides
// evenly into eighths, sixteenths and triplets.
constexpr uint16_t TicksPerQuarterNote = 480;
constexpr uint32_t TicksPerEighthNote = TicksPerQuarterNote / 2;
constexpr uint32_t TicksPerHalfNote = TicksPerQuarterNote * 2;
constexpr uint32_t TicksPerBar = TicksPerQuarterNote * 4;

// Channel 1 is index 0.
constexpr uint8_t PianoChannelIndex = 0;
constexpr uint8_t StringsChannelIndex = 1;
constexpr uint8_t BassChannelIndex = 2;
constexpr uint8_t GuitarChannelIndex = 3;

// General MIDI program numbers. They are zero based, which is how they are sent.
constexpr uint8_t AcousticGrandPiano = 0;
constexpr uint8_t NylonStringGuitar = 24;
constexpr uint8_t AcousticBass = 32;
constexpr uint8_t StringEnsemble = 48;

constexpr uint8_t ExpressionController = 11;

constexpr uint8_t OnBeatVelocity = 88;
constexpr uint8_t OffBeatVelocity = 64;

struct Chord
{
    std::array<uint8_t, 3> Notes;
    uint8_t Bass;
};

// One bar each: C, A minor, F and G. A fifth bar goes home to C.
constexpr std::array<Chord, 4> Progression
{ {
    { { 60, 64, 67 }, 48 },
    { { 57, 60, 64 }, 45 },
    { { 53, 57, 60 }, 41 },
    { { 55, 59, 62 }, 43 },
} };


// The builder has a method for notes, because a note needs an end as well as a
// start. Every other message goes in as Universal MIDI Packet words, which
// MidiMessageBuilder makes for you. The group in the words is replaced by the
// group the sequence is played on, so what you put there does not matter.
void AddChannelVoiceMessage(
    MidiSequenceBuilder const& builder,
    uint16_t const trackIndex,
    uint32_t const tick,
    Midi1ChannelVoiceMessageStatus const status,
    uint8_t const channelIndex,
    uint8_t const data1,
    uint8_t const data2)
{
    auto const message = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        0,
        MidiGroup{ static_cast<uint8_t>(0) },
        status,
        MidiChannel{ channelIndex },
        data1,
        data2);

    builder.AddMessages(trackIndex, tick, { message.Word0() });
}

// The same, for a MIDI 2.0 control change. Its value is 32 bits rather than the
// 7 bits MIDI 1.0 allows. The words are stored and played exactly as built.
void AddMidi2ControlChange(
    MidiSequenceBuilder const& builder,
    uint16_t const trackIndex,
    uint32_t const tick,
    uint8_t const channelIndex,
    uint8_t const controller,
    uint32_t const value)
{
    auto const message = MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        0,
        MidiGroup{ static_cast<uint8_t>(0) },
        Midi2ChannelVoiceMessageStatus::ControlChange,
        MidiChannel{ channelIndex },
        static_cast<uint16_t>(controller << 8),     // the controller number is the high byte
        value);

    builder.AddMessages(trackIndex, tick, { message.Word0(), message.Word1() });
}


MidiSequence BuildMusicalSequence(MidiSequenceBuilder const& builder)
{
    builder.TimingMode(MidiSequenceTimingMode::Musical);
    builder.TicksPerQuarterNote(TicksPerQuarterNote);

    // Tempo and meter belong to the whole sequence, not to a track.
    builder.AddTimeSignature(0, 4, 4);
    builder.AddTempoChange(0, 100.0);

    // Slow down a little on each beat of the fourth bar, and a little more for
    // the last. Because this sequence counts in beats, everything after a tempo
    // change moves with it.
    for (uint32_t beat = 0; beat < 4; beat++)
    {
        builder.AddTempoChange(3 * TicksPerBar + beat * TicksPerQuarterNote, 96.0 - beat * 5.0);
    }

    auto const lastBarTick = static_cast<uint32_t>(Progression.size()) * TicksPerBar;

    builder.AddTempoChange(lastBarTick, 76.0);

    // The index each call returns is what you pass to the methods below.
    auto const piano = builder.AddTrack(L"Piano");
    auto const strings = builder.AddTrack(L"Strings");
    auto const bass = builder.AddTrack(L"Bass");

    AddChannelVoiceMessage(builder, piano, 0, Midi1ChannelVoiceMessageStatus::ProgramChange, PianoChannelIndex, AcousticGrandPiano, 0);
    AddChannelVoiceMessage(builder, strings, 0, Midi1ChannelVoiceMessageStatus::ProgramChange, StringsChannelIndex, StringEnsemble, 0);
    AddChannelVoiceMessage(builder, bass, 0, Midi1ChannelVoiceMessageStatus::ProgramChange, BassChannelIndex, AcousticBass, 0);

    for (uint32_t bar = 0; bar < Progression.size(); bar++)
    {
        auto const barTick = bar * TicksPerBar;
        auto const& chord = Progression[bar];

        // Eighth notes, up through the chord to the octave and back down.
        std::array<uint8_t, 8> const arpeggio
        {
            chord.Notes[0], chord.Notes[1], chord.Notes[2], static_cast<uint8_t>(chord.Notes[0] + 12),
            chord.Notes[2], chord.Notes[1], chord.Notes[0], chord.Notes[1]
        };

        for (uint32_t step = 0; step < arpeggio.size(); step++)
        {
            // AddNote writes the note on and the note off together, so a note
            // added this way can never be left sounding.
            builder.AddNote(
                barTick + step * TicksPerEighthNote,
                TicksPerEighthNote,
                piano,
                MidiChannel{ PianoChannelIndex },
                arpeggio[step],
                step % 2 == 0 ? OnBeatVelocity : OffBeatVelocity);
        }

        for (auto const note : chord.Notes)
        {
            builder.AddNote(barTick, TicksPerBar, strings, MidiChannel{ StringsChannelIndex }, note, 72);
        }

        builder.AddNote(barTick, TicksPerHalfNote, bass, MidiChannel{ BassChannelIndex }, chord.Bass, 96);
        builder.AddNote(barTick + TicksPerHalfNote, TicksPerHalfNote, bass, MidiChannel{ BassChannelIndex }, chord.Bass, 80);
    }

    std::array<uint8_t, 4> const finalChord{ 60, 64, 67, 72 };

    for (auto const note : finalChord)
    {
        builder.AddNote(lastBarTick, TicksPerBar, piano, MidiChannel{ PianoChannelIndex }, note, 80);
    }

    builder.AddNote(lastBarTick, TicksPerBar, bass, MidiChannel{ BassChannelIndex }, 36, 90);

    // The strings swell from a quarter of full expression to all of it over the
    // first four bars, in sixteenth note steps.
    constexpr uint32_t SwellSteps = 64;
    constexpr uint32_t SwellStartValue = 0x40000000;

    for (uint32_t step = 0; step <= SwellSteps; step++)
    {
        auto const value = SwellStartValue +
            static_cast<uint32_t>(static_cast<uint64_t>(0xFFFFFFFF - SwellStartValue) * step / SwellSteps);

        AddMidi2ControlChange(builder, strings, step * (lastBarTick / SwellSteps), StringsChannelIndex, ExpressionController, value);
    }

    // Everything added so far, ready to play. You can call this again after
    // adding more; each call makes a new sequence and leaves the builder alone.
    return builder.GetSequence();
}

MidiSequence BuildRealTimeSequence(MidiSequenceBuilder const& builder)
{
    // Clear empties the builder so it can be used again. The sequence it made
    // earlier is a separate object and is not affected.
    builder.Clear();

    // Now a tick is a microsecond, and tempo does not apply.
    builder.TimingMode(MidiSequenceTimingMode::Absolute);

    auto const guitar = builder.AddTrack(L"Guitar");

    // General MIDI System On resets every channel on the device, so music written
    // for General MIDI often starts with it. System exclusive goes in whole, from
    // the F0 to the F7.
    std::array<uint8_t, 6> const generalMidiSystemOn{ 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

    builder.AddSystemExclusive(guitar, 0, generalMidiSystemOn);

    // Many devices do not listen while they reset. Give them 200 milliseconds,
    // which is 200 milliseconds whatever the tempo.
    constexpr uint32_t ResetPauseMicroseconds = 200'000;

    AddChannelVoiceMessage(builder, guitar, ResetPauseMicroseconds, Midi1ChannelVoiceMessageStatus::ProgramChange, GuitarChannelIndex, NylonStringGuitar, 0);

    // A strum happens in real time too: one string every 25 milliseconds, low
    // to high, all of them ringing until the same moment.
    std::array<uint8_t, 6> const eMajor{ 40, 47, 52, 56, 59, 64 };

    constexpr uint32_t StrumGapMicroseconds = 25'000;
    constexpr uint32_t RingMicroseconds = 2'500'000;

    for (uint32_t index = 0; index < eMajor.size(); index++)
    {
        builder.AddNote(
            ResetPauseMicroseconds + index * StrumGapMicroseconds,
            RingMicroseconds - index * StrumGapMicroseconds,
            guitar,
            MidiChannel{ GuitarChannelIndex },
            eMajor[index],
            90);
    }

    return builder.GetSequence();
}


void PrintSequence(MidiSequence const& sequence)
{
    // Input the builder cannot use is left out rather than thrown. If you need
    // to be sure everything went in, compare these counts with what you added.
    std::wcout << L"  Tracks: " << sequence.Tracks().Size()
        << L", notes: " << sequence.NoteCount()
        << L", events: " << sequence.EventCount() << std::endl;

    std::wcout << L"  Plays for " << std::fixed << std::setprecision(1)
        << sequence.DurationMicroseconds() / 1'000'000.0 << L" seconds" << std::endl;

    for (auto const& track : sequence.Tracks())
    {
        std::wcout << L"  Track " << track.TrackIndex() + 1 << L": "
            << track.Name().c_str() << L", " << track.NoteCount() << L" notes" << std::endl;
    }
}

void PrintTempoMap(MidiSequence const& sequence)
{
    // The tempo map is how ticks become time. It is worked out once, when the
    // sequence is made, so converting between the two never walks the sequence.
    std::wcout << L"  Tempo:" << std::endl;

    for (auto const& change : sequence.TempoMap())
    {
        auto const position = sequence.GetBarPositionAtTick(change.Tick);

        std::wcout << L"    bar " << position.Bar << L" beat " << position.Beat
            << L", " << change.MicrosecondsAtTick / 1'000'000.0 << L" s in: "
            << change.BeatsPerMinute << L" BPM" << std::endl;
    }
}

void PlayToEnd(MidiSequencePlayer const& player, MidiSequence const& sequence)
{
    // Preparing converts the whole sequence once, so this is the slow call and
    // Play is not.
    player.SetSequenceAsync(sequence).get();

    player.Play();

    // The player hands each message to the service ahead of time, with the time
    // it should go out, and the service sends it then. There is nothing to do
    // here but wait.
    while (player.State() == MidiSequencePlayerState::Playing)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    // The synthesizer has exactly one endpoint, so it can be named without
    // enumerating. The id is empty when the synthesizer is switched off.
    winrt::hstring endpointId = DestinationEndpointId;

    if (endpointId.empty())
    {
        endpointId = MidiSynthManager::EndpointDeviceId();
    }

    if (endpointId.empty())
    {
        std::wcout << L"The General MIDI synthesizer is not available. It may be switched off in MIDI Settings." << std::endl;
        return 1;
    }

    MidiSequenceBuilder builder;

    auto const song = BuildMusicalSequence(builder);

    std::wcout << L"Built a sequence that counts in beats:" << std::endl;
    PrintSequence(song);
    PrintTempoMap(song);

    auto session = MidiSession::Create(L"Sequence Builder Sample");

    auto connection = session.CreateEndpointConnection(endpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << endpointId.c_str() << std::endl;
        return 1;
    }

    // The player borrows the connection rather than opening one of its own, and
    // never closes it. One connection can serve a player and anything else your
    // application sends.
    MidiSequencePlayer player{ connection, MidiGroup{ static_cast<uint8_t>(0) } };

    std::wcout << std::endl << L"Playing..." << std::endl;
    PlayToEnd(player, song);

    auto const strum = BuildRealTimeSequence(builder);

    std::wcout << std::endl << L"Built a sequence that counts in microseconds:" << std::endl;
    PrintSequence(strum);

    // A player can take one sequence after another.
    std::wcout << std::endl << L"Playing..." << std::endl;
    PlayToEnd(player, strum);

    std::wcout << std::endl << L"Done." << std::endl;

    player.Close();
    session.Close();

    return 0;
}
