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
#include "HexText.h"
#include "JsonText.h"
#include "ThemeModel.h"

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

    // The file still says sendsBeatClock. It never did anything, so it is not kept.
    VERIFY_IS_TRUE(desk->Unknown == nullptr || !desk->Unknown.HasKey(L"sendsBeatClock"));

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

void LayoutDocumentTests::AnOldThemeNameReadsAsTheNewOne()
{
    auto document = LoadHandAuthored();

    document.ThemeName = L"Amber Console";

    auto const reread = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(document));

    VERIFY_IS_TRUE(reread.Succeeded);
    VERIFY_ARE_EQUAL(std::wstring{ L"Terminal Amber" }, reread.Document.ThemeName);
}

void LayoutDocumentTests::ALabelBoxSurvivesARoundTrip()
{
    auto document = LoadHandAuthored();

    auto& control = document.Pages[0].Controls[0];

    control.LabelPlaced = glass::LabelPlacementOverride::Custom;
    control.LabelLook.BoxX = -12.5;
    control.LabelLook.BoxY = 70.0;
    control.LabelLook.BoxWidth = 96.0;
    control.LabelLook.BoxHeight = 34.0;

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"\"labelPlaced\": \"custom\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"boxWidth\": 96") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.Pages[0].Controls[0];

    VERIFY_IS_TRUE(back.LabelPlaced == glass::LabelPlacementOverride::Custom);
    VERIFY_IS_TRUE(back.LabelLook.HasBox());
    VERIFY_ARE_EQUAL(-12.5, back.LabelLook.BoxX);
    VERIFY_ARE_EQUAL(70.0, back.LabelLook.BoxY);
    VERIFY_ARE_EQUAL(96.0, back.LabelLook.BoxWidth);
    VERIFY_ARE_EQUAL(34.0, back.LabelLook.BoxHeight);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::ALabelWithNoBoxWritesNoBox()
{
    auto document = LoadHandAuthored();

    // A box of nothing is the absence of a box. Writing four zeroes on every control would put
    // dead weight in every file and make a real edit impossible to find in a diff.
    document.Pages[0].Controls[0].LabelLook.Italic = true;

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"\"italic\": true") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"boxWidth\"") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"boxX\"") == std::wstring::npos);
}

void LayoutDocumentTests::ACustomPlacementWithNoBoxFallsBackToTheTheme()
{
    // A file that says the label is where the customer put it, but does not say where, is a file
    // that says one thing and carries another. Drawing nothing would be worse than deferring.
    auto const result = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "c", "kind": "knob", "label": "Knob", "labelPlaced": "custom" } ] } ] })");

    VERIFY_IS_TRUE(result.Succeeded);

    auto const& control = result.Document.Pages[0].Controls[0];

    VERIFY_IS_TRUE(control.LabelPlaced == glass::LabelPlacementOverride::UseTheme);
    VERIFY_IS_FALSE(control.LabelLook.HasBox());
}

void LayoutDocumentTests::ALabelBoxFromAFileIsBounded()
{
    // A stranger's file must not be able to ask for a text block the size of a wall.
    auto const result = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "c", "kind": "knob", "label": "Knob", "labelPlaced": "custom",
              "labelStyle": { "boxX": -1e12, "boxY": 1e12, "boxWidth": 1e12, "boxHeight": 1e12 } } ] } ] })");

    VERIFY_IS_TRUE(result.Succeeded);

    auto const& look = result.Document.Pages[0].Controls[0].LabelLook;

    VERIFY_ARE_EQUAL(-glass::MaximumLabelBoxExtent, look.BoxX);
    VERIFY_ARE_EQUAL(glass::MaximumLabelBoxExtent, look.BoxY);
    VERIFY_ARE_EQUAL(glass::MaximumLabelBoxExtent, look.BoxWidth);
    VERIFY_ARE_EQUAL(glass::MaximumLabelBoxExtent, look.BoxHeight);
}

// ---- lines, and what a control is printed on ----

void LayoutDocumentTests::ALineSurvivesARoundTrip()
{
    auto document = MinimalDocument();

    glass::Control line{};
    line.Id = L"rule";
    line.Kind = glass::ControlKind::Line;
    line.X = 20;
    line.Y = 100;
    line.Width = 240;
    line.Height = 8;
    line.Line.Thickness = 3;
    line.Line.Color = L"#FF4B36";
    line.Line.Ends = glass::LineEnds::Faded;

    document.Pages[0].Controls.push_back(line);

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"\"kind\": \"line\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"ends\": \"faded\"") != std::wstring::npos);

    // A knob has no line to write, so it writes none.
    VERIFY_ARE_EQUAL(text.find(L"\"line\": {"), text.rfind(L"\"line\": {"));

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const* back = reread.Document.FindControl(L"rule");
    VERIFY_IS_NOT_NULL(back);

    VERIFY_IS_TRUE(back->Kind == glass::ControlKind::Line);
    VERIFY_ARE_EQUAL(3.0, back->Line.Thickness);
    VERIFY_ARE_EQUAL(std::wstring{ L"#FF4B36" }, back->Line.Color);
    VERIFY_IS_TRUE(back->Line.Ends == glass::LineEnds::Faded);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::ALineFromAFileIsBounded()
{
    // A stranger's file must not be able to ask for a line thicker than the page, and a finish
    // this build has never heard of falls back to the theme's rather than failing the file.
    auto const result = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "line", "line": { "thickness": 1e9, "ends": "sparkly", "glow": 3 } },
            { "id": "b", "kind": "line", "line": { "thickness": -5 } } ] } ] })");

    VERIFY_IS_TRUE(result.Succeeded);

    auto const& controls = result.Document.Pages[0].Controls;

    VERIFY_ARE_EQUAL(glass::MaximumLineThickness, controls[0].Line.Thickness);
    VERIFY_IS_TRUE(controls[0].Line.Ends == glass::LineEnds::UseTheme);
    VERIFY_ARE_EQUAL(glass::MinimumLineThickness, controls[1].Line.Thickness);

    // and what it did not understand goes back out with it
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(result.Document).find(L"\"glow\"") != std::wstring::npos);
}

