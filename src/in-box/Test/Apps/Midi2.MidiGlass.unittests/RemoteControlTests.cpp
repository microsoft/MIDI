// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RemoteControlTests.h"

#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "RemoteRules.h"
#include "EditorController.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // Three pages and a band, a controller and a synth, two grids stacked in one group, and a
    // button. Enough for every rule to have something to aim at.
    glass::LayoutDocument SetList()
    {
        glass::LayoutDocument document{};

        document.Name = L"Set list";

        glass::DeviceEntry foot{};
        foot.Name = L"Foot";
        document.Devices.push_back(foot);

        glass::DeviceEntry synth{};
        synth.Name = L"Synth";
        document.Devices.push_back(synth);

        for (auto const* const name : { L"Verse", L"Chorus", L"Bridge" })
        {
            glass::Page page{};
            page.Id = std::wstring{ L"page-" } + name;
            page.Name = name;
            document.Pages.push_back(page);
        }

        glass::Page band{};
        band.Id = L"page-band";
        band.Name = L"Band";
        band.IsSharedBand = true;
        document.Pages.push_back(band);

        glass::Control grid{};
        grid.Id = L"grid-a";
        grid.Kind = glass::ControlKind::NotePads;
        grid.GroupId = L"stack";
        document.Pages[0].Controls.push_back(grid);

        grid.Id = L"grid-b";
        grid.Kind = glass::ControlKind::HexPads;
        grid.StartsHidden = true;
        document.Pages[0].Controls.push_back(grid);

        glass::Control button{};
        button.Id = L"mute";
        button.Kind = glass::ControlKind::Button;
        button.StartsDisabled = true;
        document.Pages[1].Controls.push_back(button);

        glass::Sequence sequence{};
        sequence.Name = L"Patch recall";
        document.Sequences.push_back(sequence);

        return document;
    }

    glass::RemoteRule Rule(
        _In_ glass::MessageKind kind,
        _In_ int32_t number,
        _In_ glass::RemoteActionKind action)
    {
        glass::RemoteRule rule{};

        rule.When.DeviceName = L"Foot";
        rule.When.Kind = kind;
        rule.When.Number = number;
        rule.Action = action;

        return rule;
    }

    glass::LayoutDocument RoundTrip(_In_ glass::LayoutDocument const& document)
    {
        auto const json = glass::WriteLayoutToJson(document);

        VERIFY_IS_FALSE(json.empty());

        auto const read = glass::ReadLayoutFromJson(json);

        VERIFY_IS_TRUE(read.Succeeded);

        return read.Document;
    }

    // Every rule this message sets off, by index.
    std::vector<glass::RemoteHit> Match(
        _In_ glass::LayoutDocument const& document,
        _In_ std::vector<uint32_t> const& words,
        _In_ int32_t deviceIndex = 0)
    {
        glass::RemoteRuleTable table{};
        table.Prepare(document);

        std::array<glass::RemoteHit, glass::MaximumRemoteHitsPerMessage> hits{};

        auto const count = table.Match(words.data(), static_cast<uint32_t>(words.size()), deviceIndex, hits);

        return { hits.begin(), hits.begin() + count };
    }

    glass::RemoteHit HitCarrying(_In_ int32_t carried, _In_ double value = 1.0)
    {
        glass::RemoteHit hit{};
        hit.Carried = carried;
        hit.Value = value;
        return hit;
    }

    constexpr uint32_t ProgramChange3 = 0x20C00300;
    constexpr uint32_t ControlChange20Down = 0x20B0147F;
    constexpr uint32_t ControlChange20Up = 0x20B01400;
    constexpr uint32_t NoteOn60 = 0x20903C64;
    constexpr uint32_t NoteOff60 = 0x20803C00;
}

// ---- the file ----

