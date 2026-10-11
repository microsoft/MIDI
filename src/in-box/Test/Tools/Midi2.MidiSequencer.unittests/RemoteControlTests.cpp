// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RemoteControlTests.h"

#include "GlassLayoutWriter.h"
#include "RemoteControl.h"
#include "SequenceTestData.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <set>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    uint32_t Midi1(uint8_t group, uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2)
    {
        return 0x20000000u |
            (static_cast<uint32_t>(group) << 24) |
            (static_cast<uint32_t>(status) << 20) |
            (static_cast<uint32_t>(channel) << 16) |
            (static_cast<uint32_t>(data1) << 8) |
            data2;
    }

    std::optional<RemoteCommand> Parse(uint32_t word)
    {
        return ParseRemoteMessage(&word, 1);
    }

    bool Contains(std::vector<uint32_t> const& messages, uint32_t word)
    {
        return std::find(messages.begin(), messages.end(), word) != messages.end();
    }

    Track MakeTrack(std::wstring const& id, bool folder = false)
    {
        Track track{};
        track.Id = id;
        track.IsFolder = folder;
        return track;
    }

    GlassLayoutText LayoutText()
    {
        GlassLayoutText text{};
        text.Name = L"Night Drive Sketch remote";
        text.Description = L"Launch clips and scenes";
        text.EndpointName = L"MIDI Sequencer";
        text.Tool = L"Windows MIDI Sequencer";
        text.Id = L"6f1c7d0e-9b8a-4c6d-8e5f-3a2b1c0d9e8f";
        text.Created = L"2026-10-10T21:14:00Z";
        text.Play = L"Play";
        text.Stop = L"Stop";
        text.Record = L"Record";
        text.Loop = L"Loop";
        text.Metronome = L"Click";
        text.GoToStart = L"Start";
        text.StopAll = L"Stop all";
        text.BackToTimeline = L"Back to timeline";
        text.Mute = L"Mute";
        text.Solo = L"Solo";
        text.Arm = L"Arm";
        text.StopTrack = L"Stop";
        text.EmptySlot = L"Empty";
        text.SceneName = [](size_t scene) { return L"Scene " + std::to_wstring(scene + 1); };
        text.TrackName = [](size_t track) { return L"Track " + std::to_wstring(track + 1); };
        text.PageName = [](size_t firstTrack, size_t lastTrack, size_t, size_t) { return L"Tracks " + std::to_wstring(firstTrack + 1) + L" to " + std::to_wstring(lastTrack + 1); };
        return text;
    }

    // The checks the layout guide's own script makes, for the kinds of control this layout uses.
    // Returns how many pages there are besides the band.
    size_t CheckLayout(std::wstring const& text, std::wstring const& endpointName)
    {
        namespace mjson = winrt::Windows::Data::Json;

        mjson::JsonObject layout{ nullptr };
        VERIFY_IS_TRUE(mjson::JsonObject::TryParse(text, layout));
        VERIFY_ARE_EQUAL(std::wstring{ L"Studio Dark" }, std::wstring{ layout.GetNamedString(L"theme") });

        auto const devices = layout.GetNamedArray(L"devices");
        VERIFY_ARE_EQUAL(1u, devices.Size());
        auto const device = devices.GetObjectAt(0);
        VERIFY_ARE_EQUAL(std::wstring{ L"Sequencer" }, std::wstring{ device.GetNamedString(L"name") });
        VERIFY_ARE_EQUAL(endpointName, std::wstring{ device.GetNamedObject(L"match").GetNamedString(L"transportSuppliedEndpointName") });

        auto const provenance = layout.GetNamedObject(L"provenance");
        VERIFY_ARE_EQUAL(std::wstring{ L"algorithmicMedia" }, std::wstring{ provenance.GetNamedString(L"digitalSourceType") });

        std::set<std::wstring> pageIds{};
        std::wstring bandId{};
        auto const pages = layout.GetNamedArray(L"pages");

        for (auto const& value : pages)
        {
            auto const page = value.GetObject();
            VERIFY_IS_TRUE(pageIds.insert(std::wstring{ page.GetNamedString(L"id") }).second);

            if (page.GetNamedBoolean(L"sharedBand", false))
            {
                bandId = page.GetNamedString(L"id");
            }
        }

        std::set<std::wstring> controlIds{};
        std::set<std::wstring> const kinds{ L"button", L"toggle", L"pad", L"pageTab", L"label" };

        for (auto const& value : pages)
        {
            for (auto const& item : value.GetObject().GetNamedArray(L"controls"))
            {
                auto const control = item.GetObject();
                VERIFY_IS_TRUE(controlIds.insert(std::wstring{ control.GetNamedString(L"id") }).second);
                VERIFY_IS_TRUE(kinds.contains(std::wstring{ control.GetNamedString(L"kind") }));
                VERIFY_IS_FALSE(control.GetNamedString(L"label").empty());

                auto const x = control.GetNamedNumber(L"x");
                auto const y = control.GetNamedNumber(L"y");
                VERIFY_IS_TRUE(x >= 0 && y >= 0 && x + control.GetNamedNumber(L"width") <= 1280 && y + control.GetNamedNumber(L"height") <= 800);

                auto const hue = control.GetNamedNumber(L"hueSlot");
                VERIFY_IS_TRUE(hue >= 0 && hue <= 6);

                for (auto const& row : control.GetNamedArray(L"messages", mjson::JsonArray{}))
                {
                    auto const message = row.GetObject();
                    std::wstring const kind{ message.GetNamedString(L"kind") };
                    std::wstring const trigger{ message.GetNamedString(L"trigger") };
                    VERIFY_IS_TRUE(trigger == L"turnsOn" || trigger == L"turnsOff");

                    if (kind == L"goToPage")
                    {
                        std::wstring const target{ message.GetNamedString(L"targetPage") };
                        VERIFY_IS_TRUE(pageIds.contains(target));
                        VERIFY_ARE_NOT_EQUAL(bandId, target);
                        continue;
                    }

                    VERIFY_IS_TRUE(kind == L"note" || kind == L"controlChange");
                    VERIFY_ARE_EQUAL(std::wstring{ L"Sequencer" }, std::wstring{ message.GetNamedString(L"device") });
                    VERIFY_IS_TRUE(message.GetNamedNumber(L"group") <= 15 && message.GetNamedNumber(L"channel") <= 15 && message.GetNamedNumber(L"number") <= 127);
                }

                if (control.HasKey(L"feedback"))
                {
                    VERIFY_ARE_EQUAL(std::wstring{ L"Sequencer" }, std::wstring{ control.GetNamedObject(L"feedback").GetNamedString(L"device") });
                }
            }
        }

        VERIFY_IS_FALSE(bandId.empty());
        return pages.Size() - 1;
    }

    size_t CountControls(std::wstring const& text, std::wstring const& idPart)
    {
        size_t count{ 0 };

        for (auto const& page : winrt::Windows::Data::Json::JsonObject::Parse(text).GetNamedArray(L"pages"))
        {
            for (auto const& control : page.GetObject().GetNamedArray(L"controls"))
            {
                if (std::wstring{ control.GetObject().GetNamedString(L"id") }.find(idPart) != std::wstring::npos)
                {
                    ++count;
                }
            }
        }

        return count;
    }
}