void LayoutDocumentTests::ASectionKnowsWhatIsPrintedOnIt()
{
    glass::Theme theme{};
    theme.PanelFill = glass::PanelFillStyle::Color;
    theme.PanelColor = { 0xCA, 0xC5, 0xAC, 255 };

    auto const make = [](wchar_t const* id, glass::ControlKind kind, double x, double y, double width, double height)
        {
            glass::Control control{};
            control.Id = id;
            control.Kind = kind;
            control.X = x;
            control.Y = y;
            control.Width = width;
            control.Height = height;

            return control;
        };

    glass::Page page{};

    // In drawing order: a section, an inset inside it, an outlined frame that fills nothing, and
    // then the controls.
    page.Controls.push_back(make(L"section", glass::ControlKind::Panel, 0, 0, 400, 300));
    page.Controls.push_back(make(L"inset", glass::ControlKind::Panel, 40, 60, 200, 160));

    auto frame = make(L"frame", glass::ControlKind::Panel, 500, 0, 300, 300);
    frame.Style = glass::ControlStyleOverride::Outline;
    page.Controls.push_back(frame);

    page.Controls.push_back(make(L"onInset", glass::ControlKind::Knob, 100, 100, 56, 56));
    page.Controls.push_back(make(L"onSection", glass::ControlKind::Knob, 300, 200, 56, 56));
    page.Controls.push_back(make(L"inFrame", glass::ControlKind::Knob, 600, 100, 56, 56));

    auto const panels = glass::PanelFootprints(page, theme);

    // The outline fills nothing, so nothing is printed on it.
    VERIFY_ARE_EQUAL(size_t{ 2 }, panels.size());
    VERIFY_IS_FALSE(panels[0].IsInset);
    VERIFY_IS_TRUE(panels[1].IsInset);

    VERIFY_IS_TRUE(glass::SurfaceAt(panels, 128, 128, 3) == glass::PrintSurface::Inset);
    VERIFY_IS_TRUE(glass::SurfaceAt(panels, 328, 228, 4) == glass::PrintSurface::Section);
    VERIFY_IS_TRUE(glass::SurfaceAt(panels, 628, 128, 5) == glass::PrintSurface::Deck);

    // Only what was drawn before a control is under it. The section is not under itself, but its
    // own name, looked up one step later, is printed on it.
    VERIFY_IS_TRUE(glass::SurfaceAt(panels, 200, 10, 0) == glass::PrintSurface::Deck);
    VERIFY_IS_TRUE(glass::SurfaceAt(panels, 200, 10, 1) == glass::PrintSurface::Section);

    // A theme that fills no sections has nothing to print on at all.
    theme.PanelFill = glass::PanelFillStyle::None;
    VERIFY_ARE_EQUAL(size_t{ 0 }, glass::PanelFootprints(page, theme).size());

    // but one section a customer asked to be solid is still filled
    page.Controls[0].Style = glass::ControlStyleOverride::Solid;
    VERIFY_ARE_EQUAL(size_t{ 1 }, glass::PanelFootprints(page, theme).size());
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

void LayoutDocumentTests::AKindThisVersionDoesNotKnowMeansANewerVersion()
{
    // A control of a kind that came later reads as a placeholder, so the layout still opens.
    auto const control = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "hologram" } ] } ] })");

    VERIFY_IS_TRUE(control.Succeeded);
    VERIFY_IS_TRUE(control.IsFromNewerVersion);
    VERIFY_IS_TRUE(control.Document.IsFromNewerVersion);
    VERIFY_IS_TRUE(control.Document.Pages[0].Controls[0].Kind == glass::ControlKind::Placeholder);

    // and the same for a message a newer version can send
    auto const message = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "knob", "messages": [ { "kind": "quantumChange" } ] } ] } ] })");

    VERIFY_IS_TRUE(message.Succeeded);
    VERIFY_IS_TRUE(message.Document.IsFromNewerVersion);
    VERIFY_IS_TRUE(message.Document.Pages[0].Controls[0].Messages[0].Kind == glass::MessageKind::Unrecognized);

    // An old name for a kind this version knows, and a retired row, are neither. Read right
    // after the two above, so nothing they set is carried over.
    auto const ours = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
            { "id": "a", "kind": "encoder", "messages": [ { "kind": "holdLayer" }, { "kind": "controlChange" } ] } ] } ] })");

    VERIFY_IS_TRUE(ours.Succeeded);
    VERIFY_IS_FALSE(ours.IsFromNewerVersion);
    VERIFY_IS_FALSE(ours.Document.IsFromNewerVersion);
}

void LayoutDocumentTests::AControlThisVersionDoesNotKnowIsWrittenBackAsItCame()
{
    std::wstring const json = LR"({ "fileVersion": 1, "name": "T", "pages": [ { "id": "p", "name": "P", "controls": [
        { "id": "knob", "kind": "knob", "label": "Cutoff", "x": 10, "y": 10, "width": 56, "height": 56 },
        { "id": "holo", "kind": "hologram", "label": "From the future", "x": 360.5, "y": 112, "width": 240,
          "height": 200, "keyboardOrder": 3, "hueSlot": 1, "shimmer": 7,
          "beam": { "color": "#FFFFFF", "angles": [ 1, 2.5 ] },
          "messages": [ { "trigger": "changes", "kind": "controlChange", "device": "Synth", "number": 74 } ],
          "feedback": { "kind": "controlChange", "device": "Synth", "number": 74 } } ] } ] })";

    auto const read = glass::ReadLayoutFromJson(json);
    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.IsFromNewerVersion);

    auto const* holo = read.Document.FindControl(L"holo");
    VERIFY_IS_NOT_NULL(holo);

    // Drawn where the file puts it, under its own name, with nothing to send or listen for.
    VERIFY_IS_TRUE(holo->Kind == glass::ControlKind::Placeholder);
    VERIFY_ARE_EQUAL(std::wstring{ L"From the future" }, holo->Label);
    VERIFY_ARE_EQUAL(360.5, holo->X);
    VERIFY_ARE_EQUAL(112.0, holo->Y);
    VERIFY_ARE_EQUAL(240.0, holo->Width);
    VERIFY_ARE_EQUAL(200.0, holo->Height);
    VERIFY_ARE_EQUAL(3, holo->KeyboardOrder);
    VERIFY_IS_TRUE(holo->Locked);
    VERIFY_IS_TRUE(holo->Messages.empty());
    VERIFY_IS_FALSE(holo->Feedback.Enabled);
    VERIFY_IS_FALSE(holo->SendsValueOnStart);

    // Written back, its object is the one the file had, key for key and value for value.
    auto const text = glass::WriteLayoutToJson(read.Document);

    auto const controlsIn = [](std::wstring const& source)
        {
            return winrt::Windows::Data::Json::JsonObject::Parse(winrt::hstring{ source })
                .GetNamedArray(L"pages").GetObjectAt(0).GetNamedArray(L"controls");
        };

    VERIFY_ARE_EQUAL(
        glass::CanonicalJson(controlsIn(json).GetObjectAt(1), 0),
        glass::CanonicalJson(controlsIn(text).GetObjectAt(1), 0));

    // The control beside it is written the usual way, and a second trip changes nothing.
    VERIFY_ARE_EQUAL(std::wstring{ L"knob" }, std::wstring{ controlsIn(text).GetObjectAt(0).GetNamedString(L"kind") });

    auto const again = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(again.Succeeded);
    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(again.Document));
}

