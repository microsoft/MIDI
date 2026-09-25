// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LayoutDocumentTests.h"
#include "TestLayoutFiles.h"

#include <winrt/Windows.Foundation.h>

#include "LayoutModel.h"
#include "LayoutSerializer.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument LoadHandAuthored()
    {
        auto const result = glass::ReadLayoutFromJson(glasstests::HandAuthoredLayout());
        VERIFY_IS_TRUE(result.Succeeded);
        return result.Document;
    }

    // A minimal document that passes validation, for the tests that then break one thing.
    glass::LayoutDocument MinimalDocument()
    {
        glass::LayoutDocument document{};

        document.Name = L"Minimal";
        document.PageWidth = 1280;
        document.PageHeight = 800;
        document.CanvasWidth = 1280;
        document.CanvasHeight = 800;

        glass::Page page{};
        page.Id = L"p1";
        page.Name = L"One";

        glass::Control control{};
        control.Id = L"c1";
        control.Kind = glass::ControlKind::Knob;
        control.Width = 56;
        control.Height = 56;

        page.Controls.push_back(control);
        document.Pages.push_back(page);

        return document;
    }
}

void LayoutDocumentTests::ReadsAHandAuthoredLayout()
{
    auto const document = LoadHandAuthored();

    VERIFY_ARE_EQUAL(std::wstring{ L"Hand Authored" }, document.Name);
    VERIFY_ARE_EQUAL(1280, document.PageWidth);
    VERIFY_ARE_EQUAL(800, document.PageHeight);

    // The canvas is deliberately larger than the page in this file. If the reader quietly
    // clamped it to the page, growing and shrinking would stop working.
    VERIFY_ARE_EQUAL(1600, document.CanvasWidth);
    VERIFY_ARE_EQUAL(1000, document.CanvasHeight);

    VERIFY_IS_TRUE(document.Scale == glass::ScaleMode::FitToScreen);
    VERIFY_IS_TRUE(document.FullScreenButtonCorner == glass::ScreenCorner::BottomLeft);
    VERIFY_IS_TRUE(document.Tempo.Kind == glass::TempoSourceKind::FollowIncomingClock);
    VERIFY_ARE_EQUAL(128.0, document.Tempo.BeatsPerMinute);

    VERIFY_ARE_EQUAL(size_t{ 1 }, document.Pages.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, document.ControlCount());

    auto const* fader = document.FindControl(L"fader-1");
    VERIFY_IS_NOT_NULL(fader);
    VERIFY_IS_TRUE(fader->Kind == glass::ControlKind::Fader);
    VERIFY_IS_TRUE(fader->Pickup == glass::PickupMode::Catch);
    VERIFY_ARE_EQUAL(0.25, fader->DefaultValue);
    VERIFY_IS_TRUE(fader->SendsValueOnStart);
    VERIFY_IS_TRUE(fader->Feedback.Enabled);
}

void LayoutDocumentTests::ReadsMessagesAndSystemExclusive()
{
    auto const document = LoadHandAuthored();

    auto const* pad = document.FindControl(L"pad-1");
    VERIFY_IS_NOT_NULL(pad);
    VERIFY_ARE_EQUAL(size_t{ 2 }, pad->Messages.size());

    VERIFY_IS_TRUE(pad->Messages[0].Kind == glass::MessageKind::Sequence);
    VERIFY_ARE_EQUAL(std::wstring{ L"Intro" }, pad->Messages[0].SequenceName);

    // F0 7E 7F 06 01 F7 - a universal identity request, which is the shortest real world blob
    // anyone would put on a pad.
    auto const& sysex = pad->Messages[1].SystemExclusive;
    VERIFY_ARE_EQUAL(size_t{ 6 }, sysex.size());
    VERIFY_ARE_EQUAL(uint8_t{ 0xF0 }, sysex[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x7E }, sysex[1]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x06 }, sysex[3]);
    VERIFY_ARE_EQUAL(uint8_t{ 0xF7 }, sysex[5]);
}

void LayoutDocumentTests::ReadsTheDeviceTableThroughTheSharedMatchCriteria()
{
    auto const document = LoadHandAuthored();

    VERIFY_ARE_EQUAL(size_t{ 2 }, document.Devices.size());

    auto const* desk = document.FindDevice(L"Desk");
    VERIFY_IS_NOT_NULL(desk);
    VERIFY_IS_TRUE(desk->MatchMode == midiapp::EndpointMatchMode::UsbVendorAndProduct);
    VERIFY_IS_TRUE(desk->SendsBeatClock);

    // The criteria come back through the shipped service configuration type, which is the whole
    // point of storing them in its shape rather than in one of our own.
    VERIFY_ARE_EQUAL(uint16_t{ 1234 }, desk->Match.UsbVendorId);
    VERIFY_ARE_EQUAL(uint16_t{ 99 }, desk->Match.UsbProductId);
    VERIFY_ARE_EQUAL(std::wstring{ L"Big Desk" }, desk->Match.TransportSuppliedEndpointName);

    auto const* synth = document.FindDevice(L"Synth");
    VERIFY_IS_NOT_NULL(synth);
    VERIFY_IS_TRUE(synth->MatchMode == midiapp::EndpointMatchMode::EndpointDeviceId);
    VERIFY_IS_FALSE(synth->Match.EndpointDeviceId.empty());
}