void RemoteControlTests::NotesLaunchClipsAndScenes()
{
    // Channel 3 is track 3; note 4 is scene 5.
    auto const slot = Parse(Midi1(0, 0x9, 2, 4, 100));
    VERIFY_IS_TRUE(slot.has_value());
    VERIFY_IS_TRUE(slot->Action == RemoteAction::LaunchSlot);
    VERIFY_ARE_EQUAL(size_t{ 2 }, slot->Track);
    VERIFY_ARE_EQUAL(size_t{ 4 }, slot->Scene);

    // Group 2, channel 1 is track 16.
    auto const later = Parse(Midi1(1, 0x9, 0, 0, 100));
    VERIFY_IS_TRUE(later.has_value());
    VERIFY_ARE_EQUAL(size_t{ 15 }, later->Track);

    auto const stop = Parse(Midi1(0, 0x9, 0, RemoteNoteStop, 1));
    VERIFY_IS_TRUE(stop.has_value() && stop->Action == RemoteAction::StopTrack);

    auto const back = Parse(Midi1(0, 0x9, 0, RemoteNoteToTimeline, 1));
    VERIFY_IS_TRUE(back.has_value() && back->Action == RemoteAction::TrackToTimeline);

    auto const scene = Parse(Midi1(0, 0x9, RemoteSequenceChannel, 2, 127));
    VERIFY_IS_TRUE(scene.has_value() && scene->Action == RemoteAction::LaunchScene);
    VERIFY_ARE_EQUAL(size_t{ 2 }, scene->Scene);
}

