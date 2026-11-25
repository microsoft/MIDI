// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "TrackSoundTests.h"
#include "SequenceTestData.h"

#include "MessageTranslation.h"
#include "SequenceSerializer.h"
#include "TrackSound.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    ClipEvent Midi1(uint32_t status, uint32_t channel, uint32_t data1, uint32_t data2)
    {
        ClipEvent message{};
        message.Words[0] = 0x20000000u | (status << 20) | (channel << 16) | (data1 << 8) | data2;
        message.WordCount = 1;
        return message;
    }

    ClipEvent Volume2(uint32_t channel, uint32_t value)
    {
        ClipEvent message{};
        message.Words[0] = 0x40000000u | (0xBu << 20) | (channel << 16) | (7u << 8);
        message.Words[1] = value;
        message.WordCount = 2;
        return message;
    }

    TrackSound Sound(uint8_t program, bool hasBank = false, uint8_t msb = 0, uint8_t lsb = 0)
    {
        TrackSound sound{};
        sound.Set = true;
        sound.Program = program;
        sound.HasBank = hasBank;
        sound.BankMsb = msb;
        sound.BankLsb = lsb;
        return sound;
    }
}

void TrackSoundTests::NoProgramChangeMeansNoSound()
{
    VERIFY_IS_FALSE(StartupSound({}).Set);

    // A bank select on its own changes nothing on the device.
    VERIFY_IS_FALSE(StartupSound({ Midi1(0xB, 0, 0, 121), Midi1(0xB, 0, 7, 100) }).Set);
}

void TrackSoundTests::AMidi1ProgramTakesTheBankBeforeIt()
{
    auto const sound = StartupSound({ Midi1(0xB, 2, 0, 121), Midi1(0xB, 2, 32, 1), Midi1(0xC, 2, 48, 0), Midi1(0xB, 2, 7, 100) });

    VERIFY_IS_TRUE(sound == Sound(48, true, 121, 1));

    // Without a bank select, it has no bank.
    VERIFY_IS_TRUE(StartupSound({ Midi1(0xC, 0, 80, 0) }) == Sound(80));
}

void TrackSoundTests::AMidi2ProgramCarriesItsOwnBank()
{
    auto const message = SoundMessage(Sound(49, true, 121, 2), 3);

    VERIFY_ARE_EQUAL(uint8_t{ 2 }, message.WordCount);
    VERIFY_ARE_EQUAL(0x40C30001u, message.Words[0]);
    VERIFY_ARE_EQUAL(0x31007902u, message.Words[1]);
    VERIFY_IS_TRUE(StartupSound({ Midi1(0xB, 3, 0, 5), message }) == Sound(49, true, 121, 2));

    // Its bank flag off, the controllers before it still choose the bank.
    auto const noBank = SoundMessage(Sound(10), 3);
    VERIFY_ARE_EQUAL(0x40C30000u, noBank.Words[0]);
    VERIFY_IS_TRUE(StartupSound({ Midi1(0xB, 3, 0, 5), noBank }) == Sound(10, true, 5, 0));
}

void TrackSoundTests::TheLastProgramChangeWins()
{
    auto const sound = StartupSound({ Midi1(0xB, 0, 0, 1), Midi1(0xC, 0, 4, 0), SoundMessage(Sound(9), 0) });

    VERIFY_IS_TRUE(sound == Sound(9, true, 1, 0));
}

void TrackSoundTests::ASoundGoesFirstAndReplacesTheOldOne()
{
    std::vector<ClipEvent> startup{ Midi1(0xB, 0, 7, 100), Midi1(0xB, 0, 0, 121), Midi1(0xB, 0, 32, 1), Midi1(0xC, 0, 48, 0), Volume2(0, 0x80000000u) };

    SetStartupSound(startup, Sound(81, true, 0, 3), 5);

    VERIFY_ARE_EQUAL(size_t{ 3 }, startup.size());
    VERIFY_IS_TRUE(startup[0] == SoundMessage(Sound(81, true, 0, 3), 5));
    VERIFY_IS_TRUE(startup[1] == Midi1(0xB, 0, 7, 100));
    VERIFY_IS_TRUE(startup[2] == Volume2(0, 0x80000000u));
    VERIFY_IS_TRUE(StartupSound(startup) == Sound(81, true, 0, 3));
}

void TrackSoundTests::ClearingTheSoundKeepsTheRest()
{
    std::vector<ClipEvent> startup{ SoundMessage(Sound(81), 0), Midi1(0xB, 0, 10, 64), Midi1(0xB, 0, 0, 2) };

    SetStartupSound(startup, TrackSound{}, 0);

    VERIFY_ARE_EQUAL(size_t{ 1 }, startup.size());
    VERIFY_IS_TRUE(startup[0] == Midi1(0xB, 0, 10, 64));
    VERIFY_IS_FALSE(StartupSound(startup).Set);
    VERIFY_IS_FALSE(IsSoundMessage(startup[0]));
}

void TrackSoundTests::AMidi1DeviceGetsBankSelectThenProgram()
{
    auto const message = SoundMessage(Sound(49, true, 121, 2), 9);
    auto const translated = TranslateToMidi1(message.Words.data(), message.WordCount);

    VERIFY_ARE_EQUAL(uint8_t{ 3 }, translated.Count);
    VERIFY_ARE_EQUAL(0x20B90079u, translated.Messages[0][0]);
    VERIFY_ARE_EQUAL(0x20B92002u, translated.Messages[1][0]);
    VERIFY_ARE_EQUAL(0x20C93100u, translated.Messages[2][0]);
}

void TrackSoundTests::TheSoundNameIsKeptInTheFile()
{
    auto original = testdata::SampleSequence();
    auto* bass = FindTrack(original, L"t-bass");
    VERIFY_IS_NOT_NULL(bass);

    bass->SoundName = L"Warm Analog Bass";
    SetStartupSound(bass->Startup, Sound(38, true, 2, 0), 1);

    auto const text = WriteSequenceJson(original);
    VERIFY_IS_TRUE(text.find(L"\"soundName\": \"Warm Analog Bass\"") != std::wstring::npos);

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());

    auto const readBass = FindTrack(read, L"t-bass");
    VERIFY_IS_NOT_NULL(readBass);
    VERIFY_ARE_EQUAL(std::wstring{ L"Warm Analog Bass" }, readBass->SoundName);
    VERIFY_IS_TRUE(StartupSound(readBass->Startup) == Sound(38, true, 2, 0));
    VERIFY_IS_TRUE(readBass->Unknown == nullptr || !readBass->Unknown.HasKey(L"soundName"));

    // A track without one writes nothing for it.
    auto const pad = FindTrack(read, L"t-pad");
    VERIFY_IS_NOT_NULL(pad);
    VERIFY_IS_TRUE(pad->SoundName.empty());
}