void RemoteControlTests::RulesRoundTripThroughTheFile()
{
    auto document = SetList();

    auto page = Rule(glass::MessageKind::ProgramChange, glass::AnyRemoteNumber, glass::RemoteActionKind::PageFromMessage);
    page.When.ChannelIndex = glass::AnyRemoteChannel;
    document.Remote.Rules.push_back(page);

    auto grid = Rule(glass::MessageKind::ControlChange, 21, glass::RemoteActionKind::PadGrid);
    grid.PadFunction = glass::PadGridFunction::NextScale;
    grid.PadTargets.AllGrids = false;
    grid.PadTargets.ControlIds = { L"grid-a" };
    grid.When.GroupIndex = 2;
    grid.When.ChannelIndex = 9;
    document.Remote.Rules.push_back(grid);

    auto root = Rule(glass::MessageKind::Note, 60, glass::RemoteActionKind::SetKeyRoot);
    root.KeyRoot = 2;
    root.Enabled = false;
    document.Remote.Rules.push_back(root);

    auto value = Rule(glass::MessageKind::ControlChange, 11, glass::RemoteActionKind::SetControlValue);
    value.TargetGroupId = L"stack";
    value.Value = 0.25;
    document.Remote.Rules.push_back(value);

    auto sequence = Rule(glass::MessageKind::ProgramChange, 9, glass::RemoteActionKind::RunSequence);
    sequence.SequenceName = L"Patch recall";
    document.Remote.Rules.push_back(sequence);

    document.Remote.TellPage.Enabled = true;
    document.Remote.TellPage.DeviceName = L"Foot";
    document.Remote.TellPage.Kind = glass::MessageKind::ControlChange;
    document.Remote.TellPage.Number = 30;
    document.Remote.TellPage.FirstValue = 1;

    auto const read = RoundTrip(document);
    auto const& rules = read.Remote.Rules;

    VERIFY_ARE_EQUAL(size_t{ 5 }, rules.size());
    VERIFY_IS_TRUE(rules[0].Action == glass::RemoteActionKind::PageFromMessage);
    VERIFY_ARE_EQUAL(glass::AnyRemoteNumber, rules[0].When.Number);
    VERIFY_ARE_EQUAL(glass::AnyRemoteChannel, rules[0].When.ChannelIndex);
    VERIFY_IS_TRUE(rules[1].PadFunction == glass::PadGridFunction::NextScale);
    VERIFY_IS_FALSE(rules[1].PadTargets.AllGrids);
    VERIFY_ARE_EQUAL(std::wstring{ L"grid-a" }, rules[1].PadTargets.ControlIds.at(0));
    VERIFY_ARE_EQUAL(2, rules[1].When.GroupIndex);
    VERIFY_ARE_EQUAL(9, rules[1].When.ChannelIndex);
    VERIFY_ARE_EQUAL(2, rules[2].KeyRoot);
    VERIFY_IS_FALSE(rules[2].Enabled);
    VERIFY_ARE_EQUAL(std::wstring{ L"stack" }, rules[3].TargetGroupId);
    VERIFY_IS_LESS_THAN(std::abs(rules[3].Value - 0.25), 0.0001);
    VERIFY_ARE_EQUAL(std::wstring{ L"Patch recall" }, rules[4].SequenceName);

    VERIFY_IS_TRUE(read.Remote.TellPage.Enabled);
    VERIFY_IS_TRUE(read.Remote.TellPage.Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(30, read.Remote.TellPage.Number);
    VERIFY_ARE_EQUAL(1, read.Remote.TellPage.FirstValue);

    // Written twice, the same bytes.
    VERIFY_ARE_EQUAL(glass::WriteLayoutToJson(read), glass::WriteLayoutToJson(RoundTrip(read)));
}

void RemoteControlTests::ALayoutWithNoRulesWritesNoObject()
{
    auto const json = glass::WriteLayoutToJson(SetList());

    VERIFY_ARE_EQUAL(std::wstring::npos, json.find(L"remoteControl"));
}

void RemoteControlTests::UnknownFieldsSurviveASave()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 1, glass::RemoteActionKind::NextPage));

    auto json = glass::WriteLayoutToJson(document);

    // A newer build's fields at every level of the object.
    auto const at = json.find(L"\"remoteControl\": {");
    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, at);
    json.insert(at + std::wstring{ L"\"remoteControl\": {" }.size(), L"\"futureList\": true,");

    auto const when = json.find(L"\"when\": {");
    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, when);
    json.insert(when + std::wstring{ L"\"when\": {" }.size(), L"\"velocityAbove\": 64,");

    auto const read = glass::ReadLayoutFromJson(json);
    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_FALSE(read.IsFromNewerVersion);

    auto const written = glass::WriteLayoutToJson(read.Document);

    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, written.find(L"\"futureList\": true"));
    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, written.find(L"\"velocityAbove\": 64"));
}