void LayoutDocumentTests::AMessageThisVersionDoesNotKnowIsWrittenBackAsItCame()
{
    std::wstring const json = LR"({ "fileVersion": 1, "name": "T", "devices": [ { "name": "Synth" } ],
        "pages": [ { "id": "p", "name": "P", "controls": [
        { "id": "knob", "kind": "knob",
          "messages": [
            { "trigger": "changes", "kind": "controlChange", "device": "Synth", "number": 74 },
            { "trigger": "turnsOn", "kind": "noteAttribute", "device": "Synth", "number": 60, "shape": { "a": 1.5 } } ],
          "feedback": { "enabled": true, "kind": "quantumChange", "device": "Synth", "glow": 3 } } ] } ],
        "sequences": [ { "name": "S", "mode": "once", "steps": [
          { "kind": "sendMessage", "message": { "kind": "noteAttribute", "device": "Synth", "number": 61 } } ] } ] })";

    auto const read = glass::ReadLayoutFromJson(json);
    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.IsFromNewerVersion);

    auto const* knob = read.Document.FindControl(L"knob");
    VERIFY_IS_NOT_NULL(knob);
    VERIFY_ARE_EQUAL(size_t{ 2 }, knob->Messages.size());

    // The row it knows is read as usual. The others are kept, and nothing here sends or follows them.
    VERIFY_IS_TRUE(knob->Messages[0].Kind == glass::MessageKind::ControlChange);
    VERIFY_IS_TRUE(knob->Messages[1].Kind == glass::MessageKind::Unrecognized);
    VERIFY_IS_TRUE(knob->Messages[1].Trigger == glass::MessageTrigger::TurnsOn);
    VERIFY_IS_TRUE(knob->Feedback.Kind == glass::MessageKind::Unrecognized);
    VERIFY_IS_FALSE(knob->Feedback.Enabled);
    VERIFY_IS_TRUE(read.Document.Sequences[0].Steps[0].Message.Kind == glass::MessageKind::Unrecognized);

    auto const text = glass::WriteLayoutToJson(read.Document);

    auto const canonical = [](std::wstring const& source, int32_t part)
        {
            auto const root = winrt::Windows::Data::Json::JsonObject::Parse(winrt::hstring{ source });
            auto const control = root.GetNamedArray(L"pages").GetObjectAt(0).GetNamedArray(L"controls").GetObjectAt(0);

            switch (part)
            {
            case 0: return glass::CanonicalJson(control.GetNamedArray(L"messages").GetObjectAt(1), 0);
            case 1: return glass::CanonicalJson(control.GetNamedObject(L"feedback"), 0);
            default:
                return glass::CanonicalJson(
                    root.GetNamedArray(L"sequences").GetObjectAt(0).GetNamedArray(L"steps").GetObjectAt(0).GetNamedObject(L"message"), 0);
            }
        };

    for (int32_t part = 0; part < 3; ++part)
    {
        VERIFY_ARE_EQUAL(canonical(json, part), canonical(text, part));
    }

    auto const again = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(again.Succeeded);
    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(again.Document));
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

void LayoutDocumentTests::SettingsThatNeverWorkedAreDroppedWhenRead()
{
    // Every layout saved before carries the first two, so they must not travel on as unknown keys.
    auto const result = glass::ReadLayoutFromJson(LR"({
        "fileVersion": 1,
        "name": "Old",
        "preferredDisplayId": "DISPLAY1",
        "devices": [ { "name": "Synth", "sendsBeatClock": true } ],
        "pages": [ { "id": "p", "name": "Page", "controls": [
            { "id": "k", "kind": "knob", "label": "Knob", "messages": [
                { "kind": "holdLayer", "targetLayer": "shift" },
                { "kind": "controlChange", "device": "Synth", "number": 7 } ] } ] } ],
        "sequences": [ { "name": "Steps", "steps": [
            { "kind": "holdLayer", "message": { "kind": "controlChange", "targetLayer": "shift" } },
            { "kind": "sendMessage", "message": { "kind": "holdLayer" } },
            { "kind": "wait", "waitMilliseconds": 10 } ] } ] })");

    VERIFY_IS_TRUE(result.Succeeded);

    // A hold layer row or step did nothing. Read as anything else, it would have sent something.
    auto const* knob = result.Document.FindControl(L"k");
    VERIFY_IS_NOT_NULL(knob);
    VERIFY_ARE_EQUAL(size_t{ 1 }, knob->Messages.size());
    VERIFY_IS_TRUE(knob->Messages[0].Kind == glass::MessageKind::ControlChange);
    VERIFY_ARE_EQUAL(uint32_t{ 7 }, knob->Messages[0].Number);

    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Document.Sequences.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Document.Sequences[0].Steps.size());
    VERIFY_IS_TRUE(result.Document.Sequences[0].Steps[0].Kind == glass::SequenceStepKind::Wait);

    auto const text = glass::WriteLayoutToJson(result.Document);

    VERIFY_IS_TRUE(text.find(L"preferredDisplayId") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"sendsBeatClock") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"holdLayer") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"targetLayer") == std::wstring::npos);
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

    // A kind nobody has heard of is a placeholder, and sends nothing whatever its messages say.
    auto const* unknown = document.FindControl(L"c2");
    VERIFY_IS_NOT_NULL(unknown);

    VERIFY_IS_TRUE(unknown->Kind == glass::ControlKind::Placeholder);
    VERIFY_IS_TRUE(unknown->Label.empty());
    VERIFY_IS_TRUE(unknown->KeyboardOrder >= 0);
    VERIFY_IS_TRUE(unknown->Messages.empty());

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

