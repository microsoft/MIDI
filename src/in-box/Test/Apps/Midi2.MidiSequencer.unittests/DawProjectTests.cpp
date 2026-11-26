// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "DawProjectTests.h"

#include "DawProject.h"
#include "SequencePattern.h"
#include "SequenceTestData.h"
#include "ZipArchive.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Beat = TicksPerQuarterNote;
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    std::vector<uint8_t> Bytes(std::string_view text)
    {
        return std::vector<uint8_t>{ text.begin(), text.end() };
    }

    std::vector<uint8_t> Zip(std::string_view project, std::string_view metadata = {})
    {
        std::vector<midiapp::ZipEntry> entries{};
        entries.push_back(midiapp::ZipEntry{ L"project.xml", Bytes(project) });

        if (!metadata.empty())
        {
            entries.push_back(midiapp::ZipEntry{ L"metadata.xml", Bytes(metadata) });
        }

        return midiapp::BuildZip(entries, midiapp::ZipCompression::Deflate);
    }

    std::string Extract(std::vector<uint8_t> const& file, std::wstring const& name)
    {
        midiapp::ZipReader zip{};
        VERIFY_ARE_EQUAL(static_cast<int>(midiapp::ZipStatus::Read), static_cast<int>(zip.Open(file, 16)));

        for (auto const& item : zip.Items())
        {
            if (item.Name == name)
            {
                std::vector<uint8_t> bytes{};
                VERIFY_ARE_EQUAL(static_cast<int>(midiapp::ZipStatus::Read), static_cast<int>(zip.Extract(item, 64u * 1024u * 1024u, bytes)));
                return std::string{ bytes.begin(), bytes.end() };
            }
        }

        VERIFY_FAIL(L"The entry isn't in the zip");
        return {};
    }

    DawProjectText ExportText()
    {
        DawProjectText text{};
        text.Application = L"Windows MIDI Sequencer";
        text.Version = L"1.0.4";
        text.Title = L"Night Drive Sketch";
        text.Artist = L"Pat Example";
        text.Copyright = L"2026 Pat Example";
        text.PlayedOn = [](std::wstring const& device) { return L"Plays on " + device; };
        return text;
    }

    void VerifySameNotes(std::vector<Note> const& expected, std::vector<Note> const& actual)
    {
        VERIFY_ARE_EQUAL(expected.size(), actual.size());

        for (size_t i = 0; i < expected.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expected[i].Tick, actual[i].Tick);
            VERIFY_ARE_EQUAL(expected[i].Length, actual[i].Length);
            VERIFY_ARE_EQUAL(expected[i].Number, actual[i].Number);
            VERIFY_ARE_EQUAL(expected[i].Channel, actual[i].Channel);
            VERIFY_ARE_EQUAL(expected[i].Velocity, actual[i].Velocity);
            VERIFY_ARE_EQUAL(expected[i].ReleaseVelocity, actual[i].ReleaseVelocity);
        }
    }

    Clip const& ClipOf(ImportedDawProject const& project, std::wstring const& id)
    {
        auto const found = std::find_if(project.Clips.begin(), project.Clips.end(), [&](Clip const& clip) { return clip.Id == id; });
        VERIFY_IS_TRUE(found != project.Clips.end());
        return *found;
    }

    // Shaped like what another DAW writes: an instrument with plug-ins, an audio track, a group,
    // the master, clips that share content, automation, markers, tempo and meter changes, and a
    // scene. Written for these tests.
    constexpr std::string_view AnotherAppsProject = R"xml(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Project version="1.0">
  <Application name="Example DAW" version="5.2"/>
  <Transport>
    <Tempo unit="bpm" value="98.0" id="id0" name="Tempo"/>
    <TimeSignature numerator="3" denominator="4" id="id1"/>
  </Transport>
  <Structure>
    <Track contentType="notes" loaded="true" id="id2" name="Lead" color="#d92e24">
      <Channel audioChannels="2" role="regular" solo="false" id="id3">
        <Devices>
          <ClapPlugin deviceID="com.example.poly" deviceName="Polysynth" deviceRole="instrument" loaded="true" id="id4" name="Polysynth">
            <Enabled value="true" id="id5" name="On/Off"/>
            <State path="plugins/poly.clap-preset"/>
          </ClapPlugin>
          <Equalizer deviceRole="audioFX" deviceName="EQ" id="id6" name="EQ"/>
        </Devices>
        <Mute value="true" id="id7" name="Mute"/>
        <Volume unit="linear" value="0.66" id="id8" name="Volume"/>
      </Channel>
    </Track>
    <Track contentType="audio" loaded="true" id="id9" name="Vocals" color="#3cba54">
      <Channel audioChannels="2" role="regular" id="id10"/>
    </Track>
    <Track contentType="tracks" loaded="true" id="id11" name="Group">
      <Channel role="submix" id="id12"/>
      <Track contentType="notes" loaded="true" id="id13" name="Inner">
        <Channel role="regular" id="id14"/>
      </Track>
    </Track>
    <Track contentType="audio notes" loaded="true" id="id15" name="Master">
      <Channel audioChannels="2" role="master" id="id16">
        <Devices>
          <Compressor deviceRole="audioFX" deviceName="Glue" id="id17"/>
        </Devices>
      </Channel>
    </Track>
  </Structure>
  <Arrangement id="id18">
    <Lanes timeUnit="beats" id="id19">
      <Lanes track="id2" id="id20">
        <Clips id="id21">
          <Clip time="4.0" duration="8.0" playStart="1.0" loopStart="0.0" loopEnd="4.0" name="Riff">
            <Notes id="id22">
              <Note time="0.0" duration="0.5" channel="0" key="60" vel="0.5" rel="0.5"/>
              <Note time="1.0" duration="0.5" channel="0" key="61" vel="0.5" rel="0.5">
                <Points id="id23">
                  <Target expression="pitchBend" channel="0"/>
                  <RealPoint time="0.0" value="0.5"/>
                </Points>
              </Note>
              <Note time="2.0" duration="0.5" channel="0" key="62" vel="0.5" rel="0.5"/>
              <Note time="3.0" duration="0.5" channel="0" key="63" vel="1.0"/>
            </Notes>
          </Clip>
          <Clip time="16.0" duration="4.0" reference="id22" name="Riff once"/>
          <Clip time="24.0" duration="4.0" playStart="1.0" loopStart="0.0" loopEnd="4.0" reference="id22" name="Riff again"/>
        </Clips>
        <Points id="id24">
          <Target expression="channelController" channel="0" controller="74"/>
          <RealPoint time="0.0" value="0.25" interpolation="linear"/>
          <RealPoint time="8.0" value="1.0" interpolation="hold"/>
        </Points>
        <Points unit="linear" id="id25">
          <Target parameter="id8"/>
          <RealPoint time="0.0" value="0.5"/>
        </Points>
      </Lanes>
      <Lanes track="id9" id="id26">
        <Clips id="id27">
          <Clip time="0.0" duration="8.0" playStart="0.0">
            <Audio channels="2" duration="4.0" sampleRate="48000" id="id28">
              <File path="audio/vocals.wav"/>
            </Audio>
          </Clip>
        </Clips>
      </Lanes>
      <Lanes track="id13" id="id29">
        <Notes id="id30">
          <Note time="0" duration="1" channel="0" key="48" vel="0.787402"/>
        </Notes>
      </Lanes>
    </Lanes>
    <Markers id="id31">
      <Marker time="0.0" name="Verse"/>
      <Marker time="8.0" name="Chorus" color="#FF0000"/>
    </Markers>
    <TempoAutomation unit="bpm" id="id32">
      <Target parameter="id0"/>
      <RealPoint time="0.0" value="98.0" interpolation="linear"/>
      <RealPoint time="8.0" value="120.0" interpolation="hold"/>
    </TempoAutomation>
    <TimeSignatureAutomation id="id33">
      <Target parameter="id1"/>
      <TimeSignaturePoint time="0.0" numerator="3" denominator="4"/>
      <TimeSignaturePoint time="6.0" numerator="4" denominator="4"/>
    </TimeSignatureAutomation>
  </Arrangement>
  <Scenes>
    <Scene id="id34" name="Hook">
      <Lanes id="id35">
        <ClipSlot track="id2" hasStop="true" id="id36">
          <Clip time="0.0" duration="4.0" loopStart="0.0" loopEnd="4.0">
            <Notes id="id37">
              <Note time="0.0" duration="1.0" channel="0" key="72" vel="0.75"/>
            </Notes>
          </Clip>
        </ClipSlot>
      </Lanes>
    </Scene>
    <Scene id="id38" name="Empty">
      <Lanes id="id39"/>
    </Scene>
  </Scenes>