void RemoteControlTests::AnUnknownActionMarksTheFileNewer()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 1, glass::RemoteActionKind::NextPage));

    auto json = glass::WriteLayoutToJson(document);
    auto const at = json.find(L"\"nextPage\"");
    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, at);
    json.replace(at, std::wstring{ L"\"nextPage\"" }.size(), L"\"tapTempo\"");

    auto const read = glass::ReadLayoutFromJson(json);

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.IsFromNewerVersion);
    VERIFY_IS_TRUE(read.Document.Remote.Rules.at(0).Action == glass::RemoteActionKind::Unrecognized);
    VERIFY_ARE_EQUAL(std::wstring{ L"tapTempo" }, read.Document.Remote.Rules.at(0).UnrecognizedAction);

    // Never run.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(read.Document, { 0x20C00100 }).size());
}

void RemoteControlTests::HostileValuesAreBounded()
{
    std::wstring const json = LR"({
        "fileVersion": 1, "name": "x", "pages": [ { "id": "p", "name": "P", "controls": [] } ],
        "remoteControl": {
            "rules": [
                { "when": { "device": "Foot", "kind": "controlChange", "group": 99, "channel": 40, "number": 900 },
                  "do": "setControlValue", "control": "c", "value": 7.5 },
                { "when": { "kind": "programChange", "number": -3 }, "do": "setKeyRoot", "key": "h" }
            ],
            "tellPage": { "enabled": true, "device": "Foot", "kind": "programChange", "firstValue": 4000 }
        } })";

    auto const read = glass::ReadLayoutFromJson(json);

    VERIFY_IS_TRUE(read.Succeeded);

    auto const& rules = read.Document.Remote.Rules;

    VERIFY_ARE_EQUAL(size_t{ 2 }, rules.size());
    VERIFY_ARE_EQUAL(0, rules[0].When.GroupIndex);
    VERIFY_ARE_EQUAL(0, rules[0].When.ChannelIndex);
    VERIFY_ARE_EQUAL(0, rules[0].When.Number);
    VERIFY_IS_LESS_THAN(std::abs(rules[0].Value - 1.0), 0.0001);
    VERIFY_ARE_EQUAL(0, rules[1].When.Number);
    VERIFY_ARE_EQUAL(0, rules[1].KeyRoot);
    VERIFY_ARE_EQUAL(0, read.Document.Remote.TellPage.FirstValue);
}

void RemoteControlTests::StartStatesAndOpenSequenceRoundTrip()
{
    auto document = SetList();
    document.Pages[1].OpenSequenceName = L"Patch recall";

    auto const read = RoundTrip(document);

    VERIFY_IS_FALSE(read.Pages[0].Controls[0].StartsHidden);
    VERIFY_IS_TRUE(read.Pages[0].Controls[1].StartsHidden);
    VERIFY_IS_TRUE(read.Pages[1].Controls[0].StartsDisabled);
    VERIFY_ARE_EQUAL(std::wstring{ L"Patch recall" }, read.Pages[1].OpenSequenceName);
    VERIFY_IS_TRUE(read.Pages[0].OpenSequenceName.empty());

    // Off by default, and then not in the file at all.
    auto const json = glass::WriteLayoutToJson(document);
    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, json.find(L"startsDisabled"));
    VERIFY_ARE_EQUAL(json.find(L"startsDisabled"), json.rfind(L"startsDisabled"));
    VERIFY_ARE_EQUAL(json.find(L"startsHidden"), json.rfind(L"startsHidden"));
}