void LayoutDocumentTests::ReadsSequenceSteps()
{
    auto const document = LoadHandAuthored();

    VERIFY_ARE_EQUAL(size_t{ 1 }, document.Sequences.size());

    auto const& steps = document.Sequences[0].Steps;
    VERIFY_ARE_EQUAL(size_t{ 5 }, steps.size());

    VERIFY_IS_TRUE(steps[0].Kind == glass::SequenceStepKind::RepeatBlockStart);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, steps[0].RepeatCount);

    VERIFY_IS_TRUE(steps[2].Kind == glass::SequenceStepKind::Wait);
    VERIFY_ARE_EQUAL(uint32_t{ 250 }, steps[2].WaitMilliseconds);

    VERIFY_IS_TRUE(steps[4].Kind == glass::SequenceStepKind::SetControlValue);
    VERIFY_ARE_EQUAL(std::wstring{ L"fader-1" }, steps[4].TargetControlId);
    VERIFY_ARE_EQUAL(0.75, steps[4].TargetValue);
}

void LayoutDocumentTests::WritingTheSameDocumentTwiceProducesTheSameBytes()
{
    auto const document = LoadHandAuthored();

    auto const first = glass::WriteLayoutToJson(document);
    auto const second = glass::WriteLayoutToJson(document);

    VERIFY_IS_FALSE(first.empty());

    // A JsonObject is a map and gives its keys back in whatever order it likes. If this file were
    // written through Stringify, two saves of an untouched layout could differ, which would make
    // the file undiffable and this whole test meaningless.
    VERIFY_ARE_EQUAL(first, second);
}

void LayoutDocumentTests::ReadingBackWhatWasWrittenChangesNothing()
{
    auto const original = LoadHandAuthored();

    auto const once = glass::WriteLayoutToJson(original);

    auto const reread = glass::ReadLayoutFromJson(once);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const twice = glass::WriteLayoutToJson(reread.Document);

    // The real round trip property: what the app writes, the app reads back to the same thing.
    // A hand-authored file is allowed to differ on whitespace and key order; a written one is not.
    VERIFY_ARE_EQUAL(once, twice);

    Log::Comment(String().Format(L"Canonical form is %zu characters.", once.size()));
}

void LayoutDocumentTests::WholeNumbersDoNotGrowADecimalPoint()
{
    auto const document = LoadHandAuthored();
    auto const text = glass::WriteLayoutToJson(document);

    // x was 48 in the hand-authored file. Written as "48.0" it would read back the same but the
    // file would churn on every save, which is exactly what the number formatter exists to stop.
    VERIFY_IS_TRUE(text.find(L"\"x\": 48\n") != std::wstring::npos ||
        text.find(L"\"x\": 48,") != std::wstring::npos);

    VERIFY_IS_TRUE(text.find(L"48.0") == std::wstring::npos);

    // and a real fraction still survives
    VERIFY_IS_TRUE(text.find(L"0.25") != std::wstring::npos);
}

// ---- overrides of the theme ----

void LayoutDocumentTests::AControlThatAgreesWithItsThemeWritesNoOverrides()
{
    auto const document = LoadHandAuthored();
    auto const text = glass::WriteLayoutToJson(document);

    // An override that defers to the theme is the absence of an override. Writing "useTheme"
    // three times on every control would put dead weight in every file for no reader's benefit,
    // and would make a diff of a real edit impossible to find.
    VERIFY_IS_TRUE(text.find(L"\"style\"") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"labelPlaced\"") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"showValue\"") == std::wstring::npos);
}