</Project>
)xml";

    constexpr std::string_view AnotherAppsMetadata = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<MetaData>
  <Title>Night Drive</Title>
  <Artist>Pat Example</Artist>
  <Copyright>2026 Pat Example</Copyright>
</MetaData>
)xml";
}

void DawProjectTests::ASequenceComesBackFromARoundTrip()
{
    auto sequence = testdata::SampleSequence();
    sequence.Tracks[0].Name = L"Drums & \"Perc\" <A>";

    auto const exported = ExportDawProject(sequence, ExportText());
    VERIFY_IS_TRUE(exported.Succeeded);

    auto const project = ImportDawProject(exported.Bytes);
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(project.Status));

    VERIFY_ARE_EQUAL(std::wstring{ L"Windows MIDI Sequencer" }, project.Application);
    VERIFY_ARE_EQUAL(std::wstring{ L"Night Drive Sketch" }, project.Title);
    VERIFY_ARE_EQUAL(std::wstring{ L"Pat Example" }, project.Artist);
    VERIFY_ARE_EQUAL(std::wstring{ L"2026 Pat Example" }, project.Copyright);

    // The tempo ramp, the meter change and the tags all come back where they were.
    VERIFY_ARE_EQUAL(sequence.Tempo.size(), project.Tempo.size());

    for (size_t i = 0; i < sequence.Tempo.size(); ++i)
    {
        VERIFY_ARE_EQUAL(sequence.Tempo[i].Tick, project.Tempo[i].Tick);
        VERIFY_ARE_EQUAL(sequence.Tempo[i].BeatsPerMinute, project.Tempo[i].BeatsPerMinute);
        VERIFY_ARE_EQUAL(sequence.Tempo[i].RampToNext, project.Tempo[i].RampToNext);
    }

    VERIFY_ARE_EQUAL(sequence.Meter.size(), project.Meter.size());

    for (size_t i = 0; i < sequence.Meter.size(); ++i)
    {
        VERIFY_ARE_EQUAL(sequence.Meter[i].Tick, project.Meter[i].Tick);
        VERIFY_ARE_EQUAL(sequence.Meter[i].Numerator, project.Meter[i].Numerator);
        VERIFY_ARE_EQUAL(sequence.Meter[i].Denominator, project.Meter[i].Denominator);
    }

    VERIFY_IS_TRUE(sequence.Tags == project.Tags);

    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Scenes.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Intro" }, project.Scenes[0].Name);
    VERIFY_ARE_EQUAL(std::wstring{ L"Groove" }, project.Scenes[1].Name);

    // The drums, then the folder holding the bass and the pad.
    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Tracks.size());

    auto const& drums = project.Tracks[0];
    VERIFY_ARE_EQUAL(sequence.Tracks[0].Name, drums.Name);
    VERIFY_ARE_EQUAL(uint32_t{ 0xFF8A65 }, drums.Color);
    VERIFY_IS_FALSE(drums.IsFolder);
    VERIFY_IS_TRUE(drums.Destination.Endpoint.IsEmpty());
    VERIFY_ARE_EQUAL(-1, static_cast<int>(drums.Destination.Channel));

    auto const& folder = project.Tracks[1];
    VERIFY_IS_TRUE(folder.IsFolder);
    VERIFY_ARE_EQUAL(std::wstring{ L"Synths" }, folder.Name);
    VERIFY_ARE_EQUAL(size_t{ 2 }, folder.Children.size());

    auto const& bass = folder.Children[0];
    auto const& pad = folder.Children[1];
    VERIFY_ARE_EQUAL(std::wstring{ L"Bass" }, bass.Name);
    VERIFY_ARE_EQUAL(std::wstring{ L"Pad" }, pad.Name);
    VERIFY_IS_TRUE(pad.Muted);
    VERIFY_IS_FALSE(bass.Muted);

    // A placement with no length of its own comes back with the clip's length, which plays the same.
    auto const* beat = FindClip(sequence, L"c-beat-a");
    VERIFY_ARE_EQUAL(size_t{ 2 }, drums.Timeline.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, drums.Timeline[0].Tick);
    VERIFY_ARE_EQUAL(Bar * 4, drums.Timeline[0].Length);
    VERIFY_ARE_EQUAL(Bar * 8, drums.Timeline[1].Tick);
    VERIFY_ARE_EQUAL(Bar, drums.Timeline[1].Length);

    for (auto const& placement : drums.Timeline)
    {
        auto const& clip = ClipOf(project, placement.ClipId);
        VERIFY_IS_TRUE(clip.Loop);
        VERIFY_ARE_EQUAL(Bar, clip.Length);
        VERIFY_ARE_EQUAL(std::wstring{ L"Beat A" }, clip.Name);
        VERIFY_ARE_EQUAL(static_cast<int>(ClipOrigin::Imported), static_cast<int>(clip.Origin));
        VERIFY_ARE_EQUAL(std::wstring{ L"Windows MIDI Sequencer" }, clip.OriginDetail);
        VerifySameNotes(beat->Notes, clip.Notes);
    }

    VERIFY_ARE_EQUAL(size_t{ 2 }, drums.Slots.size());
    VERIFY_IS_FALSE(drums.Slots[0].empty());
    VERIFY_IS_TRUE(drums.Slots[1].empty());
    VerifySameNotes(beat->Notes, ClipOf(project, drums.Slots[0]).Notes);

    // Velocities are 16 bits and come back exactly.
    auto const* bassClip = FindClip(sequence, L"c-bass-a");
    VERIFY_ARE_EQUAL(size_t{ 2 }, bass.Timeline.size());
    VERIFY_ARE_EQUAL(Bar * 4, bass.Timeline[0].Tick);
    VERIFY_ARE_EQUAL(Bar * 12, bass.Timeline[1].Tick);
    VerifySameNotes(bassClip->Notes, ClipOf(project, bass.Timeline[0].ClipId).Notes);
    VERIFY_ARE_EQUAL(Bar * 4, ClipOf(project, bass.Timeline[0].ClipId).Length);

    VERIFY_ARE_EQUAL(size_t{ 2 }, bass.Slots.size());
    VERIFY_IS_TRUE(bass.Slots[0].empty());
    VERIFY_IS_FALSE(bass.Slots[1].empty());

    VERIFY_IS_TRUE(sequence.Tracks[1].Children[0].Tags == bass.Tags);
    VERIFY_IS_TRUE(pad.Timeline.empty());

    // The same file read from disk.
    wchar_t folderPath[MAX_PATH]{};
    VERIFY_IS_TRUE(::GetTempPathW(MAX_PATH, folderPath) > 0);

    auto const path = std::filesystem::path{ folderPath } / L"midi-sequencer-dawproject-test.dawproject";

    {
        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        file.write(reinterpret_cast<char const*>(exported.Bytes.data()), static_cast<std::streamsize>(exported.Bytes.size()));
    }

    auto const fromFile = ImportDawProjectFile(path);
    std::error_code ignored{};
    std::filesystem::remove(path, ignored);

    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(fromFile.Status));
    VERIFY_ARE_EQUAL(project.Clips.size(), fromFile.Clips.size());
    VERIFY_ARE_EQUAL(project.Tracks.size(), fromFile.Tracks.size());
}