// ---- matching ----

void RemoteControlTests::AProgramChangeMatchesItsRule()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 2, glass::RemoteActionKind::NextPage));
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 3, glass::RemoteActionKind::PreviousPage));

    auto const hits = Match(document, { ProgramChange3 });

    VERIFY_ARE_EQUAL(size_t{ 1 }, hits.size());
    VERIFY_ARE_EQUAL(1u, hits[0].RuleIndex);
    VERIFY_ARE_EQUAL(3, hits[0].Carried);
}

void RemoteControlTests::DeviceGroupChannelAndNumberAllHaveToAgree()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 3, glass::RemoteActionKind::NextPage));

    VERIFY_ARE_EQUAL(size_t{ 1 }, Match(document, { ProgramChange3 }, 0).size());

    // The synth, not the foot controller.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { ProgramChange3 }, 1).size());

    // Group 2.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { ProgramChange3 | 0x02000000 }).size());

    // Channel 5.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { ProgramChange3 | 0x00040000 }).size());

    // Program 4.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { 0x20C00400 }).size());

    // A control change with the same number.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { 0x20B0037F }).size());
}

void RemoteControlTests::AnyChannelAndAnyNumberMatchEverything()
{
    auto document = SetList();

    auto rule = Rule(glass::MessageKind::ProgramChange, glass::AnyRemoteNumber, glass::RemoteActionKind::PageFromMessage);
    rule.When.ChannelIndex = glass::AnyRemoteChannel;
    rule.When.DeviceName.clear();
    document.Remote.Rules.push_back(rule);

    VERIFY_ARE_EQUAL(size_t{ 1 }, Match(document, { 0x20CF7F00 }, 1).size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Match(document, { 0x20C00000 }, -1).size());
}

void RemoteControlTests::AFootSwitchReleaseDoesNotFireTwice()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ControlChange, 20, glass::RemoteActionKind::NextPage));

    VERIFY_ARE_EQUAL(size_t{ 1 }, Match(document, { ControlChange20Down }).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { ControlChange20Up }).size());
}

void RemoteControlTests::ARuleThatTakesTheValueFiresOnZero()
{
    auto document = SetList();

    auto rule = Rule(glass::MessageKind::ControlChange, 20, glass::RemoteActionKind::SetControlValue);
    rule.FromMessage = true;
    rule.TargetControlId = L"mute";
    document.Remote.Rules.push_back(rule);

    auto const hits = Match(document, { ControlChange20Up });

    VERIFY_ARE_EQUAL(size_t{ 1 }, hits.size());
    VERIFY_ARE_EQUAL(0, hits[0].Carried);
    VERIFY_IS_LESS_THAN(hits[0].Value, 0.0001);
}

void RemoteControlTests::ANoteOffFiresNothing()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::Note, 60, glass::RemoteActionKind::Panic));

    VERIFY_ARE_EQUAL(size_t{ 1 }, Match(document, { NoteOn60 }).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { NoteOff60 }).size());

    // A note on at velocity 0 is a note off too.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { 0x20903C00 }).size());
}

void RemoteControlTests::AMidi2MessageMatchesToo()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 5, glass::RemoteActionKind::Panic));

    auto const hits = Match(document, { 0x40C00000, 0x05000000 });

    VERIFY_ARE_EQUAL(size_t{ 1 }, hits.size());
    VERIFY_ARE_EQUAL(5, hits[0].Carried);
}

void RemoteControlTests::ARuleThatIsOffOrOnAMissingDeviceNeverMatches()
{
    auto document = SetList();

    auto off = Rule(glass::MessageKind::ProgramChange, 3, glass::RemoteActionKind::Panic);
    off.Enabled = false;
    document.Remote.Rules.push_back(off);

    auto missing = Rule(glass::MessageKind::ProgramChange, 3, glass::RemoteActionKind::Panic);
    missing.When.DeviceName = L"Not in the table";
    document.Remote.Rules.push_back(missing);

    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { ProgramChange3 }).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Match(document, { ProgramChange3 }, -1).size());
}

