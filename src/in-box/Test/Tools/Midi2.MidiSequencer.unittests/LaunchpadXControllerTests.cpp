// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXControllerTests.h"

#include "LaunchpadXController.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;
using namespace midisequencer::launchpad;

namespace
{
    // Stands in for the device: keeps every packet, and can be told to fail.
    class RecordingOutput final : public ILaunchpadOutput
    {
    public:
        bool SendUmpPackets(_In_ std::span<const UmpPacket> packets) noexcept override
        {
            if (Fail)
            {
                return false;
            }

            Packets.insert(Packets.end(), packets.begin(), packets.end());
            return true;
        }

        // Each SysEx message sent, without F0 and F7.
        std::vector<std::vector<uint8_t>> SysEx() const
        {
            std::vector<std::vector<uint8_t>> messages{};
            SysEx7Accumulator accumulator{};

            for (auto const& packet : Packets)
            {
                uint8_t group = 0;
                std::vector<uint8_t> payload{};

                if (packet.wordCount == 2 && accumulator.Feed(packet.words[0], packet.words[1], group, payload))
                {
                    messages.push_back(payload);
                }
            }

            return messages;
        }

        std::vector<UmpPacket> Packets{};
        bool Fail{ false };
    };

    struct Recorded
    {
        SurfaceAction Action{};
        uint64_t Timestamp{ 0 };
    };

    struct Fixture
    {
        std::shared_ptr<RecordingOutput> Output{ std::make_shared<RecordingOutput>() };
        std::vector<Recorded> Notes{};
        std::vector<std::vector<SurfaceAction>> Actions{};
        LaunchpadXController Controller;

        explicit Fixture(_In_ uint8_t group = 1) :
            Controller(
                Output,
                group,
                [this](SurfaceAction const& action, uint64_t timestamp) { Notes.push_back({ action, timestamp }); },
                [this](std::vector<SurfaceAction> actions) { Actions.push_back(std::move(actions)); })
        {
        }

        void Send(_In_ uint32_t word, _In_ uint64_t timestamp = 1000)
        {
            Controller.OnInput(&word, 1, timestamp);
        }
    };

    uint32_t Midi1(_In_ uint8_t group, _In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2) noexcept
    {
        return 0x20000000u |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status & 0x0F) << 20) |
            (static_cast<uint32_t>(data1 & 0x7F) << 8) |
            static_cast<uint32_t>(data2 & 0x7F);
    }

    SurfaceStateSnapshot TwoByTwo()
    {
        SurfaceStateSnapshot snapshot{};
        snapshot.totalTracks = 2;
        snapshot.totalScenes = 2;
        snapshot.gridClips.resize(4);
        snapshot.gridClips[0].hasClip = true;
        return snapshot;
    }

    bool IsLedMessage(_In_ std::vector<uint8_t> const& payload) noexcept
    {
        return payload.size() > 6 && payload[5] == SysExCommandLedLighting;
    }

    // How many LEDs an LED message sets. Each entry's length depends on its type.
    size_t CountLeds(_In_ std::vector<uint8_t> const& payload) noexcept
    {
        if (!IsLedMessage(payload))
        {
            return 0;
        }

        size_t count = 0;

        for (size_t i = 6; i < payload.size();)
        {
            switch (payload[i])
            {
            case 0x00: i += 3; break;
            case 0x01: i += 4; break;
            case 0x02: i += 3; break;
            case 0x03: i += 5; break;
            default: return 0;
            }

            ++count;
        }

        return count;
    }

    size_t CountLeds(_In_ std::vector<std::vector<uint8_t>> const& messages) noexcept
    {
        size_t count = 0;

        for (auto const& message : messages)
        {
            count += CountLeds(message);
        }

        return count;
    }

    EndpointCandidate Launchpad(_In_ std::wstring id, _In_ uint16_t vendor = NovationUsbVendorId, _In_ uint16_t product = LaunchpadXUsbProductId)
    {
        EndpointCandidate candidate{};
        candidate.EndpointId = std::move(id);
        candidate.Name = L"Launchpad X";
        candidate.UsbVendorId = vendor;
        candidate.UsbProductId = product;

        for (size_t group = 0; group < 2; ++group)
        {
            candidate.SourceGroups[group] = true;
            candidate.DestinationGroups[group] = true;
        }

        return candidate;
    }
}