void DawProjectTests::TheExportSaysWhereEachTrackPlayed()
{
    auto sequence = testdata::SampleSequence();
    sequence.Tracks[0].Name = L"Drums & \"Perc\" <A>";

    auto const exported = ExportDawProject(sequence, ExportText());
    VERIFY_IS_TRUE(exported.Succeeded);

    auto const project = Extract(exported.Bytes, L"project.xml");
    VERIFY_IS_TRUE(project.starts_with("<?xml version=\"1.0\" encoding=\"UTF-8\""));
    VERIFY_IS_TRUE(project.find("<Project version=\"1.0\">") != std::string::npos);
    VERIFY_IS_TRUE(project.find("<Application name=\"Windows MIDI Sequencer\" version=\"1.0.4\"/>") != std::string::npos);
    VERIFY_IS_TRUE(project.find("comment=\"Plays on TR-8S\"") != std::string::npos);
    VERIFY_IS_TRUE(project.find("comment=\"Plays on Moog One\"") != std::string::npos);
    VERIFY_IS_TRUE(project.find("name=\"Drums &amp; &quot;Perc&quot; &lt;A&gt;\"") != std::string::npos);

    auto const metadata = Extract(exported.Bytes, L"metadata.xml");
    VERIFY_IS_TRUE(metadata.find("<Title>Night Drive Sketch</Title>") != std::string::npos);

    // No timestamps, so the same sequence always gives the same file.
    auto const again = ExportDawProject(sequence, ExportText());
    VERIFY_IS_TRUE(exported.Bytes == again.Bytes);
}

