// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RuntimeSurfaceTests.h"

#include "SurfaceScale.h"
#include "PanicMessages.h"
#include "SurfaceColors.h"
#include "InputRules.h"
#include "LayoutTemplates.h"
#include "BindingEngine.h"
#include "ThemeModel.h"

#include <array>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::Control MakeControl(_In_ glass::ControlKind kind, _In_ int32_t hueSlot) noexcept
    {
        glass::Control control{};

        control.Id = L"control";
        control.Kind = kind;
        control.HueSlot = hueSlot;
        control.Width = 64;
        control.Height = 160;

        return control;
    }

    glass::Theme ThemeNamed(_In_ std::wstring const& name) noexcept
    {
        auto const* theme = glass::FindBuiltInTheme(name);

        VERIFY_IS_NOT_NULL(theme);

        return *theme;
    }
}

// ---- where the page sits in the window ----

void RuntimeSurfaceTests::ActualSizeNeverScales()
{
    auto const viewport = glass::ComputeViewport(
        1280, 800, 1920, 1080, glass::ScaleMode::ActualSize, 100.0);

    VERIFY_ARE_EQUAL(1.0, viewport.Scale);
    VERIFY_ARE_EQUAL(1280.0, viewport.ContentWidth);
    VERIFY_ARE_EQUAL(800.0, viewport.ContentHeight);
    VERIFY_IS_FALSE(viewport.NeedsScrolling);
}

void RuntimeSurfaceTests::ActualSizeCentersWhatFits()
{
    auto const viewport = glass::ComputeViewport(
        1280, 800, 1920, 1080, glass::ScaleMode::ActualSize, 100.0);

    VERIFY_ARE_EQUAL(320.0, viewport.OffsetX);
    VERIFY_ARE_EQUAL(140.0, viewport.OffsetY);
}

void RuntimeSurfaceTests::ActualSizeScrollsWhatDoesNot()
{
    auto const viewport = glass::ComputeViewport(
        2560, 1440, 1280, 800, glass::ScaleMode::ActualSize, 100.0);

    VERIFY_IS_TRUE(viewport.NeedsScrolling);

    // Hard against the origin, not centered. A page that started part way down its own scroll
    // extent would hide its top row, which is where the transport usually is.
    VERIFY_ARE_EQUAL(0.0, viewport.OffsetX);
    VERIFY_ARE_EQUAL(0.0, viewport.OffsetY);
}

void RuntimeSurfaceTests::FitScalesBothAxesTheSame()
{
    auto const viewport = glass::ComputeViewport(
        1280, 800, 640, 600, glass::ScaleMode::FitToScreen, 100.0);

    VERIFY_ARE_EQUAL(0.5, viewport.Scale);
    VERIFY_IS_FALSE(viewport.NeedsScrolling);
}

void RuntimeSurfaceTests::FitLetterboxesRatherThanStretching()
{
    // Width is the tighter constraint, so the bars end up above and below.
    auto const viewport = glass::ComputeViewport(
        1280, 800, 640, 600, glass::ScaleMode::FitToScreen, 100.0);

    VERIFY_ARE_EQUAL(640.0, viewport.ContentWidth);
    VERIFY_ARE_EQUAL(400.0, viewport.ContentHeight);
    VERIFY_ARE_EQUAL(0.0, viewport.OffsetX);
    VERIFY_ARE_EQUAL(100.0, viewport.OffsetY);
}

void RuntimeSurfaceTests::FitCanScaleUpOnABigDisplay()
{
    // A performer on a large touch monitor wants the surface filling it, not a small page in the
    // middle of a black field.
    auto const viewport = glass::ComputeViewport(
        1280, 800, 2560, 1600, glass::ScaleMode::FitToScreen, 100.0);

    VERIFY_ARE_EQUAL(2.0, viewport.Scale);
}