// ---- the background picture ----

void LayoutDocumentTests::ABackgroundPictureSurvivesARoundTrip()
{
    auto document = LoadHandAuthored();

    document.BackgroundImage = L"desk photo.png";
    document.BackgroundFitMode = glass::BackgroundFit::Tiled;

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"\"backgroundImage\": \"desk photo.png\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"backgroundFit\": \"tiled\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);
    VERIFY_IS_TRUE(reread.Succeeded);

    VERIFY_ARE_EQUAL(std::wstring{ L"desk photo.png" }, reread.Document.BackgroundImage);
    VERIFY_IS_TRUE(reread.Document.BackgroundFitMode == glass::BackgroundFit::Tiled);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::AControlPictureSurvivesARoundTrip()
{
    auto document = LoadHandAuthored();

    glass::Control control{};

    control.Id = L"backdrop";
    control.Kind = glass::ControlKind::Image;
    control.Image.FileName = L"stage clip.mp4";
    control.Image.Fit = glass::BackgroundFit::Fill;
    control.Image.Opacity = 0.8;
    control.Image.Loops = false;
    control.Image.Zoom = 2.5;
    control.Image.CenterX = 0.25;
    control.Image.CenterY = 0.75;
    control.Image.TintColor = L"#2E6CC8";
    control.Image.TintStrength = 0.55;
    control.Image.VideoStartSeconds = 2.5;
    control.Image.VideoEndSeconds = 9.75;
    control.Image.AutoPlays = false;
    control.Image.ClickToPlay = true;
    control.Image.ShowsScrubber = true;

    document.Pages[0].Controls.push_back(control);

    auto const text = glass::WriteLayoutToJson(document);
    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.Pages[0].Controls.back().Image;

    VERIFY_ARE_EQUAL(std::wstring{ L"stage clip.mp4" }, back.FileName);
    VERIFY_IS_TRUE(back.Fit == glass::BackgroundFit::Fill);
    VERIFY_IS_FALSE(back.Loops);
    VERIFY_ARE_EQUAL(2.5, back.Zoom);
    VERIFY_ARE_EQUAL(0.25, back.CenterX);
    VERIFY_ARE_EQUAL(0.75, back.CenterY);
    VERIFY_ARE_EQUAL(std::wstring{ L"#2E6CC8" }, back.TintColor);
    VERIFY_ARE_EQUAL(0.55, back.TintStrength);
    VERIFY_ARE_EQUAL(2.5, back.VideoStartSeconds);
    VERIFY_ARE_EQUAL(9.75, back.VideoEndSeconds);
    VERIFY_IS_FALSE(back.AutoPlays);
    VERIFY_IS_TRUE(back.ClickToPlay);
    VERIFY_IS_TRUE(back.ShowsScrubber);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::AVideoFromBeforeTrimmingPlaysWhole()
{
    // A layout saved before a video could be trimmed, clicked or scrubbed.
    auto const reread = glass::ReadLayoutFromJson(LR"({
        "fileVersion": 1,
        "name": "Old clip",
        "pages": [ { "id": "p", "name": "Page", "controls": [
            { "id": "clip", "kind": "image", "label": "Clip", "x": 0, "y": 0, "width": 320, "height": 180,
              "picture": { "file": "stage clip.mp4", "fit": "fill", "loops": true } } ] } ] })");

    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& picture = reread.Document.Pages[0].Controls[0].Image;

    // The whole file, on its own, on a loop, exactly as it always played.
    VERIFY_ARE_EQUAL(0.0, picture.VideoStartSeconds);
    VERIFY_ARE_EQUAL(0.0, picture.VideoEndSeconds);
    VERIFY_IS_TRUE(picture.AutoPlays);
    VERIFY_IS_FALSE(picture.ClickToPlay);
    VERIFY_IS_FALSE(picture.ShowsScrubber);
    VERIFY_IS_TRUE(picture.Loops);

    // And written back without any of the new keys.
    auto const written = glass::WriteLayoutToJson(reread.Document);

    VERIFY_IS_TRUE(written.find(L"startSeconds") == std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"autoPlays") == std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"showsScrubber") == std::wstring::npos);
}

void LayoutDocumentTests::ABackgroundPictureThatIsAPathIsRefused()
{
    // A layout arrives from a stranger. A background that names a path is a way to make this app
    // read a file somewhere else on the PC, so anything that is not a bare file name is dropped
    // whole rather than trimmed into something that looks safe.
    wchar_t const* const hostile[]
    {
        LR"(..\..\Windows\System32\config\SAM)",
        LR"(C:\Users\Someone\secret.png)",
        LR"(\\server\share\thing.png)",
        LR"(sub/dir/thing.png)",
        LR"(..)",
        LR"(nice..name.png)",
    };

    for (auto const* const name : hostile)
    {
        // A backslash has to reach the reader as a backslash, so it is escaped for JSON here.
        // Without this the parser refuses the file and the sanitizer never gets a look, which
        // would make this test pass for the wrong reason.
        std::wstring escaped{};

        for (auto const character : std::wstring{ name })
        {
            if (character == L'\\') { escaped += L'\\'; }
            escaped += character;
        }

        std::wstring json{ LR"({ "fileVersion": 1, "name": "T", "backgroundImage": ")" };
        json += escaped;
        json += LR"(", "pages": [ { "id": "p", "name": "P", "controls": [] } ] })";

        auto const result = glass::ReadLayoutFromJson(json);

        VERIFY_IS_TRUE(result.Succeeded);
        VERIFY_IS_TRUE(result.Document.BackgroundImage.empty());
    }

    // and a plain name still gets through
    auto const good = glass::ReadLayoutFromJson(
        LR"({ "fileVersion": 1, "name": "T", "backgroundImage": "wood.jpg",
              "pages": [ { "id": "p", "name": "P", "controls": [] } ] })");

    VERIFY_IS_TRUE(good.Succeeded);
    VERIFY_ARE_EQUAL(std::wstring{ L"wood.jpg" }, good.Document.BackgroundImage);
}