void DawProjectTests::AnotherAppsProjectImports()
{
    auto const project = ImportDawProject(Zip(AnotherAppsProject, AnotherAppsMetadata));
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(project.Status));

    VERIFY_ARE_EQUAL(std::wstring{ L"Example DAW" }, project.Application);
    VERIFY_ARE_EQUAL(std::wstring{ L"Night Drive" }, project.Title);
    VERIFY_ARE_EQUAL(std::wstring{ L"Pat Example" }, project.Artist);
    VERIFY_ARE_EQUAL(std::wstring{ L"2026 Pat Example" }, project.Copyright);

    // 98 BPM rising to 120 over two bars of 3/4, then 4/4.
    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Tempo.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, project.Tempo[0].Tick);
    VERIFY_ARE_EQUAL(98.0, project.Tempo[0].BeatsPerMinute);
    VERIFY_IS_TRUE(project.Tempo[0].RampToNext);
    VERIFY_ARE_EQUAL(Beat * 8, project.Tempo[1].Tick);
    VERIFY_ARE_EQUAL(120.0, project.Tempo[1].BeatsPerMinute);
    VERIFY_IS_FALSE(project.Tempo[1].RampToNext);

    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Meter.size());
    VERIFY_ARE_EQUAL(3, static_cast<int>(project.Meter[0].Numerator));
    VERIFY_ARE_EQUAL(Beat * 6, project.Meter[1].Tick);
    VERIFY_ARE_EQUAL(4, static_cast<int>(project.Meter[1].Numerator));

    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Tags.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Verse" }, project.Tags[0].Text);
    VERIFY_IS_FALSE(project.Tags[0].Color.has_value());
    VERIFY_ARE_EQUAL(Beat * 8, project.Tags[1].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 0xFF0000 }, project.Tags[1].Color.value_or(0));

    // The audio track and the master stay behind. The group keeps its notes track.
    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Tracks.size());

    auto const& lead = project.Tracks[0];
    VERIFY_ARE_EQUAL(std::wstring{ L"Lead" }, lead.Name);
    VERIFY_ARE_EQUAL(uint32_t{ 0xD92E24 }, lead.Color);
    VERIFY_IS_TRUE(lead.Muted);

    auto const& group = project.Tracks[1];
    VERIFY_IS_TRUE(group.IsFolder);
    VERIFY_ARE_EQUAL(size_t{ 1 }, group.Children.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Inner" }, group.Children[0].Name);

    VERIFY_ARE_EQUAL(size_t{ 1 }, project.Instruments.size());
    VERIFY_ARE_EQUAL(lead.Id, project.Instruments[0].first);
    VERIFY_ARE_EQUAL(std::wstring{ L"Polysynth" }, project.Instruments[0].second);

    VERIFY_ARE_EQUAL(size_t{ 1 }, project.LeftOut.AudioTracks);
    VERIFY_ARE_EQUAL(size_t{ 1 }, project.LeftOut.AudioClips);
    VERIFY_ARE_EQUAL(size_t{ 3 }, project.LeftOut.Plugins);
    VERIFY_ARE_EQUAL(size_t{ 1 }, project.LeftOut.OtherAutomation);
    VERIFY_ARE_EQUAL(size_t{ 1 }, project.LeftOut.NoteExpressions);

    // A loop that starts a beat in is the same loop turned round by a beat.
    VERIFY_ARE_EQUAL(size_t{ 3 }, lead.Timeline.size());
    VERIFY_ARE_EQUAL(Beat * 4, lead.Timeline[0].Tick);
    VERIFY_ARE_EQUAL(Beat * 8, lead.Timeline[0].Length);

    auto const& riff = ClipOf(project, lead.Timeline[0].ClipId);
    VERIFY_ARE_EQUAL(std::wstring{ L"Riff" }, riff.Name);
    VERIFY_IS_TRUE(riff.Loop);
    VERIFY_ARE_EQUAL(Beat * 4, riff.Length);
    VERIFY_ARE_EQUAL(size_t{ 4 }, riff.Notes.size());
    VERIFY_ARE_EQUAL(61, static_cast<int>(riff.Notes[0].Number));
    VERIFY_ARE_EQUAL(int64_t{ 0 }, riff.Notes[0].Tick);
    VERIFY_ARE_EQUAL(60, static_cast<int>(riff.Notes[3].Number));
    VERIFY_ARE_EQUAL(Beat * 3, riff.Notes[3].Tick);
    VERIFY_ARE_EQUAL(Beat / 2, riff.Notes[3].Length);
    VERIFY_ARE_EQUAL(uint16_t{ 32768 }, riff.Notes[3].Velocity);
    VERIFY_ARE_EQUAL(uint16_t{ 65535 }, riff.Notes[2].Velocity);

    // The same content played once is its own clip. Played the same way again, it's linked.
    auto const& once = ClipOf(project, lead.Timeline[1].ClipId);
    VERIFY_ARE_NOT_EQUAL(riff.Id, once.Id);
    VERIFY_IS_FALSE(once.Loop);
    VERIFY_ARE_EQUAL(Beat * 4, once.Length);
    VERIFY_ARE_EQUAL(60, static_cast<int>(once.Notes[0].Number));
    VERIFY_ARE_EQUAL(Beat * 16, lead.Timeline[1].Tick);

    VERIFY_ARE_EQUAL(riff.Id, lead.Timeline[2].ClipId);
    VERIFY_ARE_EQUAL(Beat * 24, lead.Timeline[2].Tick);

    // Controller 74 from a quarter to full, then holding.
    VERIFY_ARE_EQUAL(size_t{ 1 }, lead.Lanes.size());
    VERIFY_ARE_EQUAL(static_cast<int>(LaneTarget::Controller), static_cast<int>(lead.Lanes[0].Target));
    VERIFY_ARE_EQUAL(74, static_cast<int>(lead.Lanes[0].Number));
    VERIFY_ARE_EQUAL(size_t{ 2 }, lead.Lanes[0].Points.size());
    VERIFY_ARE_EQUAL(uint32_t{ 0x40000000 }, lead.Lanes[0].Points[0].Value);
    VERIFY_IS_FALSE(lead.Lanes[0].Points[0].Hold);
    VERIFY_ARE_EQUAL(Beat * 8, lead.Lanes[0].Points[1].Tick);
    VERIFY_ARE_EQUAL(uint32_t{ 0xFFFFFFFF }, lead.Lanes[0].Points[1].Value);
    VERIFY_IS_TRUE(lead.Lanes[0].Points[1].Hold);

    // Notes straight on a track's lane become one clip at the start.
    auto const& inner = group.Children[0];
    VERIFY_ARE_EQUAL(size_t{ 1 }, inner.Timeline.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, inner.Timeline[0].Tick);
    VERIFY_ARE_EQUAL(48, static_cast<int>(ClipOf(project, inner.Timeline[0].ClipId).Notes[0].Number));

    // Every track has a slot for each scene. The hook's clip is in the lead's first.
    VERIFY_ARE_EQUAL(size_t{ 2 }, project.Scenes.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Hook" }, project.Scenes[0].Name);
    VERIFY_ARE_EQUAL(size_t{ 2 }, lead.Slots.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, group.Slots.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, inner.Slots.size());
    VERIFY_IS_TRUE(lead.Slots[1].empty());

    auto const& hook = ClipOf(project, lead.Slots[0]);
    VERIFY_IS_TRUE(hook.Loop);
    VERIFY_ARE_EQUAL(Beat * 4, hook.Length);
    VERIFY_ARE_EQUAL(72, static_cast<int>(hook.Notes[0].Number));

    VERIFY_ARE_EQUAL(size_t{ 4 }, project.Clips.size());
}

void DawProjectTests::SecondsBecomeTicksAtTheProjectTempo()
{
    constexpr std::string_view project = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="Example DAW" version="1.0"/>
  <Transport><Tempo unit="bpm" value="120" id="t"/></Transport>
  <Structure>
    <Track contentType="notes" id="k" name="Keys"><Channel role="regular" id="kc"/></Track>
  </Structure>
  <Arrangement id="a">
    <Lanes timeUnit="seconds" id="l">
      <Lanes track="k" id="kl">
        <Clips id="cs">
          <Clip time="3" duration="2" contentTimeUnit="beats" playStart="0">
            <Notes id="n">
              <Note time="0" duration="1" channel="2" key="64" vel="0.5"/>
              <Note time="1" duration="1" channel="2" key="65"/>
            </Notes>
          </Clip>
        </Clips>
      </Lanes>
    </Lanes>
    <TempoAutomation unit="bpm" id="ta">
      <Target parameter="t"/>
      <RealPoint time="0" value="120"/>
      <RealPoint time="4" value="60"/>
    </TempoAutomation>
  </Arrangement>
</Project>
)xml";

    auto const imported = ImportDawProject(Zip(project));
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(imported.Status));

    // Two seconds at 120 is four beats. The third second, at 60, is one more.
    VERIFY_ARE_EQUAL(size_t{ 1 }, imported.Tracks.size());
    auto const& keys = imported.Tracks[0];
    VERIFY_ARE_EQUAL(size_t{ 1 }, keys.Timeline.size());
    VERIFY_ARE_EQUAL(Beat * 5, keys.Timeline[0].Tick);
    VERIFY_ARE_EQUAL(Beat * 2, keys.Timeline[0].Length);

    auto const& clip = ClipOf(imported, keys.Timeline[0].ClipId);
    VERIFY_IS_FALSE(clip.Loop);
    VERIFY_ARE_EQUAL(Beat * 2, clip.Length);
    VERIFY_ARE_EQUAL(size_t{ 2 }, clip.Notes.size());
    VERIFY_ARE_EQUAL(Beat, clip.Notes[1].Tick);
    VERIFY_ARE_EQUAL(Beat, clip.Notes[1].Length);
    VERIFY_ARE_EQUAL(2, static_cast<int>(clip.Notes[1].Channel));

    // A tempo point that doesn't say how it moves on holds.
    VERIFY_ARE_EQUAL(size_t{ 2 }, imported.Tempo.size());
    VERIFY_IS_FALSE(imported.Tempo[0].RampToNext);
}