void LayoutDocumentTests::OverridesOfTheThemeSurviveARoundTrip()
{
    auto document = LoadHandAuthored();

    VERIFY_IS_GREATER_THAN(document.Pages.size(), size_t{ 0 });
    VERIFY_IS_GREATER_THAN(document.Pages[0].Controls.size(), size_t{ 0 });

    auto& control = document.Pages[0].Controls[0];

    control.Style = glass::ControlStyleOverride::Outline;
    control.LabelPlaced = glass::LabelPlacementOverride::None;
    control.ShowValue = glass::ShowValueOverride::Always;

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"\"style\": \"outline\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"labelPlaced\": \"none\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"showValue\": \"always\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.Pages[0].Controls[0];

    VERIFY_IS_TRUE(back.Style == glass::ControlStyleOverride::Outline);
    VERIFY_IS_TRUE(back.LabelPlaced == glass::LabelPlacementOverride::None);
    VERIFY_IS_TRUE(back.ShowValue == glass::ShowValueOverride::Always);

    // Writing what was read produces the same bytes, which is what keeps a save from looking
    // like an edit.
    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::KeepsFieldsFromANewerVersion()
{
    auto const result = glass::ReadLayoutFromJson(glasstests::LayoutFromANewerVersion());
    VERIFY_IS_TRUE(result.Succeeded);

    auto const text = glass::WriteLayoutToJson(result.Document);

    // An older build opening a newer file keeps what it does not understand rather than silently
    // dropping it. Losing these would quietly destroy the customer's work on the machine that
    // could least explain why.
    VERIFY_IS_TRUE(text.find(L"somethingThisBuildHasNeverHeardOf") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"alpha\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"deep\"") != std::wstring::npos);

    // and the version it announced is written back as it was found, not lowered to ours
    VERIFY_IS_TRUE(text.find(L"\"fileVersion\": 99") != std::wstring::npos);
}

void LayoutDocumentTests::SaysWhenAFileIsFromANewerVersion()
{
    auto const newer = glass::ReadLayoutFromJson(glasstests::LayoutFromANewerVersion());
    VERIFY_IS_TRUE(newer.Succeeded);
    VERIFY_IS_TRUE(newer.IsFromNewerVersion);

    auto const ours = glass::ReadLayoutFromJson(glasstests::HandAuthoredLayout());
    VERIFY_IS_TRUE(ours.Succeeded);
    VERIFY_IS_FALSE(ours.IsFromNewerVersion);
}

void LayoutDocumentTests::UnknownFieldsSurviveAtEveryLevel()
{
    auto const result = glass::ReadLayoutFromJson(glasstests::LayoutFromANewerVersion());
    VERIFY_IS_TRUE(result.Succeeded);

    auto const text = glass::WriteLayoutToJson(result.Document);

    // Document level, page level, control level and message level each keep their own leftovers.
    // Keeping only the document level would be the easy mistake and would lose the most.
    VERIFY_IS_TRUE(text.find(L"somethingThisBuildHasNeverHeardOf") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"futurePageProperty") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"holographicProjection") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"quantumEntanglement") != std::wstring::npos);

    // and they still survive a second trip
    auto const again = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(again.Succeeded);

    auto const twice = glass::WriteLayoutToJson(again.Document);
    VERIFY_ARE_EQUAL(text, twice);
}

void LayoutDocumentTests::RejectsSomethingThatIsNotJson()
{
    VERIFY_IS_FALSE(glass::ReadLayoutFromJson(L"").Succeeded);
    VERIFY_IS_FALSE(glass::ReadLayoutFromJson(L"this is not json").Succeeded);
    VERIFY_IS_FALSE(glass::ReadLayoutFromJson(L"[1,2,3]").Succeeded);
    VERIFY_IS_FALSE(glass::ReadLayoutFromJson(L"{\"unterminated\": ").Succeeded);
}

void LayoutDocumentTests::SurvivesAHostileFile()
{
    auto const result = glass::ReadLayoutFromJson(glasstests::HostileLayout());

    // It parses as JSON, so it loads. What matters is that nothing in it reached the model with a
    // value the rest of the app would have to defend against.
    VERIFY_IS_TRUE(result.Succeeded);

    auto const& document = result.Document;

    VERIFY_IS_TRUE(document.PageWidth > 0);
    VERIFY_IS_TRUE(document.PageHeight > 0);
    VERIFY_IS_TRUE(document.Scale == glass::ScaleMode::ActualSize);
    VERIFY_IS_TRUE(document.Tempo.Kind == glass::TempoSourceKind::Internal);
    VERIFY_IS_TRUE(document.Tempo.BeatsPerMinute >= 1.0);
    VERIFY_ARE_EQUAL(size_t{ 0 }, document.Devices.size());

    VERIFY_ARE_EQUAL(size_t{ 1 }, document.Pages.size());

    auto const* control = document.FindControl(L"c1");
    VERIFY_IS_NOT_NULL(control);

    VERIFY_IS_TRUE(control->Kind == glass::ControlKind::Knob);
    VERIFY_IS_TRUE(control->HueSlot >= glass::LiteralHue && control->HueSlot < glass::HueSlotCount);
    VERIFY_IS_TRUE(control->DefaultValue >= 0.0 && control->DefaultValue <= 1.0);

    // the two entries that were not objects are dropped rather than guessed at
    VERIFY_ARE_EQUAL(size_t{ 2 }, control->Messages.size());

    for (auto const& message : control->Messages)
    {
        VERIFY_IS_TRUE(message.GroupIndex >= glass::AllGroups && message.GroupIndex < glass::MaximumGroupCount);
        VERIFY_IS_TRUE(message.ChannelIndex >= 0 && message.ChannelIndex <= 15);
        VERIFY_IS_TRUE(message.RawWords.size() <= 4);
    }

    // and it still writes out as well formed JSON that reads back
    auto const text = glass::WriteLayoutToJson(document);
    VERIFY_IS_TRUE(glass::ReadLayoutFromJson(text).Succeeded);
}