// ---- what a rule does ----

void RemoteControlTests::PagesGoByIdNextPreviousAndNumber()
{
    auto const document = SetList();
    glass::RemoteCommand command{};

    auto rule = Rule(glass::MessageKind::ProgramChange, 0, glass::RemoteActionKind::GoToPage);
    rule.TargetPageId = L"page-Bridge";
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));
    VERIFY_ARE_EQUAL(2, command.PageIndex);

    rule.Action = glass::RemoteActionKind::NextPage;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));
    VERIFY_ARE_EQUAL(1, command.PageIndex);

    rule.Action = glass::RemoteActionKind::PreviousPage;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 2, command));
    VERIFY_ARE_EQUAL(1, command.PageIndex);

    // Program 1 is the second page; the band is always on screen and is never one of them.
    rule.Action = glass::RemoteActionKind::PageFromMessage;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(1), 0, command));
    VERIFY_ARE_EQUAL(1, command.PageIndex);
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(3), 0, command));
}

void RemoteControlTests::NextAndPreviousStopAtTheEnds()
{
    auto const document = SetList();
    glass::RemoteCommand command{};

    auto rule = Rule(glass::MessageKind::ProgramChange, 0, glass::RemoteActionKind::NextPage);
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 2, command));

    rule.Action = glass::RemoteActionKind::PreviousPage;
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));
}

void RemoteControlTests::AGroupTargetReachesEveryMember()
{
    auto const document = SetList();
    glass::RemoteCommand command{};

    auto rule = Rule(glass::MessageKind::ProgramChange, 0, glass::RemoteActionKind::DisableControl);
    rule.TargetGroupId = L"stack";

    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));
    VERIFY_ARE_EQUAL(size_t{ 2 }, command.Controls.size());
    VERIFY_ARE_EQUAL(0u, command.Controls[0]);
    VERIFY_ARE_EQUAL(1u, command.Controls[1]);
}

void RemoteControlTests::ShowOnlyThisHidesTheRestOfItsGroup()
{
    auto const document = SetList();
    glass::RemoteCommand command{};

    auto rule = Rule(glass::MessageKind::ProgramChange, 0, glass::RemoteActionKind::ShowOnlyThis);
    rule.TargetControlId = L"grid-b";

    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));
    VERIFY_ARE_EQUAL(size_t{ 1 }, command.Controls.size());
    VERIFY_ARE_EQUAL(1u, command.Controls[0]);
    VERIFY_ARE_EQUAL(size_t{ 1 }, command.HideControls.size());
    VERIFY_ARE_EQUAL(0u, command.HideControls[0]);

    glass::ControlStateSet states{};
    states.Prepare(document);

    VERIFY_IS_TRUE(states.IsShown(0));
    VERIFY_IS_FALSE(states.IsShown(1));

    auto const changed = states.Apply(command);

    VERIFY_ARE_EQUAL(size_t{ 2 }, changed.size());
    VERIFY_IS_FALSE(states.IsShown(0));
    VERIFY_IS_TRUE(states.IsShown(1));
}

void RemoteControlTests::ValuesRootsAndScalesComeFromTheMessage()
{
    auto const document = SetList();
    glass::RemoteCommand command{};

    auto rule = Rule(glass::MessageKind::ControlChange, 11, glass::RemoteActionKind::SetControlValue);
    rule.TargetControlId = L"mute";
    rule.Value = 0.75;

    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(64, 0.5), 0, command));
    VERIFY_IS_LESS_THAN(std::abs(command.Value - 0.75), 0.0001);

    rule.FromMessage = true;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(64, 0.5), 0, command));
    VERIFY_IS_LESS_THAN(std::abs(command.Value - 0.5), 0.0001);

    // Note 62 is a D, whatever its octave.
    rule.Action = glass::RemoteActionKind::SetKeyRoot;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(62), 0, command));
    VERIFY_ARE_EQUAL(2, command.KeyRoot);

    rule.Action = glass::RemoteActionKind::SetScale;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(4), 0, command));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(glass::MusicalScale::Dorian), command.Scale);
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(13), 0, command));

    rule.FromMessage = false;
    rule.Scale = glass::MusicalScale::Blues;
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, rule, HitCarrying(4), 0, command));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(glass::MusicalScale::Blues), command.Scale);
}