void LayoutDocumentTests::APictureNamedLikeADeviceIsRefused()
{
    // Windows opens these as devices, with or without an extension after them.
    wchar_t const* const refused[]
    {
        L"CON", L"con.png", L"NUL.tar.gz", L"aux .jpg", L"Prn.mp4", L"CLOCK$.png",
        L"COM1.png", L"com0.png", L"lpt9.jpg", L"LPT0", L"COM\u00B9.png", L"LPT\u00B3",
        L"CONIN$.png", L"conout$",

        // Windows drops these, so the file it finds is not the one the layout names.
        L"photo.png.", L"photo.png ",
    };

    for (auto const* const name : refused)
    {
        VERIFY_IS_TRUE(glass::SanitizeFileName(name).empty(), name);
    }

    // Only starting like a device name is fine.
    wchar_t const* const allowed[]
    {
        L"console.png", L"COM10.png", L"nulled.jpg", L"auxiliary.mp4", L"my con.png", L"lpt.png",
    };

    for (auto const* const name : allowed)
    {
        VERIFY_ARE_EQUAL(std::wstring{ name }, glass::SanitizeFileName(name), name);
    }
}

void LayoutDocumentTests::NoBackgroundPictureWritesNothing()
{
    auto const text = glass::WriteLayoutToJson(LoadHandAuthored());

    // The absence of a picture is the absence of the key, not an empty string and a fit mode
    // nobody chose.
    VERIFY_IS_TRUE(text.find(L"\"backgroundImage\"") == std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"backgroundFit\"") == std::wstring::npos);
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

// ---- hexadecimal in and out ----

void LayoutDocumentTests::HexBytesRoundTrip()
{
    std::vector<uint8_t> const bytes{ 0xF0, 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x02, 0x00, 0x10, 0xF7 };

    auto const text = glass::FormatHexBytes(bytes);
    auto const back = glass::ParseHexBytes(text, glass::MaximumSystemExclusiveBytes);

    VERIFY_ARE_EQUAL(bytes.size(), back.size());

    for (size_t index = 0; index < bytes.size(); ++index)
    {
        VERIFY_ARE_EQUAL(bytes[index], back[index]);
    }
}

void LayoutDocumentTests::HexAcceptsWhatSomebodyWouldPaste()
{
    // A manual, a forum post and a hex editor all write it differently, and somebody pasting one
    // of them should not have to know which form this app would have chosen.
    auto const spaced = glass::ParseHexBytes(L"F0 00 20 6B F7", 64);
    auto const packed = glass::ParseHexBytes(L"F000206BF7", 64);
    auto const commas = glass::ParseHexBytes(L"0xF0, 0x00, 0x20, 0x6B, 0xF7", 64);
    auto const lines = glass::ParseHexBytes(L"F0 00\r\n20 6B\nF7", 64);

    VERIFY_ARE_EQUAL(size_t{ 5 }, spaced.size());
    VERIFY_ARE_EQUAL(size_t{ 5 }, packed.size());
    VERIFY_ARE_EQUAL(size_t{ 5 }, commas.size());
    VERIFY_ARE_EQUAL(size_t{ 5 }, lines.size());

    VERIFY_ARE_EQUAL(uint8_t{ 0x6B }, commas[3]);
    VERIFY_ARE_EQUAL(uint8_t{ 0x6B }, lines[3]);
}

void LayoutDocumentTests::HalfAByteIsRefusedWhole()
{
    // A partly-read dump is worse than none: one bad character in a firmware image can leave a
    // synthesizer unusable.
    VERIFY_IS_TRUE(glass::ParseHexBytes(L"F0 00 2", 64).empty());
}

void LayoutDocumentTests::SomethingThatIsNotHexIsRefusedWhole()
{
    VERIFY_IS_TRUE(glass::ParseHexBytes(L"F0 ZZ 20", 64).empty());
    VERIFY_IS_TRUE(glass::ParseHexBytes(L"the quick brown fox", 64).empty());
}

void LayoutDocumentTests::HexIsBounded()
{
    std::wstring long_{};

    for (int32_t i = 0; i < 100; ++i)
    {
        long_ += L"7F";
    }

    VERIFY_ARE_EQUAL(size_t{ 100 }, glass::ParseHexBytes(long_, 100).size());
    VERIFY_IS_TRUE(glass::ParseHexBytes(long_, 99).empty());
}

void LayoutDocumentTests::HexWordsRoundTrip()
{
    std::vector<uint32_t> const words{ 0x40903C00, 0xFFFF0000 };

    auto const text = glass::FormatHexWords(words);

    VERIFY_ARE_EQUAL(std::wstring{ L"40903C00 FFFF0000" }, text);

    auto const back = glass::ParseHexWords(text, 4);

    VERIFY_ARE_EQUAL(size_t{ 2 }, back.size());
    VERIFY_ARE_EQUAL(words[0], back[0]);
    VERIFY_ARE_EQUAL(words[1], back[1]);

    // Seven digits is not a word, and a word that is not whole is not a word.
    VERIFY_IS_TRUE(glass::ParseHexWords(L"40903C0", 4).empty());
    VERIFY_IS_TRUE(glass::ParseHexWords(L"40903C00 FFFF0000 00000000 11111111 22222222", 4).empty());
}