void RemoteControlTests::ControllersSetMuteSoloAndArm()
{
    auto const mute = Parse(Midi1(0, 0xB, 1, RemoteControllerMute, 127));
    VERIFY_IS_TRUE(mute.has_value() && mute->Action == RemoteAction::Mute);
    VERIFY_ARE_EQUAL(size_t{ 1 }, mute->Track);
    VERIFY_IS_TRUE(mute->On);

    auto const solo = Parse(Midi1(0, 0xB, 1, RemoteControllerSolo, 63));
    VERIFY_IS_TRUE(solo.has_value() && solo->Action == RemoteAction::Solo);
    VERIFY_IS_FALSE(solo->On);

    auto const arm = Parse(Midi1(0, 0xB, 1, RemoteControllerArm, 64));
    VERIFY_IS_TRUE(arm.has_value() && arm->Action == RemoteAction::Arm);
    VERIFY_IS_TRUE(arm->On);

    // Other controllers are the track's own business.
    VERIFY_IS_FALSE(Parse(Midi1(0, 0xB, 1, 7, 127)).has_value());
}

void RemoteControlTests::TheTransportIsOnChannel16()
{
    std::array<std::pair<uint8_t, RemoteAction>, 8> const notes{ {
        { RemoteNotePlay, RemoteAction::Play },
        { RemoteNoteStopTransport, RemoteAction::Stop },
        { RemoteNoteRecord, RemoteAction::Record },
        { RemoteNoteLoop, RemoteAction::Loop },
        { RemoteNoteMetronome, RemoteAction::Metronome },
        { RemoteNoteGoToStart, RemoteAction::GoToStart },
        { RemoteNoteStop, RemoteAction::StopAll },
        { RemoteNoteToTimeline, RemoteAction::AllToTimeline },
    } };

    for (auto const& [note, action] : notes)
    {
        auto const command = Parse(Midi1(0, 0x9, RemoteSequenceChannel, note, 100));
        VERIFY_IS_TRUE(command.has_value());
        VERIFY_IS_TRUE(command->Action == action);
    }

    // Only group 1 has the sequence's channel; on group 2, channel 16 is nothing.
    VERIFY_IS_FALSE(Parse(Midi1(1, 0x9, RemoteSequenceChannel, RemoteNotePlay, 100)).has_value());
}

void RemoteControlTests::Midi2MessagesWorkToo()
{
    uint32_t const note[2]{ 0x40920400u, 0x80000000u };
    auto const slot = ParseRemoteMessage(note, 2);
    VERIFY_IS_TRUE(slot.has_value() && slot->Action == RemoteAction::LaunchSlot);
    VERIFY_ARE_EQUAL(size_t{ 2 }, slot->Track);
    VERIFY_ARE_EQUAL(size_t{ 4 }, slot->Scene);

    uint32_t const controller[2]{ 0x40B11400u, 0xFFFFFFFFu };
    auto const mute = ParseRemoteMessage(controller, 2);
    VERIFY_IS_TRUE(mute.has_value() && mute->Action == RemoteAction::Mute);
    VERIFY_IS_TRUE(mute->On);
    VERIFY_ARE_EQUAL(size_t{ 1 }, mute->Track);
}

