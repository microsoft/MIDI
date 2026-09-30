// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SurfaceThumbnail.h"

#include "DeckBrush.h"
#include "SurfaceRenderer.h"
#include "ThumbnailLayout.h"

#include <winrt/Windows.Graphics.Imaging.h>
#include <wil/cppwinrt_helpers.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace glass
{
    namespace
    {
        namespace imaging = ::winrt::Windows::Graphics::Imaging;
        namespace streams = ::winrt::Windows::Storage::Streams;
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

        // Long enough for a layout pass and a composition frame to land. A page with a picture
        // on it waits longer, because a picture is decoded after it is asked for rather than
        // when. Only ever paid for a stale card, one at a time, while the library is on screen.
        constexpr auto SettleTime = std::chrono::milliseconds{ 90 };
        constexpr auto PictureSettleTime = std::chrono::milliseconds{ 400 };

        // How far off to one side the page is built. Anywhere outside the window will do; this
        // is simply further than any window is wide.
        constexpr double OutOfSight = 20000.0;

        winrt::Windows::UI::Color ToColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        bool HasPictures(_In_ LayoutDocument const& document) noexcept
        {
            if (!document.BackgroundImage.empty())
            {
                return true;
            }

            if (document.Pages.empty())
            {
                return false;
            }

            for (auto const& control : document.Pages[0].Controls)
            {
                if (!control.Image.IsEmpty())
                {
                    return true;
                }
            }

            return false;
        }

        // A capture with no opaque pixel in it is treated as no capture at all, so the caller
        // falls back to the plain drawing rather than saving a blank card.
        bool HasAnythingInIt(_In_ std::vector<uint8_t> const& bgra) noexcept
        {
            for (size_t index = 3; index < bgra.size(); index += 4)
            {
                if (bgra[index] != 0)
                {
                    return true;
                }
            }

            return false;
        }

        // Written beside the card and moved over it, so the library never reads half a file.
        bool WriteCard(_In_ std::wstring const& filePath, _In_ uint8_t const* data, _In_ size_t size) noexcept
        {
            try
            {
                auto const folder = std::filesystem::path{ filePath }.parent_path();

                if (!folder.empty())
                {
                    std::error_code ignored{};
                    std::filesystem::create_directories(folder, ignored);
                }

                auto const partial = filePath + L".partial";

                {
                    std::ofstream stream{ std::filesystem::path{ partial }, std::ios::binary | std::ios::trunc };

                    if (!stream.is_open())
                    {
                        return false;
                    }

                    stream.write(reinterpret_cast<char const*>(data), static_cast<std::streamsize>(size));

                    if (!stream.good())
                    {
                        return false;
                    }
                }

                if (!::MoveFileExW(partial.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING))
                {
                    std::error_code ignored{};
                    std::filesystem::remove(partial, ignored);
                    return false;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncOperation<bool> RenderSurfaceThumbnailAsync(
        controls::Panel host,
        LayoutDocument document,
        Theme theme,
        int32_t imageWidth,
        int32_t imageHeight,
        std::wstring filePath)
    {
        if (host == nullptr || filePath.empty() || imageWidth <= 0 || imageHeight <= 0 || document.Pages.empty())
        {
            co_return false;
        }

        auto const queue = host.DispatcherQueue();

        if (queue == nullptr)
        {
            co_return false;
        }

        auto const pageWidth = std::max(static_cast<double>(document.PageWidth), 1.0);
        auto const pageHeight = std::max(static_cast<double>(document.PageHeight), 1.0);

        // Letterboxed, never stretched, the same way the plain drawing does it, so a card shows
        // the proportions the layout really has.
        auto const scale = std::min(imageWidth / pageWidth, imageHeight / pageHeight);
        auto const boxWidth = imageWidth / scale;
        auto const boxHeight = imageHeight / scale;

        auto const plan = PlanThumbnail(document, theme, imageWidth, imageHeight);

        controls::Canvas stage{ nullptr };
        SurfaceRenderer renderer{};

        std::vector<uint8_t> pixels{};
        int32_t pixelWidth{ 0 };
        int32_t pixelHeight{ 0 };

        auto built = false;

        try
        {
            controls::Grid root{};
            root.Width(boxWidth);
            root.Height(boxHeight);
            root.IsHitTestVisible(false);
            root.Background(media::SolidColorBrush(ToColor(plan.SurroundColor)));

            controls::Grid page{};
            page.Width(pageWidth);
            page.Height(pageHeight);
            page.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            page.VerticalAlignment(xaml::VerticalAlignment::Center);

            // A control parked off the page is not part of what ships, so the page stops it.
            media::RectangleGeometry clip{};
            clip.Rect(foundation::Rect{
                0.0f, 0.0f, static_cast<float>(pageWidth), static_cast<float>(pageHeight) });
            page.Clip(clip);

            shapes::Rectangle deck{};
            deck.Width(pageWidth);
            deck.Height(pageHeight);
            deck.Fill(MakeDeckBrush(theme.Deck));

            controls::Canvas surface{};
            surface.Width(pageWidth);
            surface.Height(pageHeight);

            page.Children().Append(deck);
            page.Children().Append(surface);
            root.Children().Append(page);

            // Out of sight but in the tree and not collapsed, which is all a capture needs. The
            // shift is on a stage around the page rather than on the page itself, so the page is
            // captured where it is rather than where it was moved to.
            stage = controls::Canvas{};
            stage.IsHitTestVisible(false);

            media::TranslateTransform away{};
            away.X(-(OutOfSight + boxWidth));
            stage.RenderTransform(away);

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                stage, xaml::Automation::Peers::AccessibilityView::Raw);

            stage.Children().Append(root);
            host.Children().Append(stage);

            // A card is a still, so nothing on it plays.
            renderer.SetVideosLive(false);

            renderer.Build(surface, document, theme, 0);
            renderer.ShowCurrentPage(document, 0);

            // A stopwatch on a card reads zero, the way the layout opens.
            renderer.SetElapsedRunning(false);

            // Nothing on a card can be reached. It is on screen for a tenth of a second, and a
            // screen reader or the Tab key finding a control that is not there is worse than it
            // not being findable at all.
            for (size_t index = 0; index < renderer.ItemCount(); ++index)
            {
                if (auto const element = renderer.ElementAt(index))
                {
                    element.IsTabStop(false);

                    xaml::Automation::AutomationProperties::SetAccessibilityView(
                        element, xaml::Automation::Peers::AccessibilityView::Raw);
                }
            }

            ApplyDeckOverlay(deck, theme, pageWidth, pageHeight, scale, DeckOverlayLayer::BeneathControls);
            ApplyDeckOverlay(surface, theme, pageWidth, pageHeight, scale, DeckOverlayLayer::AboveControls);

            built = true;
        }
        catch (...)
        {
            built = false;
        }

        if (!built)
        {
            renderer.Teardown();

            if (stage != nullptr)
            {
                uint32_t index{ 0 };

                if (host.Children().IndexOf(stage, index))
                {
                    host.Children().RemoveAt(index);
                }
            }

            co_return false;
        }

        co_await winrt::resume_after(HasPictures(document) ? PictureSettleTime : SettleTime);
        co_await wil::resume_foreground(queue);

        try
        {
            media::Imaging::RenderTargetBitmap bitmap{};

            co_await bitmap.RenderAsync(stage.Children().GetAt(0), imageWidth, imageHeight);

            auto const buffer = co_await bitmap.GetPixelsAsync();

            pixelWidth = bitmap.PixelWidth();
            pixelHeight = bitmap.PixelHeight();

            if (buffer != nullptr && buffer.Length() > 0)
            {
                pixels.assign(buffer.data(), buffer.data() + buffer.Length());
            }
        }
        catch (...)
        {
            pixels.clear();
        }

        // Taken down on the UI thread it was built on, before anything else can go wrong.
        try
        {
            renderer.Teardown();

            uint32_t index{ 0 };

            if (host.Children().IndexOf(stage, index))
            {
                host.Children().RemoveAt(index);
            }
        }
        catch (...)
        {
        }

        if (pixelWidth <= 0 ||
            pixelHeight <= 0 ||
            pixels.size() < static_cast<size_t>(pixelWidth) * static_cast<size_t>(pixelHeight) * 4 ||
            !HasAnythingInIt(pixels))
        {
            co_return false;
        }

        auto written = false;

        try
        {
            streams::InMemoryRandomAccessStream stream{};

            auto encoder = co_await imaging::BitmapEncoder::CreateAsync(
                imaging::BitmapEncoder::PngEncoderId(), stream);

            encoder.SetPixelData(
                imaging::BitmapPixelFormat::Bgra8,
                imaging::BitmapAlphaMode::Premultiplied,
                static_cast<uint32_t>(pixelWidth),
                static_cast<uint32_t>(pixelHeight),
                96.0,
                96.0,
                pixels);

            co_await encoder.FlushAsync();

            auto const size = static_cast<uint32_t>(stream.Size());

            if (size > 0)
            {
                streams::Buffer encoded{ size };

                stream.Seek(0);

                auto const read = co_await stream.ReadAsync(encoded, size, streams::InputStreamOptions::None);

                written = WriteCard(filePath, read.data(), read.Length());
            }
        }
        catch (...)
        {
            written = false;
        }

        co_return written;
    }
}