void LayoutDocumentTests::OneHexNumberReadsAndWrites()
{
    uint32_t value{ 99 };

    VERIFY_IS_TRUE(glass::TryParseHexNumber(L"02", 255, value));
    VERIFY_ARE_EQUAL(2u, value);

    VERIFY_IS_TRUE(glass::TryParseHexNumber(L"0x1a2B", 65535, value));
    VERIFY_ARE_EQUAL(0x1A2Bu, value);

    // Empty is zero, so clearing a field turns the attribute off.
    VERIFY_IS_TRUE(glass::TryParseHexNumber(L"", 255, value));
    VERIFY_ARE_EQUAL(0u, value);

    VERIFY_IS_FALSE(glass::TryParseHexNumber(L"ZZ", 255, value));
    VERIFY_IS_FALSE(glass::TryParseHexNumber(L"100", 255, value));
    VERIFY_IS_FALSE(glass::TryParseHexNumber(L"123456789", 0xFFFFFFFF, value));

    VERIFY_ARE_EQUAL(std::wstring{ L"02" }, glass::FormatHexNumber(2, 2));
    VERIFY_ARE_EQUAL(std::wstring{ L"1A2B" }, glass::FormatHexNumber(0x1A2B, 4));
    VERIFY_ARE_EQUAL(std::wstring{ L"0000" }, glass::FormatHexNumber(0, 4));
}

void LayoutDocumentTests::AnRpnOrNrpnNumberIsABankAndAnIndex()
{
    VERIFY_IS_TRUE(glass::HasBankAndIndex(glass::MessageKind::RegisteredController));
    VERIFY_IS_TRUE(glass::HasBankAndIndex(glass::MessageKind::AssignedController));
    VERIFY_IS_FALSE(glass::HasBankAndIndex(glass::MessageKind::ControlChange));

    // NRPN 3:17 the way a manual prints it, which MIDI 1.0 sends as CC 99 = 3 and CC 98 = 17.
    auto const number = glass::ControllerNumber(3, 17);

    VERIFY_ARE_EQUAL(uint32_t{ 401 }, number);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, glass::ControllerBank(number));
    VERIFY_ARE_EQUAL(uint32_t{ 17 }, glass::ControllerIndex(number));
    VERIFY_ARE_EQUAL(std::wstring{ L"3:17" }, glass::FormatMessageNumber(glass::MessageKind::AssignedController, number));
    VERIFY_ARE_EQUAL(std::wstring{ L"74" }, glass::FormatMessageNumber(glass::MessageKind::ControlChange, 74));

    // Each half is seven bits, so a stray larger number cannot spill into the other half.
    VERIFY_ARE_EQUAL(glass::MaximumControllerNumber, glass::ControllerNumber(200, 300));
}

void LayoutDocumentTests::AnNrpnAbove127SurvivesARoundTrip()
{
    auto document = MinimalDocument();

    glass::DeviceEntry synth{};
    synth.Name = L"Synth";
    document.Devices.push_back(synth);

    glass::ControlMessage message{};
    message.Kind = glass::MessageKind::AssignedController;
    message.DeviceName = L"Synth";
    message.Number = glass::ControllerNumber(3, 17);
    document.Pages[0].Controls[0].Messages.push_back(message);

    auto const reread = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(document));
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.Pages[0].Controls[0].Messages;
    VERIFY_ARE_EQUAL(size_t{ 1 }, back.size());
    VERIFY_IS_TRUE(back[0].Kind == glass::MessageKind::AssignedController);
    VERIFY_ARE_EQUAL(uint32_t{ 401 }, back[0].Number);
}

// ---- per-note controllers and note attributes ----

void LayoutDocumentTests::APerNoteControllerAndAnAttributeSurviveARoundTrip()
{
    auto document = MinimalDocument();

    glass::DeviceEntry synth{};
    synth.Name = L"Synth";
    document.Devices.push_back(synth);

    glass::ControlMessage controller{};
    controller.Kind = glass::MessageKind::AssignablePerNoteController;
    controller.DeviceName = L"Synth";
    controller.Number = 60;
    controller.Controller = 200;

    glass::ControlMessage note{};
    note.Kind = glass::MessageKind::Note;
    note.DeviceName = L"Synth";
    note.Number = 62;
    note.AttributeType = 3;
    note.AttributeData = 0xABCD;

    document.Pages[0].Controls[0].Messages = { controller, note };

    auto const reread = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(document));
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.Pages[0].Controls[0].Messages;
    VERIFY_ARE_EQUAL(size_t{ 2 }, back.size());
    VERIFY_IS_TRUE(back[0].Kind == glass::MessageKind::AssignablePerNoteController);
    VERIFY_ARE_EQUAL(60u, back[0].Number);
    VERIFY_ARE_EQUAL(200u, back[0].Controller);
    VERIFY_ARE_EQUAL(3u, back[1].AttributeType);
    VERIFY_ARE_EQUAL(0xABCDu, back[1].AttributeData);
}

void LayoutDocumentTests::NoControllerAndNoAttributeStayOutOfTheFile()
{
    auto document = MinimalDocument();

    glass::ControlMessage note{};
    note.Kind = glass::MessageKind::Note;
    note.Number = 60;
    document.Pages[0].Controls[0].Messages.push_back(note);

    auto const json = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(json.find(L"\"controller\"") == std::wstring::npos);
    VERIFY_IS_TRUE(json.find(L"\"attributeType\"") == std::wstring::npos);
    VERIFY_IS_TRUE(json.find(L"\"attributeData\"") == std::wstring::npos);

    // A number past what the field holds, from a stranger's file, is read as none.
    auto patched = json;
    auto const at = patched.find(L"\"number\"");
    VERIFY_IS_TRUE(at != std::wstring::npos);
    patched.insert(at, L"\"controller\": 300, \"attributeType\": 999, \"attributeData\": 70000, ");

    auto const reread = glass::ReadLayoutFromJson(patched);
    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& back = reread.Document.Pages[0].Controls[0].Messages[0];
    VERIFY_ARE_EQUAL(0u, back.Controller);
    VERIFY_ARE_EQUAL(0u, back.AttributeType);
    VERIFY_ARE_EQUAL(0u, back.AttributeData);
}