void RemoteControlTests::NoteOffsAndOtherMessagesDoNothing()
{
    VERIFY_IS_FALSE(Parse(Midi1(0, 0x8, 0, 0, 64)).has_value());
    VERIFY_IS_FALSE(Parse(Midi1(0, 0x9, 0, 0, 0)).has_value());
    VERIFY_IS_FALSE(Parse(Midi1(0, 0xC, 0, 5, 0)).has_value());
    VERIFY_IS_FALSE(Parse(Midi1(0, 0x9, 0, 70, 100)).has_value());
    VERIFY_IS_FALSE(Parse(0x10F80000u).has_value());

    uint32_t const off[2]{ 0x40820400u, 0x80000000u };
    VERIFY_IS_FALSE(ParseRemoteMessage(off, 2).has_value());

    uint32_t const shortMidi2[1]{ 0x40920400u };
    VERIFY_IS_FALSE(ParseRemoteMessage(shortMidi2, 1).has_value());
    VERIFY_IS_FALSE(ParseRemoteMessage(nullptr, 0).has_value());
}

void RemoteControlTests::TracksAreCountedWithoutFolders()
{
    auto folder = MakeTrack(L"f-synths", true);
    folder.Children = { MakeTrack(L"t-bass"), MakeTrack(L"t-pad") };

    auto lead = MakeTrack(L"t-lead");
    lead.Pinned = true;

    Sequence sequence{};
    sequence.Tracks = { MakeTrack(L"t-drums"), folder, lead };

    auto const order = RemoteTrackOrder(sequence);
    VERIFY_ARE_EQUAL(size_t{ 4 }, order.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"t-drums" }, order[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-bass" }, order[1]);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-pad" }, order[2]);
    VERIFY_ARE_EQUAL(std::wstring{ L"t-lead" }, order[3]);

    // Track 17 is group 2, channel 2.
    auto const [group, channel] = RemoteTrackAddress(16);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, group);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, channel);
}

void RemoteControlTests::FeedbackSendsOnlyWhatChanged()
{
    RemoteState first{};
    first.SlotsLit = { { false, true }, { false, false } };
    first.Muted = { false, true };
    first.Soloed = { false, false };
    first.Armed = { false, false };
    first.Playing = true;

    // A surface that just connected gets everything: 4 slots, 6 switches and 4 transport lights.
    auto const everything = RemoteFeedback(nullptr, first);
    VERIFY_ARE_EQUAL(size_t{ 14 }, everything.size());
    VERIFY_IS_TRUE(Contains(everything, Midi1(0, 0x9, 0, 1, 127)));
    VERIFY_IS_TRUE(Contains(everything, Midi1(0, 0x8, 0, 0, 0)));
    VERIFY_IS_TRUE(Contains(everything, Midi1(0, 0xB, 1, RemoteControllerMute, 127)));
    VERIFY_IS_TRUE(Contains(everything, Midi1(0, 0x9, RemoteSequenceChannel, RemoteNotePlay, 127)));
    VERIFY_IS_TRUE(Contains(everything, Midi1(0, 0x8, RemoteSequenceChannel, RemoteNoteRecord, 0)));

    auto second = first;
    second.SlotsLit[0][1] = false;
    second.SlotsLit[1][0] = true;
    second.Playing = false;

    auto const changes = RemoteFeedback(&first, second);
    VERIFY_ARE_EQUAL(size_t{ 3 }, changes.size());
    VERIFY_ARE_EQUAL(Midi1(0, 0x8, 0, 1, 0), changes[0]);
    VERIFY_ARE_EQUAL(Midi1(0, 0x9, 1, 0, 127), changes[1]);
    VERIFY_ARE_EQUAL(Midi1(0, 0x8, RemoteSequenceChannel, RemoteNotePlay, 0), changes[2]);

    VERIFY_IS_TRUE(RemoteFeedback(&second, second).empty());

    // A track that went away is turned off.
    auto third = second;
    third.SlotsLit.pop_back();
    third.Muted.pop_back();
    third.Soloed.pop_back();
    third.Armed.pop_back();

    auto const gone = RemoteFeedback(&second, third);
    VERIFY_ARE_EQUAL(size_t{ 2 }, gone.size());
    VERIFY_IS_TRUE(Contains(gone, Midi1(0, 0x8, 1, 0, 0)));
    VERIFY_IS_TRUE(Contains(gone, Midi1(0, 0xB, 1, RemoteControllerMute, 0)));
}