void LaunchpadXControllerTests::FindsTheMidiPortOfOneEndpointWithBothPorts()
{
    auto candidate = Launchpad(L"lpx");
    candidate.SourcePortNames[0] = L"LPX DAW";
    candidate.DestinationPortNames[0] = L"LPX DAW";
    candidate.SourcePortNames[1] = L"LPX MIDI";
    candidate.DestinationPortNames[1] = L"LPX MIDI";

    std::array const candidates{ candidate };
    auto const port = FindLaunchpadPort(candidates);
    VERIFY_IS_TRUE(port.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"lpx" }, port->EndpointId);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, port->Group);

    // The port names can be the other way round; the name decides, not the order.
    std::swap(candidate.SourcePortNames[0], candidate.SourcePortNames[1]);
    std::swap(candidate.DestinationPortNames[0], candidate.DestinationPortNames[1]);
    std::array const swapped{ candidate };
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, FindLaunchpadPort(swapped)->Group);

    // A block name says it too.
    auto named = Launchpad(L"blocks", 0, 0);
    named.Name = L"Some device";
    named.SourceGroupNames[1] = L"LPX MIDI";
    std::array const blocks{ named };
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, FindLaunchpadPort(blocks)->Group);
}

void LaunchpadXControllerTests::FindsTheMidiPortWhenEachPortIsItsOwnEndpoint()
{
    EndpointCandidate daw{};
    daw.EndpointId = L"daw";
    daw.Name = L"LPX DAW";
    daw.SourceGroups[0] = daw.DestinationGroups[0] = true;

    EndpointCandidate midi{};
    midi.EndpointId = L"midi";
    midi.Name = L"MIDIIN2 (LPX MIDI)";
    midi.SourceGroups[0] = midi.DestinationGroups[0] = true;

    std::array const candidates{ daw, midi };
    auto const port = FindLaunchpadPort(candidates);
    VERIFY_IS_TRUE(port.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"midi" }, port->EndpointId);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, port->Group);
}

void LaunchpadXControllerTests::FindsTheSecondPortOfAnUnnamedLaunchpadByUsbIds()
{
    auto candidate = Launchpad(L"lpx");
    candidate.Name = L"USB device";
    std::array const candidates{ candidate };

    auto const port = FindLaunchpadPort(candidates);
    VERIFY_IS_TRUE(port.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, port->Group);

    // With the DAW port named, the other one is the MIDI port, wherever it is.
    candidate.SourceGroups[2] = candidate.DestinationGroups[2] = true;
    candidate.SourcePortNames[0] = L"LPX DAW";
    std::array const named{ candidate };
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, FindLaunchpadPort(named)->Group);
}

void LaunchpadXControllerTests::NeverChoosesTheDawPortOrAnotherDevice()
{
    EndpointCandidate daw{};
    daw.EndpointId = L"daw";
    daw.Name = L"LPX DAW";
    daw.SourceGroups[0] = daw.DestinationGroups[0] = true;

    auto other = Launchpad(L"other", NovationUsbVendorId, 0x0113);
    other.Name = L"Launchpad Mini MK3";

    auto outputOnly = Launchpad(L"output-only");
    outputOnly.SourceGroups = {};

    std::array const candidates{ daw, other, outputOnly };
    VERIFY_IS_FALSE(FindLaunchpadPort(candidates).has_value());
    VERIFY_IS_FALSE(FindLaunchpadPort(std::span<const EndpointCandidate>{}).has_value());
}

void LaunchpadXControllerTests::PrefersThePickedLaunchpadWhenThereAreTwo()
{
    auto first = Launchpad(L"first");
    first.SourcePortNames[1] = L"LPX MIDI";
    auto second = Launchpad(L"second");

    std::array const candidates{ first, second };
    VERIFY_ARE_EQUAL(std::wstring{ L"first" }, FindLaunchpadPort(candidates)->EndpointId);
    VERIFY_ARE_EQUAL(std::wstring{ L"second" }, FindLaunchpadPort(candidates, L"second")->EndpointId);

    // A picked one that isn't here falls back to the best one that is.
    VERIFY_ARE_EQUAL(std::wstring{ L"first" }, FindLaunchpadPort(candidates, L"gone")->EndpointId);
}

void LaunchpadXControllerTests::StartEntersProgrammerModeThenPaintsEveryLed()
{
    Fixture f{ 1 };
    VERIFY_IS_FALSE(f.Controller.IsStarted());
    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));
    VERIFY_IS_TRUE(f.Controller.IsStarted());

    for (auto const& packet : f.Output->Packets)
    {
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, (packet.words[0] >> 24) & 0x0Fu);
    }

    auto const messages = f.Output->SysEx();
    VERIFY_IS_TRUE(messages.size() >= 2);
    VERIFY_IS_TRUE(messages[0] == BuildProgrammerModeSysEx(true));

    for (size_t i = 1; i < messages.size(); ++i)
    {
        VERIFY_IS_TRUE(IsLedMessage(messages[i]));
    }

    VERIFY_ARE_EQUAL(size_t{ TotalLedCount }, CountLeds(messages));

    // Nothing changed, so nothing more is sent.
    f.Output->Packets.clear();
    f.Controller.Update(TwoByTwo());
    VERIFY_IS_TRUE(f.Output->Packets.empty());
}