void DawProjectTests::PatternsGoOutAsTheNotesTheyPlay()
{
    auto pattern = MakeDrumPattern();

    for (uint16_t step = 0; step < 16; step += 4)
    {
        SetStepOn(pattern.Lanes[0], step, true, pattern.Lanes[0].Note);
    }

    // Twelve steps against sixteen take three bars to line up.
    pattern.Lanes[2].StepCount = 12;
    SetStepOn(pattern.Lanes[2], 0, true, pattern.Lanes[2].Note);
    SetStepOn(pattern.Lanes[2], 6, true, pattern.Lanes[2].Note);

    Clip clip{};
    clip.Id = L"c-p";
    clip.Kind = ClipKind::Pattern;
    clip.Name = L"Beat";
    clip.Seed = 99;
    clip.Pattern = std::move(pattern);
    RefreshPatternPreview(clip);

    Track track{};
    track.Id = L"t-1";
    track.Name = L"Drums";
    track.Destination.Channel = -1;
    track.Timeline = { Placement{ L"c-p", 0, Bar * 6 } };

    Sequence sequence{};
    sequence.Clips = { clip };
    sequence.Tracks = { track };
    NormalizeSequence(sequence);

    auto const exported = ExportDawProject(sequence, ExportText());
    VERIFY_IS_TRUE(exported.Succeeded);

    auto const project = ImportDawProject(exported.Bytes);
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(project.Status));
    VERIFY_ARE_EQUAL(size_t{ 1 }, project.Clips.size());

    auto const expected = MakeNotesFromPattern(*FindClip(sequence, L"c-p"), Bar * 64);
    auto const& imported = project.Clips[0];

    VERIFY_ARE_EQUAL(static_cast<int>(ClipKind::Notes), static_cast<int>(imported.Kind));
    VERIFY_ARE_EQUAL(Bar * 3, imported.Length);
    VERIFY_ARE_EQUAL(expected.Length, imported.Length);
    VerifySameNotes(expected.Notes, imported.Notes);
    VERIFY_ARE_EQUAL(Bar * 6, project.Tracks[0].Timeline[0].Length);
}

