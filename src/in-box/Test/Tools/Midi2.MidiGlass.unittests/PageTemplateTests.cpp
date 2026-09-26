// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PageTemplateTests.h"

#include <algorithm>

#include "PageTemplates.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // The fraction of the page's width one control takes up. This is what "chunky" actually
    // means: a control is easy to hit when it is a large part of a small screen.
    double WidthFraction(glass::ControlKind kind, int32_t pageWidth, int32_t pageHeight)
    {
        auto const size = glass::DefaultControlSize(kind, pageWidth, pageHeight);
        return static_cast<double>(size.Width) / pageWidth;
    }
}

void PageTemplateTests::OffersTheTemplatesTheDesignNames()
{
    auto const& templates = glass::PageTemplates();

    VERIFY_ARE_EQUAL(size_t{ 6 }, templates.size());

    auto const has = [&templates](int32_t width, int32_t height)
        {
            return std::any_of(templates.begin(), templates.end(),
                [width, height](glass::PageTemplate const& t)
                { return t.Width == width && t.Height == height; });
        };

    VERIFY_IS_TRUE(has(1280, 800));
    VERIFY_IS_TRUE(has(1920, 1080));
    VERIFY_IS_TRUE(has(2560, 1440));
    VERIFY_IS_TRUE(has(2736, 1824));
    VERIFY_IS_TRUE(has(1024, 768));

    // portrait, which is the one an author forgets and a tablet player needs
    VERIFY_IS_TRUE(has(1080, 1920));
}

void PageTemplateTests::MatchesTheQuotedSizesOnTheReferencePage()
{
    constexpr int32_t w = glass::ReferencePageWidth;
    constexpr int32_t h = glass::ReferencePageHeight;

    // These four numbers are quoted in the design document. If the scaling formula ever stops
    // being exactly 1.0 at the reference page, these catch it.
    auto const pad = glass::DefaultControlSize(glass::ControlKind::Pad, w, h);
    VERIFY_ARE_EQUAL(56, pad.Width);
    VERIFY_ARE_EQUAL(56, pad.Height);

    auto const knob = glass::DefaultControlSize(glass::ControlKind::Knob, w, h);
    VERIFY_ARE_EQUAL(56, knob.Width);

    auto const fader = glass::DefaultControlSize(glass::ControlKind::Fader, w, h);
    VERIFY_ARE_EQUAL(40, fader.Width);
    VERIFY_ARE_EQUAL(180, fader.Height);

    auto const xy = glass::DefaultControlSize(glass::ControlKind::XYPad, w, h);
    VERIFY_ARE_EQUAL(240, xy.Width);
    VERIFY_ARE_EQUAL(240, xy.Height);
}

void PageTemplateTests::EverySizeLandsOnTheFourPixelQuantum()
{
    glass::ControlKind const kinds[]
    {
        glass::ControlKind::Knob, glass::ControlKind::Fader, glass::ControlKind::Pad,
        glass::ControlKind::Button, glass::ControlKind::Toggle, glass::ControlKind::XYPad,
        glass::ControlKind::Encoder, glass::ControlKind::Meter, glass::ControlKind::Lamp,
        glass::ControlKind::Readout, glass::ControlKind::Label, glass::ControlKind::Image,
        glass::ControlKind::PageTab,
    };

    for (auto const& page : glass::PageTemplates())
    {
        for (auto const kind : kinds)
        {
            auto const size = glass::DefaultControlSize(kind, page.Width, page.Height);

            // Everything is a multiple of 4 so snapping always lands cleanly and a control never
            // ends up on a half pixel.
            VERIFY_ARE_EQUAL(0, size.Width % glass::PixelQuantum);
            VERIFY_ARE_EQUAL(0, size.Height % glass::PixelQuantum);

            VERIFY_IS_GREATER_THAN(size.Width, 0);
            VERIFY_IS_GREATER_THAN(size.Height, 0);
        }
    }
}

void PageTemplateTests::ASmallPageGetsChunkierControlsThanALargeOne()
{
    auto const small = WidthFraction(glass::ControlKind::Pad, 1024, 768);
    auto const reference = WidthFraction(glass::ControlKind::Pad, 1280, 800);
    auto const large = WidthFraction(glass::ControlKind::Pad, 2560, 1440);

    Log::Comment(String().Format(
        L"pad takes %.2f%% of a 1024 page, %.2f%% of a 1280 page, %.2f%% of a 2560 page",
        small * 100, reference * 100, large * 100));

    // This is the behavior the design asks for, and it is not obvious: fingers do not get bigger
    // with the monitor, so a control has to be a larger share of a small page.
    VERIFY_IS_GREATER_THAN(small, reference);
    VERIFY_IS_GREATER_THAN(reference, large);
}

void PageTemplateTests::ABiggerPageStillGetsBiggerControlsInAbsoluteTerms()
{
    auto const small = glass::DefaultControlSize(glass::ControlKind::Pad, 1024, 768);
    auto const large = glass::DefaultControlSize(glass::ControlKind::Pad, 2560, 1440);

    // The other half of the trade. If the fraction shrank without the pixels growing, a pad on a
    // 4K page would end up a postage stamp.
    VERIFY_IS_GREATER_THAN(large.Width, small.Width);
}

void PageTemplateTests::SurvivesANonsensePageSize()
{
    auto const zero = glass::DefaultControlSize(glass::ControlKind::Knob, 0, 0);
    VERIFY_IS_GREATER_THAN(zero.Width, 0);

    auto const negative = glass::DefaultControlSize(glass::ControlKind::Knob, -100, -100);
    VERIFY_IS_GREATER_THAN(negative.Width, 0);

    VERIFY_ARE_EQUAL(glass::PixelQuantum, glass::QuantizePixels(0));
    VERIFY_ARE_EQUAL(glass::PixelQuantum, glass::QuantizePixels(-50));
}