void LaunchpadXControllerTests::IgnoresInputBeforeStartAndOnOtherGroups()
{
    Fixture f{ 1 };
    f.Send(Midi1(1, 0x9, 81, 100));
    VERIFY_IS_TRUE(f.Actions.empty());

    // And no LEDs before it's started.
    f.Controller.Update(TwoByTwo());
    VERIFY_IS_TRUE(f.Output->Packets.empty());

    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));
    f.Send(Midi1(0, 0x9, 81, 100));
    VERIFY_IS_TRUE(f.Actions.empty());

    f.Send(Midi1(1, 0x9, 81, 100));
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Actions.size());

    f.Controller.OnInput(nullptr, 1, 0);
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Actions.size());
}

void LaunchpadXControllerTests::ClipPadsGoToTheAppAndNotesPlayAtOnce()
{
    Fixture f{ 0 };
    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));

    // Pad 81 is the top-left pad: track 0, scene 0.
    f.Send(Midi1(0, 0x9, 81, 100), 5000);
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Actions.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Actions[0].size());
    VERIFY_IS_TRUE(f.Actions[0][0].kind == SurfaceActionKind::TriggerOrStopSlot);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, f.Actions[0][0].trackIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, f.Actions[0][0].sceneIndex);
    VERIFY_IS_TRUE(f.Notes.empty());

    f.Controller.SetMode(ControllerMode::Note);
    VERIFY_IS_TRUE(f.Controller.Mode() == ControllerMode::Note);
    f.Actions.clear();

    f.Send(Midi1(0, 0x9, 11, 90), 6000);
    f.Send(Midi1(0, 0xA, 11, 70), 6100);
    f.Send(Midi1(0, 0x9, 11, 0), 6200);

    VERIFY_IS_TRUE(f.Actions.empty());
    VERIFY_ARE_EQUAL(size_t{ 3 }, f.Notes.size());
    VERIFY_IS_TRUE(f.Notes[0].Action.kind == SurfaceActionKind::EmitNoteOn);
    VERIFY_ARE_EQUAL(uint64_t{ 6000 }, f.Notes[0].Timestamp);
    VERIFY_ARE_EQUAL(uint8_t{ 90 }, f.Notes[0].Action.velocity);
    VERIFY_IS_TRUE(f.Notes[1].Action.kind == SurfaceActionKind::EmitPolyAftertouch);
    VERIFY_IS_TRUE(f.Notes[2].Action.kind == SurfaceActionKind::EmitNoteOff);
    VERIFY_ARE_EQUAL(f.Notes[0].Action.midiNote, f.Notes[2].Action.midiNote);
}

void LaunchpadXControllerTests::SwitchingModeWhileHoldingPadsLetsGoOfEveryNote()
{
    Fixture f{ 0 };
    f.Controller.SetMode(ControllerMode::Note);
    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));

    f.Send(Midi1(0, 0x9, 11, 100));
    f.Send(Midi1(0, 0x9, 23, 100));
    VERIFY_ARE_EQUAL(size_t{ 2 }, f.Notes.size());
    f.Notes.clear();

    // Session: back to Clip mode. Both notes stop before the app hears about the switch.
    f.Send(Midi1(0, 0xB, ButtonSession, 127));
    VERIFY_ARE_EQUAL(size_t{ 2 }, f.Notes.size());
    VERIFY_IS_TRUE(std::all_of(f.Notes.begin(), f.Notes.end(), [](Recorded const& r) { return r.Action.kind == SurfaceActionKind::EmitNoteOff; }));
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Actions.size());
    VERIFY_IS_TRUE(f.Actions[0][0].kind == SurfaceActionKind::SwitchMode);
    VERIFY_IS_TRUE(f.Actions[0][0].targetMode == ControllerMode::Clip);

    // Lifting those fingers in Clip mode does nothing.
    f.Notes.clear();
    f.Actions.clear();
    f.Send(Midi1(0, 0x9, 11, 0));
    f.Send(Midi1(0, 0x9, 23, 0));
    VERIFY_IS_TRUE(f.Notes.empty());
    VERIFY_IS_TRUE(f.Actions.empty());

    // A layout change from the app lets go too.
    f.Controller.SetMode(ControllerMode::Note);
    f.Send(Midi1(0, 0x9, 11, 100));
    f.Notes.clear();
    auto config = f.Controller.NoteConfig();
    config.rootNote = 7;
    f.Controller.SetNoteConfig(config);
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Notes.size());
    VERIFY_IS_TRUE(f.Notes[0].Action.kind == SurfaceActionKind::EmitNoteOff);
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, f.Notes[0].Timestamp);
}