void DawProjectTests::ThingsTheFormatCantHoldAreCounted()
{
    auto sequence = testdata::SampleSequence();

    // A controller lane goes out. An RPN lane has no place in the format.
    AutomationLane controller{};
    controller.Id = L"a-cc";
    controller.Target = LaneTarget::Controller;
    controller.Number = 1;
    controller.Points = { LanePoint{ 0, 0x40000000, false }, LanePoint{ Beat * 2, 0xFFFFFFFF, true } };

    AutomationLane registered{};
    registered.Id = L"a-rpn";
    registered.Target = LaneTarget::Registered;
    registered.Points = { LanePoint{ 0, 0x80000000, true } };

    auto* beat = FindClip(sequence, L"c-beat-a");
    beat->Lanes = { controller, registered };
    sequence.Tracks[0].Lanes = { registered };

    auto const exported = ExportDawProject(sequence, ExportText());
    VERIFY_IS_TRUE(exported.Succeeded);

    // The bass clip's per-note pitch bend and the bass track's two start-up messages, each once,
    // however many times the clip is placed. Its controller goes out as automation.
    VERIFY_ARE_EQUAL(size_t{ 3 }, exported.SkippedMessages);

    // The clip's RPN lane once, and the track's.
    VERIFY_ARE_EQUAL(size_t{ 2 }, exported.SkippedLanes);

    auto const project = ImportDawProject(exported.Bytes);
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(project.Status));

    // Controller 74 at full 32-bit resolution, holding from where it was recorded.
    auto const& bass = ClipOf(project, project.Tracks[1].Children[0].Timeline[0].ClipId);
    VERIFY_ARE_EQUAL(size_t{ 1 }, bass.Lanes.size());
    VERIFY_ARE_EQUAL(static_cast<int>(LaneTarget::Controller), static_cast<int>(bass.Lanes[0].Target));
    VERIFY_ARE_EQUAL(74, static_cast<int>(bass.Lanes[0].Number));
    VERIFY_ARE_EQUAL(1, static_cast<int>(bass.Lanes[0].Channel));
    VERIFY_ARE_EQUAL(size_t{ 1 }, bass.Lanes[0].Points.size());
    VERIFY_ARE_EQUAL(uint32_t{ 0x80000000 }, bass.Lanes[0].Points[0].Value);
    VERIFY_IS_TRUE(bass.Lanes[0].Points[0].Hold);

    auto const& clip = ClipOf(project, project.Tracks[0].Timeline[0].ClipId);
    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Lanes.size());
    VERIFY_ARE_EQUAL(1, static_cast<int>(clip.Lanes[0].Number));

    // The drums play on channel 10 whatever the lane says, so that's what's written.
    VERIFY_ARE_EQUAL(9, static_cast<int>(clip.Lanes[0].Channel));
    VERIFY_IS_TRUE(controller.Points == clip.Lanes[0].Points);
    VerifySameNotes(beat->Notes, clip.Notes);
    VERIFY_IS_TRUE(project.Tracks[0].Lanes.empty());
}