void RemoteControlTests::TheGlassLayoutFollowsTheLayoutRules()
{
    auto const sequence = testdata::SampleSequence();
    auto const text = MakeGlassLayout(sequence, LayoutText());

    // Set this to look at the file, or to run the layout guide's checker on it.
    wchar_t* dump{ nullptr };
    size_t dumpLength{ 0 };

    if (_wdupenv_s(&dump, &dumpLength, L"SEQUENCER_GLASS_LAYOUT_OUT") == 0 && dump != nullptr)
    {
        auto const utf8 = ToUtf8(text);
        std::ofstream file{ dump, std::ios::binary };
        file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        free(dump);
    }

    VERIFY_ARE_EQUAL(size_t{ 1 }, CheckLayout(text, L"MIDI Sequencer"));

    // A pad for every slot, and a stop pad, mute, solo and arm for every track.
    auto const tracks = RemoteTrackOrder(sequence).size();
    VERIFY_ARE_EQUAL(tracks * sequence.Scenes.size(), CountControls(text, L"-slot-"));
    VERIFY_ARE_EQUAL(tracks, CountControls(text, L"-stop"));
    VERIFY_ARE_EQUAL(tracks * 3, CountControls(text, L"-switch-"));
    VERIFY_ARE_EQUAL(sequence.Scenes.size(), CountControls(text, L"-scene-"));
    VERIFY_ARE_EQUAL(size_t{ 8 }, CountControls(text, L"transport-"));

    // One page needs no tabs.
    VERIFY_ARE_EQUAL(size_t{ 0 }, CountControls(text, L"tab-"));
}

void RemoteControlTests::ABigSequenceSpreadsOverPages()
{
    Sequence sequence{};
    sequence.Name = L"Big";

    for (size_t scene = 0; scene < 20; ++scene)
    {
        sequence.Scenes.push_back(Scene{ L"s-" + std::to_wstring(scene), L"", nullptr });
    }

    for (size_t track = 0; track < 20; ++track)
    {
        auto made = MakeTrack(L"t-" + std::to_wstring(track));
        made.Slots.resize(sequence.Scenes.size());
        sequence.Tracks.push_back(std::move(made));
    }

    // 3 pages of tracks by 3 of scenes is nine pages, which all fit.
    auto const coverage = GlassLayoutCoverage(sequence);
    VERIFY_ARE_EQUAL(size_t{ 20 }, coverage.first);
    VERIFY_ARE_EQUAL(size_t{ 20 }, coverage.second);

    auto const text = MakeGlassLayout(sequence, LayoutText());
    VERIFY_ARE_EQUAL(size_t{ 9 }, CheckLayout(text, L"MIDI Sequencer"));
    VERIFY_ARE_EQUAL(size_t{ 9 }, CountControls(text, L"tab-"));
    VERIFY_ARE_EQUAL(size_t{ 400 }, CountControls(text, L"-slot-"));

    // Far more tracks than nine pages hold: the first 72, and the first 8 scenes.
    for (size_t track = 20; track < 100; ++track)
    {
        sequence.Tracks.push_back(MakeTrack(L"t-" + std::to_wstring(track)));
    }

    auto const big = GlassLayoutCoverage(sequence);
    VERIFY_ARE_EQUAL(GlassMaximumPages * GlassTracksPerPage, big.first);
    VERIFY_ARE_EQUAL(GlassScenesPerPage, big.second);
    VERIFY_ARE_EQUAL(GlassMaximumPages, CheckLayout(MakeGlassLayout(sequence, LayoutText()), L"MIDI Sequencer"));
}