void LayoutDocumentTests::AnAttributeGoesOnlyOnAMidi2Note()
{
    glass::ControlMessage note{};
    note.Kind = glass::MessageKind::Note;

    VERIFY_IS_TRUE(glass::SendsNoteAttribute(note, glass::DeviceProtocol::Midi2));
    VERIFY_IS_FALSE(glass::SendsNoteAttribute(note, glass::DeviceProtocol::Midi1));
    VERIFY_IS_FALSE(glass::SendsNoteAttribute(note, glass::DeviceProtocol::MackieControl));

    note.UseMidi1Protocol = true;
    VERIFY_IS_FALSE(glass::SendsNoteAttribute(note, glass::DeviceProtocol::Midi2));

    glass::ControlMessage change{};
    change.Kind = glass::MessageKind::ControlChange;
    VERIFY_IS_FALSE(glass::SendsNoteAttribute(change, glass::DeviceProtocol::Midi2));

    VERIFY_IS_TRUE(glass::IsPerNoteController(glass::MessageKind::PerNoteController));
    VERIFY_IS_TRUE(glass::IsPerNoteController(glass::MessageKind::AssignablePerNoteController));
    VERIFY_IS_FALSE(glass::IsPerNoteController(glass::MessageKind::AssignedController));
}

// ---- the band that is on every page ----

void LayoutDocumentTests::TheBandIsNotAPageToGoTo()
{
    glass::LayoutDocument document{};
    document.Pages.resize(3);
    document.Pages[1].IsSharedBand = true;

    auto const pages = glass::PagesToChooseFrom(document);

    VERIFY_ARE_EQUAL(size_t{ 2 }, pages.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, pages[0]);
    VERIFY_ARE_EQUAL(size_t{ 2 }, pages[1]);

    // A layout that is nothing but bands still has a page to show.
    for (auto& page : document.Pages)
    {
        page.IsSharedBand = true;
    }

    VERIFY_ARE_EQUAL(size_t{ 3 }, glass::PagesToChooseFrom(document).size());
}

// ---- how a knob is turned ----

void LayoutDocumentTests::AKnobTurnedRoundAndRoundSurvivesARoundTrip()
{
    auto document = MinimalDocument();

    // Up and down is the default and stays out of the file.
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(document).find(L"\"drag\"") == std::wstring::npos);

    document.Pages[0].Controls[0].Drag = glass::DragAxis::Circular;

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"\"drag\": \"circular\"") != std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);
    VERIFY_IS_TRUE(reread.Document.Pages[0].Controls[0].Drag == glass::DragAxis::Circular);
}

// ---- what a group is called ----

namespace
{
    // Two controls in one group, so there is a group to name.
    glass::LayoutDocument GroupedDocument()
    {
        auto document = MinimalDocument();

        auto second = document.Pages[0].Controls[0];
        second.Id = L"c2";
        document.Pages[0].Controls.push_back(second);

        document.Pages[0].Controls[0].GroupId = L"g1";
        document.Pages[0].Controls[1].GroupId = L"g1";

        return document;
    }
}

void LayoutDocumentTests::AGroupNameSurvivesARoundTrip()
{
    auto document = GroupedDocument();

    // A group nobody named writes no list at all, so a file from before names comes back as it was.
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(document).find(L"\"controlGroups\"") == std::wstring::npos);

    document.Pages[0].Groups.push_back({ L"g1", L"Drums", nullptr });

    // The name of a group with nothing left in it is not written.
    document.Pages[0].Groups.push_back({ L"g2", L"Bass", nullptr });

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(text.find(L"Bass") == std::wstring::npos);

    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);

    auto const& page = reread.Document.Pages[0];

    VERIFY_ARE_EQUAL(size_t{ 1 }, page.Groups.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"g1" }, page.Groups[0].Id);
    VERIFY_ARE_EQUAL(std::wstring{ L"Drums" }, page.Groups[0].Name);

    // Understood, so not kept aside as something this build does not know.
    VERIFY_IS_TRUE(page.Unknown == nullptr);

    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::AGroupNameFromAFileIsChecked()
{
    auto document = GroupedDocument();

    document.Pages[0].Groups.push_back({ L"g1", L"Drums", nullptr });

    auto text = glass::WriteLayoutToJson(document);

    // An entry with no id, a second name for the same group, and something that is not an
    // entry at all, ahead of the real one.
    std::wstring const list{ L"\"controlGroups\": [" };
    auto const at = text.find(list);

    VERIFY_ARE_NOT_EQUAL(std::wstring::npos, at);

    text.insert(at + list.size(),
        L"{ \"name\": \"Nobody\" }, 7, { \"id\": \"g1\", \"name\": \"First\\u0007\" }, { \"id\": \"g1\", \"name\": \"Second\" }, ");

    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);

    // The first name for a group wins, cleaned of what cannot be shown, and the rest are dropped.
    auto const& groups = reread.Document.Pages[0].Groups;

    VERIFY_ARE_EQUAL(size_t{ 1 }, groups.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"g1" }, groups[0].Id);
    VERIFY_ARE_EQUAL(std::wstring{ L"First" }, groups[0].Name);
}

void LayoutDocumentTests::ACopiedGroupIsNumberedOnFromItsName()
{
    glass::Page page{};

    // A name nobody on the page has yet stays as it is, so a cut and paste changes nothing.
    VERIFY_ARE_EQUAL(std::wstring{ L"Strip 1" }, glass::NameForCopiedGroup(L"Strip 1", page));
    VERIFY_ARE_EQUAL(std::wstring{}, glass::NameForCopiedGroup(L"", page));

    page.Groups.push_back({ L"a", L"Strip 1", nullptr });
    page.Groups.push_back({ L"b", L"Strip 2", nullptr });
    page.Groups.push_back({ L"c", L"Drums", nullptr });
    page.Groups.push_back({ L"d", L"Bus 9", nullptr });
    page.Groups.push_back({ L"e", L"7", nullptr });

    VERIFY_ARE_EQUAL(std::wstring{ L"Strip 3" }, glass::NameForCopiedGroup(L"Strip 1", page));
    VERIFY_ARE_EQUAL(std::wstring{ L"Drums 2" }, glass::NameForCopiedGroup(L"Drums", page));
    VERIFY_ARE_EQUAL(std::wstring{ L"Bus 10" }, glass::NameForCopiedGroup(L"Bus 9", page));
    VERIFY_ARE_EQUAL(std::wstring{ L"8" }, glass::NameForCopiedGroup(L"7", page));
}