void RuntimeSurfaceTests::CustomPercentIsClamped()
{
    auto const tiny = glass::ComputeViewport(
        1280, 800, 1920, 1080, glass::ScaleMode::Custom, 0.0);

    VERIFY_ARE_EQUAL(glass::MinimumCustomScalePercent / 100.0, tiny.Scale);

    auto const huge = glass::ComputeViewport(
        1280, 800, 1920, 1080, glass::ScaleMode::Custom, 100000.0);

    VERIFY_ARE_EQUAL(glass::MaximumCustomScalePercent / 100.0, huge.Scale);
}

void RuntimeSurfaceTests::APointInTheLetterboxIsNotOnThePage()
{
    auto const viewport = glass::ComputeViewport(
        1280, 800, 640, 600, glass::ScaleMode::FitToScreen, 100.0);

    double pageX{ 0.0 };
    double pageY{ 0.0 };

    // 40 pixels down is inside the top bar, which must not count as the top row of controls.
    VERIFY_IS_FALSE(glass::ViewportToPage(viewport, 1280, 800, 100.0, 40.0, pageX, pageY));

    VERIFY_IS_TRUE(glass::ViewportToPage(viewport, 1280, 800, 100.0, 120.0, pageX, pageY));
}

void RuntimeSurfaceTests::APointRoundTripsThroughTheScale()
{
    auto const viewport = glass::ComputeViewport(
        1280, 800, 640, 600, glass::ScaleMode::FitToScreen, 100.0);

    double pageX{ 0.0 };
    double pageY{ 0.0 };

    VERIFY_IS_TRUE(glass::ViewportToPage(viewport, 1280, 800, 320.0, 300.0, pageX, pageY));

    VERIFY_ARE_EQUAL(640.0, pageX);
    VERIFY_ARE_EQUAL(400.0, pageY);
}

void RuntimeSurfaceTests::AWindowWithNoSizeIsHarmless()
{
    // A window can be measured before it has been laid out, and a divide by zero there would
    // take the whole layout down on open.
    auto const viewport = glass::ComputeViewport(
        1280, 800, 0.0, 0.0, glass::ScaleMode::FitToScreen, 100.0);

    VERIFY_ARE_EQUAL(1.0, viewport.Scale);
    VERIFY_ARE_EQUAL(0.0, viewport.ContentWidth);

    double pageX{ 0.0 };
    double pageY{ 0.0 };

    VERIFY_IS_FALSE(glass::ViewportToPage(
        glass::SurfaceViewport{ 0.0, 0.0, 0.0, 0.0, 0.0, false }, 1280, 800, 1.0, 1.0, pageX, pageY));
}

// ---- panic ----

void RuntimeSurfaceTests::PanicSendsFourMessagesPerChannel()
{
    std::array<uint32_t, glass::PanicWordsPerChannel> words{};

    VERIFY_ARE_EQUAL(glass::PanicWordsPerChannel, glass::BuildPanicWords(0, 0, words));
}

void RuntimeSurfaceTests::PanicReleasesTheSustainPedalFirst()
{
    std::array<uint32_t, glass::PanicWordsPerChannel> words{};

    glass::BuildPanicWords(3, 5, words);

    // Group 3, channel 5, control change. Sustain off, then all notes off, then all sound off:
    // asking for silence before the pedal is up does not stay silent.
    VERIFY_ARE_EQUAL(0x23B54000u, words[0]);
    VERIFY_ARE_EQUAL(0x23B57B00u, words[1]);
    VERIFY_ARE_EQUAL(0x23B57800u, words[2]);
}

void RuntimeSurfaceTests::PanicCentersPitchBend()
{
    std::array<uint32_t, glass::PanicWordsPerChannel> words{};

    glass::BuildPanicWords(0, 0, words);

    // 0x2000 on the wire is data 1 of 0 and data 2 of 0x40.
    VERIFY_ARE_EQUAL(0x20E00040u, words[3]);
}