void LaunchpadXControllerTests::StopLetsGoOfNotesAndLeavesProgrammerModeOnce()
{
    Fixture f{ 0 };
    f.Controller.SetMode(ControllerMode::Note);
    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));
    f.Send(Midi1(0, 0x9, 11, 100));
    f.Notes.clear();
    f.Output->Packets.clear();

    f.Controller.Stop();
    VERIFY_IS_FALSE(f.Controller.IsStarted());
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Notes.size());
    VERIFY_IS_TRUE(f.Notes[0].Action.kind == SurfaceActionKind::EmitNoteOff);

    auto const messages = f.Output->SysEx();
    VERIFY_ARE_EQUAL(size_t{ 1 }, messages.size());
    VERIFY_IS_TRUE(messages[0] == BuildProgrammerModeSysEx(false));

    // Stopped already: nothing more goes to the device, and pads do nothing.
    f.Output->Packets.clear();
    f.Controller.Stop();
    f.Send(Midi1(0, 0x9, 11, 100));
    VERIFY_IS_TRUE(f.Output->Packets.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, f.Notes.size());

    // Started again, it repaints everything.
    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));
    VERIFY_ARE_EQUAL(size_t{ TotalLedCount }, CountLeds(f.Output->SysEx()));
}

void LaunchpadXControllerTests::AFailedLedUpdateRepaintsEverythingNextTime()
{
    Fixture f{ 0 };
    VERIFY_IS_TRUE(f.Controller.Start(TwoByTwo()));

    auto changed = TwoByTwo();
    changed.gridClips[0].playing = true;

    f.Output->Fail = true;
    f.Controller.Update(changed);
    f.Output->Fail = false;
    f.Output->Packets.clear();

    // Only one pad changed, but what the device shows is unknown now.
    f.Controller.Update(changed);
    VERIFY_ARE_EQUAL(size_t{ TotalLedCount }, CountLeds(f.Output->SysEx()));

    // A device that won't take Programmer mode doesn't start.
    Fixture failing{ 0 };
    failing.Output->Fail = true;
    VERIFY_IS_FALSE(failing.Controller.Start(TwoByTwo()));
    VERIFY_IS_FALSE(failing.Controller.IsStarted());
}

void LaunchpadXControllerTests::BuildNoteWordsMakesMidi2NotesAndPressure()
{
    uint32_t words[2]{};

    SurfaceAction on{};
    on.kind = SurfaceActionKind::EmitNoteOn;
    on.midiNote = 60;
    on.velocity = 127;
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, LaunchpadXController::BuildNoteWords(on, words));
    VERIFY_ARE_EQUAL(uint32_t{ 0x40903C00u }, words[0]);
    VERIFY_ARE_EQUAL(uint32_t{ 0xFFFFu }, words[1] >> 16);

    // A velocity of 0 would be a note-off to a MIDI 1.0 device, so the quietest note-on is 1.
    on.velocity = 0;
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, LaunchpadXController::BuildNoteWords(on, words));
    VERIFY_ARE_NOT_EQUAL(uint32_t{ 0 }, words[1] >> 16);

    SurfaceAction off{};
    off.kind = SurfaceActionKind::EmitNoteOff;
    off.midiNote = 60;
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, LaunchpadXController::BuildNoteWords(off, words));
    VERIFY_ARE_EQUAL(uint32_t{ 0x40803C00u }, words[0]);

    SurfaceAction pressure{};
    pressure.kind = SurfaceActionKind::EmitPolyAftertouch;
    pressure.midiNote = 60;
    pressure.velocity = 127;
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, LaunchpadXController::BuildNoteWords(pressure, words));
    VERIFY_ARE_EQUAL(uint32_t{ 0x40A03C00u }, words[0]);
    VERIFY_ARE_EQUAL(uint32_t{ 0xFFFFFFFFu }, words[1]);

    SurfaceAction other{};
    other.kind = SurfaceActionKind::LaunchScene;
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, LaunchpadXController::BuildNoteWords(other, words));
}