// ---- toolbars, pan controls, page tabs and fonts ----

namespace
{
    // The same file with one piece of its text swapped, the way an older build or a person
    // with a text editor would have written it.
    std::wstring Replaced(_In_ std::wstring text, _In_ std::wstring const& from, _In_ std::wstring const& to)
    {
        auto const at = text.find(from);

        VERIFY_IS_TRUE(at != std::wstring::npos);

        if (at != std::wstring::npos)
        {
            text.replace(at, from.size(), to);
        }

        return text;
    }
}

void LayoutDocumentTests::AnEncoderInAFileOpensAsAKnob()
{
    // An encoder drew and behaved exactly like a knob, so it went. A layout that has one still
    // opens, with a knob where it was, and says knob the next time it is written.
    auto const text = Replaced(
        glass::WriteLayoutToJson(MinimalDocument()), L"\"kind\": \"knob\"", L"\"kind\": \"encoder\"");

    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);
    VERIFY_IS_TRUE(reread.Document.Pages[0].Controls[0].Kind == glass::ControlKind::Knob);

    auto const written = glass::WriteLayoutToJson(reread.Document);

    VERIFY_IS_TRUE(written.find(L"\"kind\": \"knob\"") != std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"encoder") == std::wstring::npos);
}

void LayoutDocumentTests::APanControlSurvivesARoundTrip()
{
    auto document = MinimalDocument();

    // Off is the default and stays out of the file.
    VERIFY_IS_TRUE(glass::WriteLayoutToJson(document).find(L"lightsFromCenter") == std::wstring::npos);

    document.Pages[0].Controls[0].LightsFromCenter = true;

    auto const reread = glass::ReadLayoutFromJson(glass::WriteLayoutToJson(document));

    VERIFY_IS_TRUE(reread.Succeeded);
    VERIFY_IS_TRUE(reread.Document.Pages[0].Controls[0].LightsFromCenter);
    VERIFY_IS_TRUE(reread.Document.Pages[0].Controls[0].Unknown == nullptr ||
        !reread.Document.Pages[0].Controls[0].Unknown.HasKey(L"lightsFromCenter"));
}

void LayoutDocumentTests::TheWindowSettingsSurviveARoundTrip()
{
    auto document = MinimalDocument();

    // A layout that never asked for them writes the file it always did.
    auto const plain = glass::WriteLayoutToJson(document);

    VERIFY_IS_TRUE(plain.find(L"toolbarWindow") == std::wstring::npos);
    VERIFY_IS_TRUE(plain.find(L"alwaysOnTop") == std::wstring::npos);
    VERIFY_IS_TRUE(plain.find(L"seeThrough") == std::wstring::npos);

    document.ToolbarWindow = true;
    document.AlwaysOnTop = true;
    document.SeeThrough = true;

    auto const text = glass::WriteLayoutToJson(document);
    auto const reread = glass::ReadLayoutFromJson(text);

    VERIFY_IS_TRUE(reread.Succeeded);
    VERIFY_IS_TRUE(reread.Document.ToolbarWindow);
    VERIFY_IS_TRUE(reread.Document.AlwaysOnTop);
    VERIFY_IS_TRUE(reread.Document.SeeThrough);

    // Read as settings, not kept as keys this build does not know, or they would be written twice.
    VERIFY_ARE_EQUAL(text, glass::WriteLayoutToJson(reread.Document));
}

void LayoutDocumentTests::AFontNameThatIsReallyAPathIsDropped()
{
    auto document = MinimalDocument();
    document.Pages[0].Controls[0].Label = L"Volume";
    document.Pages[0].Controls[0].LabelLook.FontFamily = L"Bahnschrift";

    auto const text = glass::WriteLayoutToJson(document);

    VERIFY_ARE_EQUAL(std::wstring{ L"Bahnschrift" },
        glass::ReadLayoutFromJson(text).Document.Pages[0].Controls[0].LabelLook.FontFamily);

    // A layout can come from a stranger, and XAML would go and fetch a font file named here.
    for (auto const* const name :
        {
            LR"(\\\\server\\share\\font.ttf#Font)",
            LR"(C:\\Fonts\\font.ttf#Font)",
            L"ms-appx:///font.ttf#Font",
            L"/fonts/font.ttf#Font",
            L"Arial, Wingdings",
        })
    {
        auto const hostile = Replaced(text, L"\"fontFamily\": \"Bahnschrift\"", std::wstring{ L"\"fontFamily\": \"" } + name + L"\"");
        auto const reread = glass::ReadLayoutFromJson(hostile);

        VERIFY_IS_TRUE(reread.Succeeded);
        VERIFY_IS_TRUE(reread.Document.Pages[0].Controls[0].LabelLook.FontFamily.empty());
    }

    VERIFY_IS_TRUE(glass::IsSafeFontFamilyName(L"Segoe UI Variable Display"));
    VERIFY_IS_TRUE(glass::IsSafeFontFamilyName(L"Font Awesome 6 Free"));
    VERIFY_IS_FALSE(glass::IsSafeFontFamilyName(L""));
    VERIFY_IS_FALSE(glass::IsSafeFontFamilyName(L"   "));
    VERIFY_IS_FALSE(glass::IsSafeFontFamilyName(std::wstring(200, L'a')));
}

void LayoutDocumentTests::APageTabGoesWhereItsFirstPageRowSays()
{
    glass::Control tab{};
    tab.Kind = glass::ControlKind::PageTab;

    VERIFY_IS_TRUE(glass::PageTabTarget(tab).empty());

    glass::ControlMessage note{};
    note.Kind = glass::MessageKind::Note;

    glass::ControlMessage page{};
    page.Kind = glass::MessageKind::GoToPage;
    page.TargetPageId = L"p2";

    glass::ControlMessage later{};
    later.Kind = glass::MessageKind::GoToPage;
    later.TargetPageId = L"p3";

    tab.Messages = { note, page, later };

    VERIFY_ARE_EQUAL(std::wstring{ L"p2" }, glass::PageTabTarget(tab));
}