void RemoteControlTests::ARuleNamingWhatIsNotThereDoesNothing()
{
    auto const document = SetList();
    glass::RemoteCommand command{};

    auto rule = Rule(glass::MessageKind::ProgramChange, 0, glass::RemoteActionKind::PressControl);
    rule.TargetControlId = L"gone";
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));

    rule.Action = glass::RemoteActionKind::GoToPage;
    rule.TargetPageId = L"page-gone";
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));

    rule.Action = glass::RemoteActionKind::RunSequence;
    rule.SequenceName = L"Not a sequence";
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));

    rule.Action = glass::RemoteActionKind::Unrecognized;
    VERIFY_IS_FALSE(glass::ResolveRemoteCommand(document, rule, HitCarrying(0), 0, command));
}

// ---- control state ----

void RemoteControlTests::ControlsStartAsTheLayoutSays()
{
    glass::ControlStateSet states{};
    states.Prepare(SetList());

    VERIFY_ARE_EQUAL(size_t{ 3 }, states.Count());
    VERIFY_IS_TRUE(states.TakesInput(0));
    VERIFY_IS_FALSE(states.TakesInput(1));
    VERIFY_IS_TRUE(states.IsEnabled(1));
    VERIFY_IS_FALSE(states.IsEnabled(2));
    VERIFY_IS_TRUE(states.IsShown(2));
    VERIFY_IS_TRUE(states.AnyChanged());

    // Past the end is a control the set doesn't know, and it is left alone.
    VERIFY_IS_TRUE(states.TakesInput(99));
}

void RemoteControlTests::EnableDisableAndToggleChangeState()
{
    glass::ControlStateSet states{};
    states.Prepare(SetList());

    glass::RemoteCommand command{};
    command.Action = glass::RemoteActionKind::EnableControl;
    command.Controls = { 2 };

    VERIFY_ARE_EQUAL(size_t{ 1 }, states.Apply(command).size());
    VERIFY_IS_TRUE(states.IsEnabled(2));

    // Already on: nothing changes, nothing to redraw.
    VERIFY_ARE_EQUAL(size_t{ 0 }, states.Apply(command).size());

    command.Action = glass::RemoteActionKind::ToggleEnabled;
    states.Apply(command);
    VERIFY_IS_FALSE(states.IsEnabled(2));

    // A toggle on a half-shown group goes by its first member, so the group comes out all one way.
    command.Action = glass::RemoteActionKind::ToggleShown;
    command.Controls = { 0, 1 };
    states.Apply(command);
    VERIFY_IS_FALSE(states.IsShown(0));
    VERIFY_IS_FALSE(states.IsShown(1));

    states.Reset();
    VERIFY_IS_TRUE(states.IsShown(0));
    VERIFY_IS_FALSE(states.IsShown(1));
    VERIFY_IS_FALSE(states.IsEnabled(2));
}

void RemoteControlTests::AnEditKeepsWhatARuleDid()
{
    auto document = SetList();

    glass::ControlStateSet states{};
    states.Prepare(document);

    glass::RemoteCommand command{};
    command.Action = glass::RemoteActionKind::EnableControl;
    command.Controls = { 2 };
    states.Apply(command);

    // An edit somewhere else in Try mode.
    document.Pages[2].Name = L"Outro";
    states.Prepare(document);
    VERIFY_IS_TRUE(states.IsEnabled(2));

    // An edit to how it starts is a new start.
    document.Pages[1].Controls[0].StartsDisabled = false;
    states.Prepare(document);
    VERIFY_IS_TRUE(states.IsEnabled(2));

    document.Pages[1].Controls[0].StartsDisabled = true;
    states.Prepare(document);
    VERIFY_IS_FALSE(states.IsEnabled(2));
}

// ---- loops and the controller's lights ----