void LayoutDocumentTests::BoundsStringsFromAFile()
{
    auto const result = glass::ReadLayoutFromJson(glasstests::HostileLayout());
    VERIFY_IS_TRUE(result.Succeeded);

    // 4000 characters went in. A name that long in a title bar or a card is a denial of service
    // against the person looking at it.
    VERIFY_IS_LESS_THAN_OR_EQUAL(result.Document.Name.size(), midiapp::MaximumStringLength);
}

void LayoutDocumentTests::RejectsSystemExclusiveThatIsNotHex()
{
    auto const result = glass::ReadLayoutFromJson(glasstests::HostileLayout());
    VERIFY_IS_TRUE(result.Succeeded);

    auto const* control = result.Document.FindControl(L"c1");
    VERIFY_IS_NOT_NULL(control);

    // "F0ZZ7F06" is half plausible, which is the dangerous kind. One bad character means the
    // whole blob is dropped rather than partly believed, because a truncated system exclusive
    // message sent to a synth is how a device gets bricked.
    VERIFY_ARE_EQUAL(size_t{ 0 }, control->Messages[0].SystemExclusive.size());
}

void LayoutDocumentTests::AcceptsAValidDocument()
{
    auto const issues = glass::Validate(MinimalDocument());

    for (auto const& issue : issues)
    {
        Log::Error(String().Format(L"unexpected: %s", issue.Detail.c_str()));
    }

    VERIFY_ARE_EQUAL(size_t{ 0 }, issues.size());

    // and the file a person wrote is valid too
    VERIFY_ARE_EQUAL(size_t{ 0 }, glass::Validate(LoadHandAuthored()).size());
}

void LayoutDocumentTests::CatchesAMessageSentToAnUnknownDevice()
{
    auto document = MinimalDocument();

    glass::ControlMessage message{};
    message.DeviceName = L"A Device That Is Not In The Table";

    document.Pages[0].Controls[0].Messages.push_back(message);

    auto const issues = glass::Validate(document);
    VERIFY_ARE_EQUAL(size_t{ 1 }, issues.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"c1" }, issues[0].ObjectId);
}

void LayoutDocumentTests::CatchesDuplicateControlIds()
{
    auto document = MinimalDocument();

    auto duplicate = document.Pages[0].Controls[0];
    document.Pages[0].Controls.push_back(duplicate);

    auto const issues = glass::Validate(document);
    VERIFY_ARE_EQUAL(size_t{ 1 }, issues.size());
}

void LayoutDocumentTests::CatchesAnUnclosedRepeatBlock()
{
    auto document = MinimalDocument();

    glass::Sequence sequence{};
    sequence.Name = L"Broken";

    glass::SequenceStep start{};
    start.Kind = glass::SequenceStepKind::RepeatBlockStart;
    sequence.Steps.push_back(start);

    document.Sequences.push_back(sequence);

    auto const issues = glass::Validate(document);
    VERIFY_ARE_EQUAL(size_t{ 1 }, issues.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Broken" }, issues[0].ObjectId);
}

void LayoutDocumentTests::CatchesASequenceThatDoesNotExist()
{
    auto document = MinimalDocument();

    glass::ControlMessage message{};
    message.Kind = glass::MessageKind::Sequence;
    message.SequenceName = L"Never Written";

    document.Pages[0].Controls[0].Messages.push_back(message);

    auto const issues = glass::Validate(document);
    VERIFY_ARE_EQUAL(size_t{ 1 }, issues.size());
}

void LayoutDocumentTests::FindsControlsOutsideThePage()
{
    auto document = MinimalDocument();

    document.CanvasWidth = 4000;
    document.CanvasHeight = 4000;

    glass::Control stray{};
    stray.Id = L"stray";
    stray.X = 3000;
    stray.Y = 10;
    stray.Width = 56;
    stray.Height = 56;

    // half on, half off. Clamping this one inside would be the tidy answer and the wrong one,
    // because then shrinking a page could never be undone.
    glass::Control straddling{};
    straddling.Id = L"straddling";
    straddling.X = document.PageWidth - 20;
    straddling.Y = 10;
    straddling.Width = 56;
    straddling.Height = 56;

    document.Pages[0].Controls.push_back(stray);
    document.Pages[0].Controls.push_back(straddling);

    auto const outside = document.ControlsOutsidePage();

    VERIFY_ARE_EQUAL(size_t{ 2 }, outside.size());
}