void RuntimeSurfaceTests::PanicCoversEveryChannelOfAGroup()
{
    std::array<uint32_t, glass::PanicWordsPerChannel * glass::PanicChannelCount> words{};

    auto const written = glass::BuildPanicWordsForGroup(2, words);

    VERIFY_ARE_EQUAL(uint32_t{ 64 }, written);

    // Every channel nibble appears exactly four times.
    std::array<uint32_t, 16> perChannel{};

    for (uint32_t i = 0; i < written; ++i)
    {
        perChannel[(words[i] >> 16) & 0x0F]++;
    }

    for (auto const count : perChannel)
    {
        VERIFY_ARE_EQUAL(uint32_t{ 4 }, count);
    }
}

void RuntimeSurfaceTests::PanicRefusesABufferThatIsTooSmall()
{
    std::array<uint32_t, 3> words{};

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::BuildPanicWords(0, 0, words));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, glass::BuildPanicWordsForGroup(0, words));
}

// ---- which groups a layout drives ----

void RuntimeSurfaceTests::GroupMasksFollowTheDeviceTable()
{
    glass::LayoutDocument document{};

    document.Devices.push_back({ L"Synth" });
    document.Devices.push_back({ L"Desk" });

    glass::Page page{};

    glass::Control first{};
    first.Messages.push_back({ glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Synth", 0 });
    first.Messages.push_back({ glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Synth", 3 });

    glass::Control second{};
    second.Messages.push_back({ glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Desk", 7 });

    page.Controls.push_back(first);
    page.Controls.push_back(second);

    document.Pages.push_back(page);

    auto const masks = glass::CollectGroupMasks(document);

    VERIFY_ARE_EQUAL(size_t{ 2 }, masks.size());

    // Groups 0 and 3 on the synth, group 7 on the desk.
    VERIFY_ARE_EQUAL(uint16_t{ 0x0009 }, masks[0]);
    VERIFY_ARE_EQUAL(uint16_t{ 0x0080 }, masks[1]);
}

void RuntimeSurfaceTests::GroupMasksIgnoreADeviceThatIsNotInTheTable()
{
    glass::LayoutDocument document{};

    document.Devices.push_back({ L"Synth" });

    glass::Page page{};

    glass::Control control{};
    control.Messages.push_back({ glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Gone", 4 });

    page.Controls.push_back(control);
    document.Pages.push_back(page);

    auto const masks = glass::CollectGroupMasks(document);

    VERIFY_ARE_EQUAL(size_t{ 1 }, masks.size());
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, masks[0]);
}

void RuntimeSurfaceTests::AMessageOnEveryGroupSetsEveryBit()
{
    glass::LayoutDocument document{};

    document.Devices.push_back({ L"Synth" });

    glass::Page page{};

    glass::Control control{};
    control.Messages.push_back(
        { glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Synth", glass::AllGroups });

    page.Controls.push_back(control);
    document.Pages.push_back(page);

    auto const masks = glass::CollectGroupMasks(document);

    VERIFY_ARE_EQUAL(uint16_t{ 0xFFFF }, masks[0]);
}

void RuntimeSurfaceTests::GroupMasksIncludeSequenceSteps()
{
    // A sequence is a way of sending, not a different kind of destination, so a panic has to
    // reach wherever it plays.
    glass::LayoutDocument document{};

    document.Devices.push_back({ L"Synth" });

    glass::Sequence sequence{};
    sequence.Name = L"Intro";

    glass::SequenceStep step{};
    step.Kind = glass::SequenceStepKind::SendMidiMessage;
    step.Message.DeviceName = L"Synth";
    step.Message.GroupIndex = 11;

    sequence.Steps.push_back(step);
    document.Sequences.push_back(sequence);

    auto const masks = glass::CollectGroupMasks(document);

    VERIFY_ARE_EQUAL(uint16_t{ 0x0800 }, masks[0]);
}

// ---- which way a finger moves a control ----

void RuntimeSurfaceTests::AVerticalFaderReadsBottomToTop()
{
    // Screen coordinates run downward and a fader does not. This is the one place the arithmetic
    // is easy to get backwards, and nobody would notice until a fader was upside down on stage.
    VERIFY_ARE_EQUAL(1.0, glass::PositionToValue(glass::ControlKind::Fader, 48, 200, 24, 0));
    VERIFY_ARE_EQUAL(0.0, glass::PositionToValue(glass::ControlKind::Fader, 48, 200, 24, 200));
    VERIFY_ARE_EQUAL(0.5, glass::PositionToValue(glass::ControlKind::Fader, 48, 200, 24, 100));
}

void RuntimeSurfaceTests::AHorizontalFaderReadsLeftToRight()
{
    // A fader laid out wider than it is tall is a horizontal fader, so the axis follows the
    // rectangle rather than the name of the control.
    VERIFY_ARE_EQUAL(0.0, glass::PositionToValue(glass::ControlKind::Fader, 200, 48, 0, 24));
    VERIFY_ARE_EQUAL(1.0, glass::PositionToValue(glass::ControlKind::Fader, 200, 48, 200, 24));
}

void RuntimeSurfaceTests::AKnobIsNudgedRatherThanSet()
{
    // A knob has no travel under the finger, so a touch must not jump it to where the finger
    // landed. Every plug-in on the planet behaves this way.
    VERIFY_IS_FALSE(glass::UsesAbsolutePosition(glass::ControlKind::Knob));
    VERIFY_IS_FALSE(glass::UsesAbsolutePosition(glass::ControlKind::Encoder));
    VERIFY_IS_TRUE(glass::UsesAbsolutePosition(glass::ControlKind::Fader));
}

void RuntimeSurfaceTests::ADisplayOnlyControlTakesNoInput()
{
    VERIFY_IS_FALSE(glass::IsInteractive(glass::ControlKind::Meter));
    VERIFY_IS_FALSE(glass::IsInteractive(glass::ControlKind::Lamp));
    VERIFY_IS_FALSE(glass::IsInteractive(glass::ControlKind::Label));
    VERIFY_IS_FALSE(glass::IsInteractive(glass::ControlKind::Readout));
    VERIFY_IS_FALSE(glass::IsInteractive(glass::ControlKind::Image));

    VERIFY_IS_TRUE(glass::IsInteractive(glass::ControlKind::Fader));
    VERIFY_IS_TRUE(glass::IsMomentary(glass::ControlKind::Pad));
    VERIFY_IS_TRUE(glass::IsToggling(glass::ControlKind::Toggle));
}

// ---- colors ----

void RuntimeSurfaceTests::ATonalThemeTintsThePlateWithTheControlHue()
{
    auto const theme = ThemeNamed(L"Pigment Light");
    auto const control = MakeControl(glass::ControlKind::Fader, 0);

    auto const colors = glass::ResolveControlColors(control, theme);

    // A tint is a background color, which is why the tonal themes cost no extra rendering layer.
    VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.Plate.A);
    VERIFY_IS_TRUE(colors.Plate != theme.Deck.Color);
    VERIFY_IS_TRUE(colors.Plate != theme.HueSlots[0]);
}

void RuntimeSurfaceTests::AGlassThemeLeavesTheDeckShowingThrough()
{
    auto const studio = ThemeNamed(L"Studio Dark");
    auto const control = MakeControl(glass::ControlKind::Fader, 1);

    auto const colors = glass::ResolveControlColors(control, studio);

    VERIFY_IS_TRUE(colors.Plate.A > 0 && colors.Plate.A < 255);

    // Smoked, not black. A plate of black at this opacity over a near-black deck composites to
    // almost nothing, and the control stops reading as a piece of glass sitting on the surface
    // and starts reading as a hole cut out of it.
    auto const onDeck = glass::BlendOver(studio.Deck.Color, colors.Plate, 1.0);

    VERIFY_IS_TRUE(onDeck.R > 8 || onDeck.G > 8 || onDeck.B > 8);

    // Still darker than what it sits on, or it is not glass.
    VERIFY_IS_TRUE(glass::RelativeLuminance(onDeck) < glass::RelativeLuminance(studio.Deck.Color));

    // High contrast asks for no plate at all, so the deck is untouched behind the rim.
    auto const contrast = ThemeNamed(L"High contrast");

    VERIFY_ARE_EQUAL(uint8_t{ 0 }, glass::ResolveControlColors(control, contrast).Plate.A);
}

void RuntimeSurfaceTests::NothingIsSaturatedAtRest()
{
    // The rule the whole surface language rests on: a control's resting rim says which control
    // it is, and the value and the activity are the only things allowed to be bright. A full
    // strength rim on every control turns a busy page into a grid of neon rectangles.
    for (auto const& theme : glass::BuiltInThemes())
    {
        if (theme.Rim != glass::RimSource::ControlHue)
        {
            continue;
        }

        auto const control = MakeControl(glass::ControlKind::Fader, 0);
        auto const colors = glass::ResolveControlColors(control, theme);

        VERIFY_IS_LESS_THAN(colors.Rim.A, colors.Pipe.A);
    }
}

void RuntimeSurfaceTests::AFaderCapCarriesTheHueWhenItIsNotTheHue()
{
    auto const studio = ThemeNamed(L"Studio Dark");
    auto const control = MakeControl(glass::ControlKind::Fader, 2);
    auto const colors = glass::ResolveControlColors(control, studio);

    // Studio Dark's cap is a neutral machined bar, so the hue has to arrive as a line through
    // it. Six of them side by side are told apart by that line and nothing else.
    VERIFY_ARE_EQUAL(glass::ThumbStyle::Neutral, studio.Thumb);
    VERIFY_IS_TRUE(colors.ThumbLine.A > 0);
    VERIFY_IS_TRUE(colors.Thumb != colors.ThumbEnd);

    // Bigwig's cap is the hue itself, so a line of the same color would be invisible.
    auto const bigwig = ThemeNamed(L"Bigwig");
    auto const orange = glass::ResolveControlColors(control, bigwig);

    VERIFY_ARE_EQUAL(glass::ThumbStyle::Hue, bigwig.Thumb);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, orange.ThumbLine.A);
}

void RuntimeSurfaceTests::AFlatThemeAsksForAFlatValueBar()
{
    auto const control = MakeControl(glass::ControlKind::Fader, 0);

    // The glass themes fade the bar behind the value; the flat ones do not, because a fade on a
    // theme with no glow reads as the bar being the wrong length.
    auto const studio = glass::ResolveControlColors(control, ThemeNamed(L"Studio Dark"));
    VERIFY_IS_LESS_THAN(studio.PipeEnd.A, studio.Pipe.A);

    for (auto const* name : { L"Pigment Light", L"Pigment Dark", L"Bigwig", L"High contrast", L"Bone" })
    {
        auto const colors = glass::ResolveControlColors(control, ThemeNamed(name));

        VERIFY_ARE_EQUAL(colors.Pipe.A, colors.PipeEnd.A);
    }
}

void RuntimeSurfaceTests::ANamedPlateColorWins()
{
    // Bigwig names its plate outright rather than deriving one, because its whole idea is that
    // orange only ever means "this is the value". Bone does the same for a different reason.
    auto const theme = ThemeNamed(L"Bigwig");
    auto const control = MakeControl(glass::ControlKind::Knob, 0);

    auto const colors = glass::ResolveControlColors(control, theme);

    VERIFY_ARE_EQUAL(uint8_t{ 0x3A }, colors.Plate.R);
    VERIFY_ARE_EQUAL(uint8_t{ 255 }, colors.Plate.A);

    // and its rim is neutral, not the hue
    VERIFY_ARE_EQUAL(theme.NeutralRimColor.R, colors.Rim.R);
}

void RuntimeSurfaceTests::ALiteralColorThatDoesNotParseFallsBackToTheSlot()
{
    auto const theme = ThemeNamed(L"Studio Dark");

    auto control = MakeControl(glass::ControlKind::Fader, glass::LiteralHue);
    control.LiteralColor = L"not a color";

    // Black on a black deck is an invisible control, so a bad value falls back to something
    // legible rather than to nothing.
    VERIFY_IS_TRUE(glass::ResolveHue(control, theme) == theme.HueSlots[0]);

    control.LiteralColor = L"#FF00FF";

    auto const parsed = glass::ResolveHue(control, theme);

    VERIFY_ARE_EQUAL(uint8_t{ 0xFF }, parsed.R);
    VERIFY_ARE_EQUAL(uint8_t{ 0x00 }, parsed.G);
    VERIFY_ARE_EQUAL(uint8_t{ 0xFF }, parsed.B);
}

void RuntimeSurfaceTests::TheLampRingFallsBackToASolidArcWhenSmall()
{
    auto const theme = ThemeNamed(L"Bigwig");

    // Measured at 36 px the lamps stop separating and the ring reads as a fine comb, so below the
    // theme's own floor a knob draws a solid arc instead.
    VERIFY_IS_TRUE(glass::UsesLampRing(theme, 64, 64));
    VERIFY_IS_FALSE(glass::UsesLampRing(theme, 36, 36));

    VERIFY_IS_FALSE(glass::UsesLampRing(ThemeNamed(L"Studio Dark"), 120, 120));
}

void RuntimeSurfaceTests::LabelInkIsChosenByMeasuringTheBackground()
{
    auto const onBlack = glass::ReadableInk({ 0, 0, 0, 255 });
    auto const onWhite = glass::ReadableInk({ 255, 255, 255, 255 });

    VERIFY_IS_TRUE(onBlack.R > 200);
    VERIFY_IS_TRUE(onWhite.R < 60);

    VERIFY_IS_TRUE(glass::ContrastRatio(onBlack, { 0, 0, 0, 255 }) > 4.5);
    VERIFY_IS_TRUE(glass::ContrastRatio(onWhite, { 255, 255, 255, 255 }) > 4.5);
}

// ---- detents, as the surface sees them ----

void RuntimeSurfaceTests::ASmoothControlHasNoStops()
{
    glass::LayoutDocument document{};
    document.Devices.push_back({ L"Synth" });

    glass::Page page{};
    glass::Control control{};
    control.Messages.push_back({ glass::MessageTrigger::Changes, glass::MessageKind::ControlChange, L"Synth", 0 });
    page.Controls.push_back(control);
    document.Pages.push_back(page);

    glass::BindingEngine engine{};
    engine.Prepare(document, { { L"Synth", glass::DestinationProtocol::Midi2, true } });

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, engine.DetentCountForControl(0));
    VERIFY_ARE_EQUAL(0.37, engine.SnapToDetent(0, 0.37));
}

void RuntimeSurfaceTests::EveryListedStopGetsAnEqualShareOfTheTravel()
{
    glass::LayoutDocument document{};
    document.Devices.push_back({ L"Synth" });

    glass::Page page{};
    glass::Control control{};

    glass::ControlMessage message{};
    message.Trigger = glass::MessageTrigger::Changes;
    message.Kind = glass::MessageKind::ControlChange;
    message.DeviceName = L"Synth";
    message.Detents.Mode = glass::DetentMode::ExplicitValues;
    message.Detents.Scaling = glass::ValueScaling::Absolute;
    message.Detents.Stops = { 10, 17, 38, 39, 40, 57 };

    control.Messages.push_back(message);
    page.Controls.push_back(control);
    document.Pages.push_back(page);

    glass::BindingEngine engine{};
    engine.Prepare(document, { { L"Synth", glass::DestinationProtocol::Midi2, true } });

    VERIFY_ARE_EQUAL(uint32_t{ 6 }, engine.DetentCountForControl(0));

    // Six stops, six equal shares. Spaced by value instead, the middle three would be a fortieth
    // of the travel apart and nobody could pick one on purpose.
    for (uint32_t i = 0; i < 6; ++i)
    {
        VERIFY_ARE_EQUAL(i / 5.0, glass::DetentPosition(i, 6));
    }
}

void RuntimeSurfaceTests::SnappingPicksTheNearestStop()
{
    glass::LayoutDocument document{};
    document.Devices.push_back({ L"Synth" });

    glass::Page page{};
    glass::Control control{};

    glass::ControlMessage message{};
    message.Trigger = glass::MessageTrigger::Changes;
    message.Kind = glass::MessageKind::ControlChange;
    message.DeviceName = L"Synth";
    message.Detents.Mode = glass::DetentMode::EvenSteps;
    message.Detents.Scaling = glass::ValueScaling::Fraction;
    message.Detents.Step = 0.25;

    control.Messages.push_back(message);
    page.Controls.push_back(control);
    document.Pages.push_back(page);

    glass::BindingEngine engine{};
    engine.Prepare(document, { { L"Synth", glass::DestinationProtocol::Midi2, true } });

    VERIFY_ARE_EQUAL(uint32_t{ 5 }, engine.DetentCountForControl(0));

    VERIFY_ARE_EQUAL(0.0, engine.SnapToDetent(0, 0.04));
    VERIFY_ARE_EQUAL(0.25, engine.SnapToDetent(0, 0.20));
    VERIFY_ARE_EQUAL(1.0, engine.SnapToDetent(0, 0.99));
}

// ---- the starter layout ----

void RuntimeSurfaceTests::TheStarterLayoutIsValid()
{
    midiapp::EndpointMatch match{};
    match.EndpointDeviceId = L"\\\\?\\swd#midisrv#test";

    auto const document = glass::BuildStarterLayout(
        L"Test rig", L"Loopback A", match, midiapp::EndpointMatchMode::EndpointDeviceId);

    auto const issues = glass::Validate(document);

    for (auto const& issue : issues)
    {
        Log::Error(String().Format(L"%s: %s", issue.ObjectId.c_str(), issue.Detail.c_str()));
    }

    VERIFY_ARE_EQUAL(size_t{ 0 }, issues.size());

    VERIFY_ARE_EQUAL(
        static_cast<size_t>(glass::StarterFaderCount + glass::StarterKnobCount + glass::StarterPadCount),
        document.ControlCount());
}

void RuntimeSurfaceTests::TheStarterLayoutFitsOnItsPage()
{
    midiapp::EndpointMatch match{};

    auto const document = glass::BuildStarterLayout(
        L"Test rig", L"Loopback A", match, midiapp::EndpointMatchMode::EndpointName);

    // A starter layout with controls hanging off the page would be a poor first impression and
    // would not be drawn on its own card.
    VERIFY_ARE_EQUAL(size_t{ 0 }, document.ControlsOutsidePage().size());
}

void RuntimeSurfaceTests::TheStarterLayoutPointsAtOneDevice()
{
    midiapp::EndpointMatch match{};
    match.EndpointDeviceId = L"\\\\?\\swd#midisrv#test";

    auto const document = glass::BuildStarterLayout(
        L"Test rig", L"Loopback A", match, midiapp::EndpointMatchMode::EndpointDeviceId);

    VERIFY_ARE_EQUAL(size_t{ 1 }, document.Devices.size());
    VERIFY_IS_TRUE(document.Devices[0].Match.EndpointDeviceId == match.EndpointDeviceId);

    auto const masks = glass::CollectGroupMasks(document);

    VERIFY_ARE_EQUAL(uint16_t{ 0x0001 }, masks[0]);
}

void RuntimeSurfaceTests::TheStarterLayoutHasAKeyboardOrder()
{
    midiapp::EndpointMatch match{};

    auto const document = glass::BuildStarterLayout(
        L"Test rig", L"Loopback A", match, midiapp::EndpointMatchMode::EndpointName);

    // Tab order, screen reader order and the order a bank learn fills. A layout where every
    // control claimed order zero would walk in whatever order the file happened to be in.
    int32_t expected{ 0 };

    for (auto const& control : document.Pages[0].Controls)
    {
        VERIFY_ARE_EQUAL(expected, control.KeyboardOrder);
        expected++;
    }

    VERIFY_IS_TRUE(expected > 1);
}

// ---- every template the New layout picker offers ----

void RuntimeSurfaceTests::EveryTemplateIsValid()
{
    midiapp::EndpointMatch match{};
    match.EndpointDeviceId = L"\\\\?\\swd#midisrv#test";

    for (auto const& info : glass::LayoutTemplates())
    {
        auto const document = glass::BuildLayoutFromTemplate(
            info.Kind, L"Template test", L"Loopback A", match,
            midiapp::EndpointMatchMode::EndpointDeviceId);

        auto const issues = glass::Validate(document);

        for (auto const& issue : issues)
        {
            Log::Error(String().Format(L"template %d: %s: %s",
                static_cast<int32_t>(info.Kind), issue.ObjectId.c_str(), issue.Detail.c_str()));
        }

        VERIFY_ARE_EQUAL(size_t{ 0 }, issues.size());
    }
}

void RuntimeSurfaceTests::EveryTemplateFitsOnItsPage()
{
    midiapp::EndpointMatch match{};

    for (auto const& info : glass::LayoutTemplates())
    {
        auto const document = glass::BuildLayoutFromTemplate(
            info.Kind, L"Template test", L"Loopback A", match,
            midiapp::EndpointMatchMode::EndpointName);

        // A template with controls hanging off the page is a poor first impression and would not
        // be drawn on its own card.
        auto const outside = document.ControlsOutsidePage();

        if (!outside.empty())
        {
            Log::Error(String().Format(L"template %d has %u controls off the page",
                static_cast<int32_t>(info.Kind), static_cast<uint32_t>(outside.size())));
        }

        VERIFY_ARE_EQUAL(size_t{ 0 }, outside.size());
    }
}

void RuntimeSurfaceTests::EveryTemplateDrivesTheOneDevice()
{
    midiapp::EndpointMatch match{};

    for (auto const& info : glass::LayoutTemplates())
    {
        auto const document = glass::BuildLayoutFromTemplate(
            info.Kind, L"Template test", L"Loopback A", match,
            midiapp::EndpointMatchMode::EndpointName);

        VERIFY_ARE_EQUAL(size_t{ 1 }, document.Devices.size());

        // Every template but the blank one sends on group 0 of that one device, so a panic knows
        // where to be loud.
        auto const masks = glass::CollectGroupMasks(document);

        VERIFY_ARE_EQUAL(
            info.Kind == glass::LayoutTemplateKind::Blank ? uint16_t{ 0 } : uint16_t{ 0x0001 },
            masks[0]);
    }
}

void RuntimeSurfaceTests::TheBlankTemplateHasAPageAndADeviceAndNothingElse()
{
    midiapp::EndpointMatch match{};

    auto const document = glass::BuildLayoutFromTemplate(
        glass::LayoutTemplateKind::Blank, L"Empty", L"Loopback A", match,
        midiapp::EndpointMatchMode::EndpointName);

    // Blank still has to be runnable, so it is a page and a device, not an empty document.
    VERIFY_ARE_EQUAL(size_t{ 1 }, document.Pages.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, document.Devices.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, document.ControlCount());
    VERIFY_ARE_EQUAL(size_t{ 0 }, glass::Validate(document).size());
}