void RemoteControlTests::TheLayoutsOwnOutputIsNotAPress()
{
    glass::RemoteEchoGuard guard{};

    // Sent as MIDI 2.0, and back as MIDI 1.0 after the service converted it.
    uint32_t const sent[2]{ 0x40B01400, 0xFFFFFFFF };
    guard.NoteSent(0, sent, 2, 1000);

    VERIFY_IS_TRUE(guard.IsEcho(0, &ControlChange20Down, 1, 1100));

    // From another device, a different controller, or long after: a real press.
    VERIFY_IS_FALSE(guard.IsEcho(1, &ControlChange20Down, 1, 1100));
    VERIFY_IS_FALSE(guard.IsEcho(0, &NoteOn60, 1, 1100));
    VERIFY_IS_FALSE(guard.IsEcho(0, &ControlChange20Down, 1, 1000 + glass::RemoteEchoGuard::EchoMilliseconds + 1));

    guard.Clear();
    VERIFY_IS_FALSE(guard.IsEcho(0, &ControlChange20Down, 1, 1100));
}

void RemoteControlTests::TheRateLimitStopsARunaway()
{
    glass::RemoteRateLimit limit{};

    for (uint32_t i = 0; i < glass::RemoteRateLimit::MaximumPerSecond; ++i)
    {
        VERIFY_IS_TRUE(limit.Allow(5000));
    }

    VERIFY_IS_FALSE(limit.Allow(5500));
    VERIFY_IS_TRUE(limit.Allow(6000));
}

void RemoteControlTests::ThePageIsToldAsAProgramOrAControlChange()
{
    auto document = SetList();
    int32_t device{ -1 };
    uint32_t words[1]{};

    // Off unless asked for.
    VERIFY_ARE_EQUAL(0u, glass::BuildPageFeedback(document, 1, device, words));

    document.Remote.TellPage.Enabled = true;
    document.Remote.TellPage.DeviceName = L"Foot";
    document.Remote.TellPage.ChannelIndex = 2;

    VERIFY_ARE_EQUAL(1u, glass::BuildPageFeedback(document, 1, device, words));
    VERIFY_ARE_EQUAL(0, device);
    VERIFY_ARE_EQUAL(0x20C20100u, words[0]);

    document.Remote.TellPage.Kind = glass::MessageKind::ControlChange;
    document.Remote.TellPage.Number = 30;
    document.Remote.TellPage.FirstValue = 10;

    VERIFY_ARE_EQUAL(1u, glass::BuildPageFeedback(document, 2, device, words));
    VERIFY_ARE_EQUAL(0x20B21E0Cu, words[0]);

    // The band is not a page the controller can name.
    VERIFY_ARE_EQUAL(0u, glass::BuildPageFeedback(document, 3, device, words));

    document.Remote.TellPage.DeviceName = L"Gone";
    VERIFY_ARE_EQUAL(0u, glass::BuildPageFeedback(document, 1, device, words));
}

// ---- the editor ----

void RemoteControlTests::EditorEditsAreOneUndoStep()
{
    glass::EditorController controller{};
    controller.Load(SetList());

    auto remote = controller.Document().Remote;
    remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 1, glass::RemoteActionKind::NextPage));

    VERIFY_IS_TRUE(controller.SetRemoteControl(remote));

    // The same again is no edit at all.
    VERIFY_IS_FALSE(controller.SetRemoteControl(remote));

    remote.Rules[0].When.Number = 12;
    VERIFY_IS_TRUE(controller.SetRemoteControl(remote));

    remote.Rules[0].When.Number = 127;
    VERIFY_IS_TRUE(controller.SetRemoteControl(remote));
    VERIFY_ARE_EQUAL(127, controller.Document().Remote.Rules.at(0).When.Number);

    VERIFY_IS_TRUE(controller.Undo());
    VERIFY_IS_TRUE(controller.Document().Remote.Rules.empty());

    VERIFY_IS_TRUE(controller.SetPageOpenSequence(1, L"Patch recall"));
    VERIFY_IS_FALSE(controller.SetPageOpenSequence(1, L"No such sequence"));
    // Like every control setting, it works on the page being edited.
    controller.SetPageIndex(1);
    VERIFY_IS_TRUE(controller.SetControlStartsHidden(L"mute", true));
    VERIFY_IS_TRUE(controller.SetControlStartsDisabled(L"mute", false));
    VERIFY_IS_TRUE(controller.Document().FindControl(L"mute")->StartsHidden);
}

