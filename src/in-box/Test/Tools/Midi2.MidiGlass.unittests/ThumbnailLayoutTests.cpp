// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThumbnailLayoutTests.h"

#include <cmath>

#include "ThumbnailLayout.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::LayoutDocument PageOf(int32_t width, int32_t height)
    {
        glass::LayoutDocument document{};

        document.PageWidth = width;
        document.PageHeight = height;
        document.CanvasWidth = width;
        document.CanvasHeight = height;

        glass::Page page{};
        page.Id = L"p1";

        document.Pages.push_back(page);

        return document;
    }

    void AddControl(glass::LayoutDocument& document, double x, double y, double w, double h, int32_t slot = 0)
    {
        glass::Control control{};

        control.Id = L"c" + std::to_wstring(document.Pages[0].Controls.size());
        control.X = x;
        control.Y = y;
        control.Width = w;
        control.Height = h;
        control.HueSlot = slot;

        document.Pages[0].Controls.push_back(control);
    }

    glass::Theme const& StudioDark()
    {
        return *glass::FindBuiltInTheme(L"Studio Dark");
    }

    bool Close(double a, double b, double tolerance = 0.01)
    {
        return std::fabs(a - b) < tolerance;
    }
}

void ThumbnailLayoutTests::FillsTheImageWhenTheAspectMatches()
{
    // 1280 x 800 and 480 x 300 are both 1.6, so there is nothing to letterbox and the page
    // should use the whole card.
    auto const plan = glass::PlanThumbnail(PageOf(1280, 800), StudioDark(), 480, 300);

    VERIFY_ARE_EQUAL(480, plan.Width);
    VERIFY_ARE_EQUAL(300, plan.Height);

    VERIFY_IS_TRUE(Close(plan.PageBounds.X, 0.0));
    VERIFY_IS_TRUE(Close(plan.PageBounds.Y, 0.0));
    VERIFY_IS_TRUE(Close(plan.PageBounds.Width, 480.0));
    VERIFY_IS_TRUE(Close(plan.PageBounds.Height, 300.0));
}

void ThumbnailLayoutTests::LetterboxesAPortraitPageIntoALandscapeCard()
{
    // The portrait template, which is the one an author forgets and a tablet player uses.
    auto const plan = glass::PlanThumbnail(PageOf(1080, 1920), StudioDark(), 480, 300);

    // Height is the limit, so the page is full height and centered horizontally with a bar
    // either side.
    VERIFY_IS_TRUE(Close(plan.PageBounds.Height, 300.0));
    VERIFY_IS_TRUE(Close(plan.PageBounds.Width, 300.0 * 1080 / 1920));
    VERIFY_IS_TRUE(Close(plan.PageBounds.Y, 0.0));

    VERIFY_IS_GREATER_THAN(plan.PageBounds.X, 0.0);
    VERIFY_IS_TRUE(Close(plan.PageBounds.X, (480.0 - plan.PageBounds.Width) / 2.0));

    Log::Comment(String().Format(L"portrait page sits at x=%.1f, %.1f wide in a 480 card",
        plan.PageBounds.X, plan.PageBounds.Width));

    // The surround has to be distinguishable from the deck, or the card looks like a
    // mis-sized page rather than a letterboxed one.
    VERIFY_IS_FALSE(plan.SurroundColor == plan.DeckColor);
}

void ThumbnailLayoutTests::LetterboxesALandscapePageIntoASquareCard()
{
    auto const plan = glass::PlanThumbnail(PageOf(2560, 1440), StudioDark(), 200, 200);

    VERIFY_IS_TRUE(Close(plan.PageBounds.Width, 200.0));
    VERIFY_IS_TRUE(Close(plan.PageBounds.Height, 200.0 * 1440 / 2560));
    VERIFY_IS_TRUE(Close(plan.PageBounds.X, 0.0));
    VERIFY_IS_GREATER_THAN(plan.PageBounds.Y, 0.0);
}

void ThumbnailLayoutTests::NeverStretchesThePage()
{
    // A control surface is muscle memory. A card that stretched a page would show a customer
    // shapes they will never see when they run it.
    int32_t const pages[][2] { { 1280, 800 }, { 1920, 1080 }, { 2560, 1440 }, { 1024, 768 }, { 1080, 1920 } };

    for (auto const& page : pages)
    {
        auto const plan = glass::PlanThumbnail(PageOf(page[0], page[1]), StudioDark(), 480, 300);

        auto const pageAspect = static_cast<double>(page[0]) / page[1];
        auto const drawnAspect = plan.PageBounds.Width / plan.PageBounds.Height;

        VERIFY_IS_TRUE(Close(pageAspect, drawnAspect, 0.001));

        // and it must fit
        VERIFY_IS_LESS_THAN_OR_EQUAL(plan.PageBounds.Width, 480.0 + 0.01);
        VERIFY_IS_LESS_THAN_OR_EQUAL(plan.PageBounds.Height, 300.0 + 0.01);
    }
}