void DawProjectTests::JunkIsRefused()
{
    auto const status = [](std::vector<uint8_t> const& bytes) { return static_cast<int>(ImportDawProject(bytes).Status); };

    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::NotAProject), status({}));
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::NotAProject), status(Bytes("not a zip at all")));

    // A zip without a project in it.
    std::vector<midiapp::ZipEntry> entries{};
    entries.push_back(midiapp::ZipEntry{ L"readme.txt", Bytes("hello") });
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::NotAProject), status(midiapp::BuildZip(entries, midiapp::ZipCompression::Store)));

    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::NotAProject), status(Zip("<?xml version=\"1.0\"?><Other/>")));
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Damaged), status(Zip("this is not XML")));
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Damaged), status(Zip("<?xml version=\"1.0\"?><Project version=\"1.0\"><Structure>")));

    // A DTD could expand entities without end, so a file with one isn't read.
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Damaged), status(Zip(
        "<?xml version=\"1.0\"?><!DOCTYPE Project [<!ENTITY a \"aaaaaaaaaa\"><!ENTITY b \"&a;&a;&a;&a;&a;&a;&a;&a;\">]>"
        "<Project version=\"1.0\"><Application name=\"&b;\" version=\"1\"/></Project>")));

    // Deeper than any real project.
    std::string deep{ "<?xml version=\"1.0\"?><Project version=\"1.0\">" };

    for (int i = 0; i < 200; ++i)
    {
        deep += "<Lanes>";
    }

    for (int i = 0; i < 200; ++i)
    {
        deep += "</Lanes>";
    }

    deep += "</Project>";
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Damaged), status(Zip(deep)));

    // Half a file.
    auto whole = ExportDawProject(testdata::SampleSequence(), ExportText()).Bytes;
    whole.resize(whole.size() / 2);
    VERIFY_ARE_NOT_EQUAL(static_cast<int>(DawProjectStatus::Read), status(whole));

    // A project with nothing in it is still a project.
    auto const empty = ImportDawProject(Zip("<?xml version=\"1.0\"?><Project version=\"1.0\"><Application name=\"x\" version=\"1\"/></Project>"));
    VERIFY_ARE_EQUAL(static_cast<int>(DawProjectStatus::Read), static_cast<int>(empty.Status));
    VERIFY_IS_TRUE(empty.Tracks.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, empty.Tempo.size());
    VERIFY_ARE_EQUAL(120.0, empty.Tempo[0].BeatsPerMinute);
}