void RemoteControlTests::RenamingADeviceCarriesTheRules()
{
    auto document = SetList();
    document.Remote.Rules.push_back(Rule(glass::MessageKind::ProgramChange, 1, glass::RemoteActionKind::NextPage));
    document.Remote.TellPage.DeviceName = L"Foot";

    glass::EditorController controller{};
    controller.Load(document);

    VERIFY_IS_TRUE(controller.RenameDevice(L"Foot", L"MC6"));
    VERIFY_ARE_EQUAL(std::wstring{ L"MC6" }, controller.Document().Remote.Rules.at(0).When.DeviceName);
    VERIFY_ARE_EQUAL(std::wstring{ L"MC6" }, controller.Document().Remote.TellPage.DeviceName);
}

void RemoteControlTests::TheListensShortcutAddsARule()
{
    glass::EditorController controller{};
    controller.Load(SetList());

    VERIFY_ARE_EQUAL(0, controller.AddRemoteRuleFor(L"mute"));
    VERIFY_ARE_EQUAL(-1, controller.AddRemoteRuleFor(L"gone"));

    auto const& rule = controller.Document().Remote.Rules.at(0);

    VERIFY_IS_TRUE(rule.Action == glass::RemoteActionKind::EnableControl);
    VERIFY_ARE_EQUAL(std::wstring{ L"mute" }, rule.TargetControlId);
    VERIFY_ARE_EQUAL(std::wstring{ L"Foot" }, rule.When.DeviceName);
}

// ---- the example the docs hand out ----

void RemoteControlTests::TheDocsExampleLoadsAndRuns()
{
    // Found from this source file, so it is the copy in the same checkout.
    auto const path = std::filesystem::path{ __FILE__ }.parent_path() /
        L"../../../../../docs/assets/layouts/midi-glass-foot-controller.midilayout";

    if (!std::filesystem::exists(path))
    {
        Log::Comment(L"The docs folder isn't next to this build, so the example isn't checked here.");
        return;
    }

    std::ifstream file{ path, std::ios::binary };
    std::stringstream bytes{};
    bytes << file.rdbuf();

    auto const text = bytes.str();
    auto const read = glass::ReadLayoutFromJson(std::wstring{ winrt::to_hstring(text) });

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_FALSE(read.IsFromNewerVersion);

    auto const& document = read.Document;

    VERIFY_ARE_EQUAL(size_t{ 8 }, document.Remote.Rules.size());
    VERIFY_IS_TRUE(document.Remote.TellPage.Enabled);
    VERIFY_ARE_EQUAL(std::wstring{ L"Piano sound" }, document.Pages.at(0).OpenSequenceName);

    // Switch D from the foot controller shows the hex pads and hides the square ones.
    auto const hits = Match(document, { 0x20B0177F }, 0);
    VERIFY_ARE_EQUAL(size_t{ 1 }, hits.size());

    glass::RemoteCommand command{};
    VERIFY_IS_TRUE(glass::ResolveRemoteCommand(document, document.Remote.Rules[hits[0].RuleIndex], hits[0], 0, command));

    glass::ControlStateSet states{};
    states.Prepare(document);
    VERIFY_IS_FALSE(states.IsShown(1));

    states.Apply(command);
    VERIFY_IS_TRUE(states.IsShown(1));
    VERIFY_IS_FALSE(states.IsShown(0));

    // The page sent back as program 1 for the second page.
    int32_t device{ -1 };
    uint32_t words[1]{};
    VERIFY_ARE_EQUAL(1u, glass::BuildPageFeedback(document, 1, device, words));
    VERIFY_ARE_EQUAL(0x20C00100u, words[0]);
}