void ThumbnailLayoutTests::PlacesAControlWhereThePagePutsIt()
{
    auto document = PageOf(1280, 800);

    AddControl(document, 0, 0, 128, 80);        // top left corner, a tenth of the page
    AddControl(document, 640, 400, 128, 80);    // dead center

    auto const plan = glass::PlanThumbnail(document, StudioDark(), 480, 300);

    VERIFY_ARE_EQUAL(size_t{ 2 }, plan.Items.size());

    VERIFY_IS_TRUE(Close(plan.Items[0].Bounds.X, 0.0));
    VERIFY_IS_TRUE(Close(plan.Items[0].Bounds.Y, 0.0));
    VERIFY_IS_TRUE(Close(plan.Items[0].Bounds.Width, 48.0));
    VERIFY_IS_TRUE(Close(plan.Items[0].Bounds.Height, 30.0));

    VERIFY_IS_TRUE(Close(plan.Items[1].Bounds.X, 240.0));
    VERIFY_IS_TRUE(Close(plan.Items[1].Bounds.Y, 150.0));
}

void ThumbnailLayoutTests::LeavesOffPageControlsOutOfTheCard()
{
    auto document = PageOf(1280, 800);

    AddControl(document, 100, 100, 56, 56);      // on the page
    AddControl(document, 2000, 100, 56, 56);     // out on the work area
    AddControl(document, 1250, 100, 56, 56);     // straddling the right edge
    AddControl(document, -10, 100, 56, 56);      // straddling the left edge

    auto const plan = glass::PlanThumbnail(document, StudioDark(), 480, 300);

    // The editor still shows these and they are still real. They are not part of what ships,
    // and a card that drew them would misrepresent the layout at exactly the moment somebody is
    // choosing between layouts.
    VERIFY_ARE_EQUAL(size_t{ 1 }, plan.Items.size());
}

void ThumbnailLayoutTests::KeepsTinyControlsVisible()
{
    auto document = PageOf(2560, 1440);

    // A lamp on a big page, shrunk into a small card, lands well under a pixel.
    AddControl(document, 100, 100, 16, 16);

    auto const plan = glass::PlanThumbnail(document, StudioDark(), 240, 135);

    VERIFY_ARE_EQUAL(size_t{ 1 }, plan.Items.size());

    Log::Comment(String().Format(L"a 16 px control on a 2560 page draws at %.2f px on a 240 card",
        plan.Items[0].Bounds.Width));

    // Rounding it away would make a dense layout look empty, which is the opposite of what the
    // card is for.
    VERIFY_IS_GREATER_THAN(plan.Items[0].Bounds.Width, 1.0);
    VERIFY_IS_GREATER_THAN(plan.Items[0].Bounds.Height, 1.0);
}

void ThumbnailLayoutTests::ResolvesTheHueFromTheThemeSlot()
{
    auto document = PageOf(1280, 800);

    AddControl(document, 0, 0, 56, 56, 3);

    auto const& theme = StudioDark();
    auto const plan = glass::PlanThumbnail(document, theme, 480, 300);

    // The renderer never sees the theme, so the slot has to be resolved here or a card would be
    // drawn in whatever the renderer guessed.
    VERIFY_IS_TRUE(plan.Items[0].Hue == theme.HueSlots[3]);

    // A slot outside the theme falls back rather than reading past the array.
    auto outOfRange = PageOf(1280, 800);
    AddControl(outOfRange, 0, 0, 56, 56, glass::LiteralHue);

    auto const fallback = glass::PlanThumbnail(outOfRange, theme, 480, 300);
    VERIFY_IS_TRUE(fallback.Items[0].Hue == theme.HueSlots[0]);
}

void ThumbnailLayoutTests::SurvivesAPageWithNoSize()
{
    auto const plan = glass::PlanThumbnail(PageOf(0, 0), StudioDark(), 480, 300);

    VERIFY_ARE_EQUAL(480, plan.Width);
    VERIFY_ARE_EQUAL(300, plan.Height);
    VERIFY_ARE_EQUAL(size_t{ 0 }, plan.Items.size());

    // A zero sized image would make the renderer fail rather than produce a blank card.
    auto const zero = glass::PlanThumbnail(PageOf(1280, 800), StudioDark(), 0, 0);
    VERIFY_IS_GREATER_THAN(zero.Width, 0);
    VERIFY_IS_GREATER_THAN(zero.Height, 0);
}

void ThumbnailLayoutTests::SurvivesAPageIndexThatDoesNotExist()
{
    auto document = PageOf(1280, 800);
    AddControl(document, 10, 10, 56, 56);

    auto const plan = glass::PlanThumbnail(document, StudioDark(), 480, 300, 7);

    // Still a usable plan with a deck and a page rectangle, just nothing on it.
    VERIFY_ARE_EQUAL(size_t{ 0 }, plan.Items.size());
    VERIFY_IS_GREATER_THAN(plan.PageBounds.Width, 0.0);
}
