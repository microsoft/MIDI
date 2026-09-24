// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThumbnailRenderer.h"

#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.Canvas.Brushes.h>

#include <shlobj_core.h>

#include <array>
#include <cwctype>
#include <filesystem>

namespace glass
{
    namespace
    {
        namespace canvas = ::winrt::Microsoft::Graphics::Canvas;

        winrt::Windows::UI::Color ToColor(_In_ ThemeColor const& color, _In_ double opacity = 1.0) noexcept
        {
            auto const alpha = static_cast<uint8_t>(
                std::clamp(std::lround(color.A * std::clamp(opacity, 0.0, 1.0)), 0L, 255L));

            return winrt::Windows::UI::ColorHelper::FromArgb(alpha, color.R, color.G, color.B);
        }

        winrt::Windows::Foundation::Rect ToRect(_In_ ThumbnailRect const& rect) noexcept
        {
            return
            {
                static_cast<float>(rect.X),
                static_cast<float>(rect.Y),
                static_cast<float>(rect.Width),
                static_cast<float>(rect.Height)
            };
        }

        // The plate is dark and slightly translucent, the rim is a hairline in the control's hue,
        // and nothing is filled or saturated at rest. Same four ideas the surface uses, at a size
        // where only the first two survive.
        void DrawControl(
            _In_ canvas::CanvasDrawingSession const& session,
            _In_ ThumbnailItem const& item,
            _In_ ThumbnailPlan const& plan) noexcept
        {
            auto const bounds = ToRect(item.Bounds);
            auto const radius = static_cast<float>((std::min)(
                plan.CornerRadius,
                (std::min)(item.Bounds.Width, item.Bounds.Height) / 2.0));

            ThemeColor plate{ 0, 0, 0, 255 };
            plate.A = static_cast<uint8_t>(std::lround(255 * plan.PlateOpacity));

            if (plan.PlateOpacity > 0.0)
            {
                session.FillRoundedRectangle(bounds, radius, radius, ToColor(plate));
            }

            // A knob or a pad is square and reads as its rim; a fader is a slot. At thumbnail
            // size the difference between thirteen control types is not legible, so the card
            // makes the one distinction that is: tall and thin, or not.
            auto const rimColor = ToColor(item.Hue, 0.85);

            if (item.Bounds.Width < 2.0 || item.Bounds.Height < 2.0)
            {
                // Below a couple of pixels a stroked outline disappears entirely, so the control
                // becomes a solid mark instead of vanishing from the card.
                session.FillRoundedRectangle(bounds, radius, radius, rimColor);
                return;
            }

            session.DrawRoundedRectangle(bounds, radius, radius, rimColor, 1.0f);

            // The value pipe, drawn at rest, which is what makes a card look like the surface
            // rather than a wireframe of it.
            auto const isTall = item.Bounds.Height > item.Bounds.Width * 1.5;

            if (isTall && item.Bounds.Height > 6.0)
            {
                auto const pipeWidth = (std::max)(item.Bounds.Width * 0.18, 1.0);
                auto const pipeHeight = item.Bounds.Height * 0.35;

                winrt::Windows::Foundation::Rect pipe
                {
                    static_cast<float>(item.Bounds.X + (item.Bounds.Width - pipeWidth) / 2.0),
                    static_cast<float>(item.Bounds.Y + item.Bounds.Height - pipeHeight - 1.0),
                    static_cast<float>(pipeWidth),
                    static_cast<float>(pipeHeight)
                };

                session.FillRectangle(pipe, ToColor(item.Hue, 0.55));
            }
        }
    }

    _Use_decl_annotations_
    ThumbnailResult RenderThumbnailToFile(ThumbnailPlan const& plan, std::wstring const& filePath) noexcept
    {
        ThumbnailResult result{};

        try
        {
            if (filePath.empty() || plan.Width <= 0 || plan.Height <= 0)
            {
                result.Detail = L"The thumbnail has no size or no destination.";
                return result;
            }

            // A software device, deliberately. This runs with no window, sometimes on a machine
            // with no usable GPU, and a card is not worth failing over.
            canvas::CanvasDevice device{ true };

            canvas::CanvasRenderTarget target
            {
                device,
                static_cast<float>(plan.Width),
                static_cast<float>(plan.Height),
                96.0f
            };

            {
                auto session = target.CreateDrawingSession();

                session.Clear(ToColor(plan.SurroundColor));

                // Lit from just above the top edge, the way the surface itself is, so a card
                // reads as glass rather than a flat rectangle.
                std::array<canvas::Brushes::CanvasGradientStop, 3> stops
                {
                    canvas::Brushes::CanvasGradientStop{ 0.0f, ToColor(plan.DeckTopColor) },
                    canvas::Brushes::CanvasGradientStop{ 0.58f, ToColor(plan.DeckColor) },
                    canvas::Brushes::CanvasGradientStop{ 1.0f, ToColor(plan.DeckBottomColor) },
                };

                canvas::Brushes::CanvasRadialGradientBrush deck{ device, stops };

                auto const bounds = ToRect(plan.PageBounds);

                deck.Center({ bounds.X + bounds.Width / 2.0f, bounds.Y - bounds.Height * 0.10f });
                deck.RadiusX(bounds.Width * 0.65f);
                deck.RadiusY(bounds.Height);

                session.FillRectangle(bounds, deck);

                for (auto const& item : plan.Items)
                {
                    DrawControl(session, item, plan);
                }
            }

            auto const folder = std::filesystem::path{ filePath }.parent_path();

            if (!folder.empty())
            {
                std::error_code ignored{};
                std::filesystem::create_directories(folder, ignored);
            }

            target.SaveAsync(winrt::hstring{ filePath }, canvas::CanvasBitmapFileFormat::Png).get();

            result.Succeeded = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            result.Detail = std::wstring{ ex.message() };
        }
        catch (...)
        {
            result.Detail = L"The thumbnail could not be written.";
        }

        return result;
    }

    std::wstring ThumbnailCacheFolder() noexcept
    {
        try
        {
            wil::unique_cotaskmem_string path{};

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &path)))
            {
                return {};
            }

            std::filesystem::path folder{ path.get() };
            folder /= L"Microsoft";
            folder /= L"MIDI Glass";
            folder /= L"Thumbnails";

            std::error_code ignored{};
            std::filesystem::create_directories(folder, ignored);

            return folder.wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring ThumbnailPathForLayout(std::wstring const& layoutFilePath, int32_t imageWidth) noexcept
    {
        try
        {
            auto const folder = ThumbnailCacheFolder();

            if (folder.empty() || layoutFilePath.empty())
            {
                return {};
            }

            // Two layouts with the same file name in different folders must not share a card, so
            // the whole path is hashed rather than just the name.
            std::wstring lowered{ layoutFilePath };
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            auto const hash = std::hash<std::wstring>{}(lowered);

            std::filesystem::path file{ folder };
            file /= std::format(L"{:016x}-{}.png", hash, imageWidth);

            return file.wstring();
        }
        catch (...)
        {
            return {};
        }
    }
}
