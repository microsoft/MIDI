// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The hands-on parts of the picture panel: nine buttons that push a picture to an edge or a
// corner, a small copy of the whole picture with a box around the part that shows, and an
// opacity track drawn the way paint programs draw one.
//
// All three edit the same two numbers the canvas renderer reads, CenterX and CenterY, so there
// is one way to position a picture and the buttons, the box and the arrow keys cannot disagree.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "LayoutStore.h"

#include <winrt/Windows.Storage.FileProperties.h>

#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;
        namespace imaging = ::winrt::Microsoft::UI::Xaml::Media::Imaging;

        // Left, middle and right, or top, middle and bottom, as the number that puts the
        // picture there.
        constexpr double AlignFractions[]{ 0.0, 0.5, 1.0 };

        // Reading order, the same names the page size dialog uses for its anchor.
        constexpr wchar_t const* AlignNames[]
        {
            L"AnchorTopLeft", L"AnchorTop", L"AnchorTopRight",
            L"AnchorLeft", L"AnchorCenter", L"AnchorRight",
            L"AnchorBottomLeft", L"AnchorBottom", L"AnchorBottomRight",
        };

        // The space around the picture inside the crop box, so the box outline is never cut
        // off by the edge.
        constexpr double CropInset = 6.0;

        bool Near(_In_ double first, _In_ double second) noexcept
        {
            return std::abs(first - second) < 0.001;
        }

        // Which of the nine buttons the picture is on, or -1 when a drag left it in between.
        int32_t AlignIndexOf(_In_ double centerX, _In_ double centerY) noexcept
        {
            int32_t column{ -1 };
            int32_t row{ -1 };

            for (int32_t index = 0; index < 3; ++index)
            {
                if (Near(centerX, AlignFractions[index]))
                {
                    column = index;
                }

                if (Near(centerY, AlignFractions[index]))
                {
                    row = index;
                }
            }

            return (column < 0 || row < 0) ? -1 : (row * 3) + column;
        }

        // To the percent, which is what the old sliders stored and what anybody can see.
        double ToPercent(_In_ double value) noexcept
        {
            return std::clamp(std::round(value * 100.0) / 100.0, 0.0, 1.0);
        }

        media::Brush ThemeBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        winrt::Windows::UI::Color ThemeColorOf(_In_ wchar_t const* key)
        {
            if (auto const brush = ThemeBrush(key).try_as<media::SolidColorBrush>())
            {
                return brush.Color();
            }

            return winrt::Windows::UI::Color{ 255, 128, 128, 128 };
        }

        // One axis of a crop drag. Where the picture fits inside the control on this axis the
        // box already covers all of it and there is nothing to move, so the number is left as
        // it was: it is what places the picture in the spare space, and a drag across the
        // other axis should not throw that away.
        double CenterAlong(
            _In_ double desired,
            _In_ double imageStart,
            _In_ double imageLength,
            _In_ double windowLength,
            _In_ double current) noexcept
        {
            if (imageLength <= 0.0 || windowLength >= imageLength - 0.5)
            {
                return current;
            }

            auto const half = (windowLength * 0.5) / imageLength;
            auto const along = (desired - imageStart) / imageLength;

            // Run up against an edge, it is that edge exactly, so the matching button lights.
            if (along <= half)
            {
                return 0.0;
            }

            if (along >= 1.0 - half)
            {
                return 1.0;
            }

            return ToPercent(along);
        }
    }

    // ---------------------------------------------------------------- built once

    void EditorWindow::BuildPicturePanel()
    {
        try
        {
            auto const grid = PictureAlignGrid();

            for (int32_t index = 0; index < 3; ++index)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto));
                grid.ColumnDefinitions().Append(column);

                controls::RowDefinition row{};
                row.Height(xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto));
                grid.RowDefinitions().Append(row);
            }

            auto const style = xaml::Application::Current().Resources()
                .Lookup(box_value(L"AnchorCellStyle")).as<xaml::Style>();

            for (int32_t index = 0; index < 9; ++index)
            {
                controls::Primitives::ToggleButton cell{};

                cell.Style(style);

                automation::AutomationProperties::SetName(cell, resources::GetString(AlignNames[index]));
                controls::ToolTipService::SetToolTip(cell, box_value(resources::GetString(AlignNames[index])));

                // Click, not Checked: Checked also fires while the inspector is being filled in.
                // Whatever the click did to the button itself, the document decides which one
                // is lit, so it is put back from the document straight after.
                cell.Click([weak = get_weak(), index](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->SetPictureCenter(AlignFractions[index % 3], AlignFractions[index / 3], false);

                            if (auto const* const control = strong->SingleSelectedControl())
                            {
                                strong->RefreshPicturePosition(*control);
                            }
                        }
                    });

                controls::Grid::SetColumn(cell, index % 3);
                controls::Grid::SetRow(cell, index / 3);

                m_pictureAlignCells.push_back(cell);
                grid.Children().Append(cell);
            }

            // Both drawings take their colors from the theme, and a theme change does not
            // change their size, so SizeChanged alone would leave them in the old colors.
            PictureOpacityRamp().ActualThemeChanged([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->DrawPictureOpacityRamp();
                        strong->DrawPictureCropBox();
                    }
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to build the picture position buttons.")
    }

    // ---------------------------------------------------------------- filled in

    _Use_decl_annotations_
    void EditorWindow::RefreshPicturePosition(glass::Control const& control)
    {
        try
        {
            auto const& image = control.Image;
            auto const selected = AlignIndexOf(image.CenterX, image.CenterY);

            for (size_t index = 0; index < m_pictureAlignCells.size(); ++index)
            {
                m_pictureAlignCells[index].IsChecked(static_cast<int32_t>(index) == selected);
            }

            // The small copy is fetched again only when the file changes, not on every edit.
            auto const path = image.IsEmpty()
                ? std::wstring{}
                : glass::ControlPicturePath(m_filePath, image.FileName);

            if (path != m_cropPicturePath)
            {
                m_cropPicturePath = path;
                m_cropNaturalWidth = 0.0;
                m_cropNaturalHeight = 0.0;
                m_cropThumbnail = nullptr;
                ++m_cropLoadToken;

                if (!path.empty())
                {
                    LoadPictureCropThumbnail(path, glass::IsVideoFileName(image.FileName));
                }
            }

            DrawPictureCropBox();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show where the picture sits.")
    }

    // The canvas renderer needs the file's real size to crop it, and so does the box, or the
    // box would show a different crop from the one on the page.
    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::LoadPictureCropThumbnail(std::wstring path, bool isVideo)
    {
        auto lifetime = get_strong();
        auto const token = m_cropLoadToken;

        try
        {
            if (!isVideo)
            {
                foundation::Uri const uri{ L"file:///" + winrt::hstring{ path } };

                auto const extension = std::filesystem::path{ path }.extension().wstring();

                // An SVG has no size of its own. The canvas stretches it to the control, and
                // so does the box.
                if (_wcsicmp(extension.c_str(), L".svg") == 0)
                {
                    imaging::SvgImageSource svg{};

                    svg.UriSource(uri);

                    m_cropThumbnail = svg;
                    DrawPictureCropBox();
                    co_return;
                }

                imaging::BitmapImage bitmap{};

                bitmap.ImageOpened([weak = get_weak(), token](auto const& sender, auto const&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr || strong->m_cropLoadToken != token)
                        {
                            return;
                        }

                        auto const source = sender.template try_as<imaging::BitmapImage>();

                        if (source == nullptr)
                        {
                            return;
                        }

                        strong->m_cropNaturalWidth = static_cast<double>(source.PixelWidth());
                        strong->m_cropNaturalHeight = static_cast<double>(source.PixelHeight());
                        strong->DrawPictureCropBox();
                    });

                bitmap.UriSource(uri);

                m_cropThumbnail = bitmap;
                DrawPictureCropBox();
                co_return;
            }

            // A video: its frame size, and a frame to show, both from the shell.
            auto const file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path);
            auto const properties = co_await file.Properties().GetVideoPropertiesAsync();

            if (m_cropLoadToken != token)
            {
                co_return;
            }

            // A phone video shot upright is stored sideways and turned when played.
            auto const orientation = properties.Orientation();
            auto const turned =
                orientation == winrt::Windows::Storage::FileProperties::VideoOrientation::Rotate90 ||
                orientation == winrt::Windows::Storage::FileProperties::VideoOrientation::Rotate270;

            m_cropNaturalWidth = static_cast<double>(turned ? properties.Height() : properties.Width());
            m_cropNaturalHeight = static_cast<double>(turned ? properties.Width() : properties.Height());

            DrawPictureCropBox();

            auto const thumbnail = co_await file.GetThumbnailAsync(
                winrt::Windows::Storage::FileProperties::ThumbnailMode::SingleItem, 320);

            if (thumbnail == nullptr || m_cropLoadToken != token)
            {
                co_return;
            }

            imaging::BitmapImage frame{};

            co_await frame.SetSourceAsync(thumbnail);

            if (m_cropLoadToken != token)
            {
                co_return;
            }

            m_cropThumbnail = frame;
            DrawPictureCropBox();
        }
        catch (...)
        {
            // No thumbnail is not a failure worth a word. The box still shows the crop, with a
            // plain block standing in for the picture.
        }
    }

    // ---------------------------------------------------------------- drawing

    void EditorWindow::DrawPictureCropBox()
    {
        try
        {
            auto const canvas = PictureCropCanvas();

            canvas.Children().Clear();

            auto const* const control = SingleSelectedControl();

            auto const boxWidth = PictureCropBox().ActualWidth();
            auto const boxHeight = PictureCropBox().ActualHeight();

            if (control == nullptr ||
                control->Image.IsEmpty() ||
                boxWidth <= CropInset * 4.0 ||
                boxHeight <= CropInset * 4.0)
            {
                automation::AutomationProperties::SetItemStatus(PictureCropFrame(), L"");
                return;
            }

            auto const& picture = control->Image;
            auto const controlWidth = std::max(control->Width, 1.0);
            auto const controlHeight = std::max(control->Height, 1.0);

            // Exactly what the canvas works out, in the control's own units.
            auto const rect = glass::PictureCropRect(
                picture, controlWidth, controlHeight, m_cropNaturalWidth, m_cropNaturalHeight);

            // Everything that is either picture or control. A zoomed picture shows the part the
            // control cuts out of it; a small one shows the spare space around it.
            auto const left = std::min(0.0, rect.X);
            auto const top = std::min(0.0, rect.Y);
            auto const right = std::max(controlWidth, rect.X + rect.Width);
            auto const bottom = std::max(controlHeight, rect.Y + rect.Height);

            auto const scale = std::min(
                (boxWidth - (CropInset * 2.0)) / (right - left),
                (boxHeight - (CropInset * 2.0)) / (bottom - top));

            if (!(scale > 0.0))
            {
                return;
            }

            auto const originX = ((boxWidth - ((right - left) * scale)) * 0.5) - (left * scale);
            auto const originY = ((boxHeight - ((bottom - top) * scale)) * 0.5) - (top * scale);

            m_cropImageLeft = originX + (rect.X * scale);
            m_cropImageTop = originY + (rect.Y * scale);
            m_cropImageWidth = rect.Width * scale;
            m_cropImageHeight = rect.Height * scale;

            m_cropWindowLeft = originX;
            m_cropWindowTop = originY;
            m_cropWindowWidth = controlWidth * scale;
            m_cropWindowHeight = controlHeight * scale;

            auto const place = [&canvas](xaml::FrameworkElement const& element, double x, double y, double width, double height)
                {
                    element.Width(std::max(width, 0.0));
                    element.Height(std::max(height, 0.0));
                    element.IsHitTestVisible(false);

                    automation::AutomationProperties::SetAccessibilityView(
                        element, automation::Peers::AccessibilityView::Raw);

                    controls::Canvas::SetLeft(element, x);
                    controls::Canvas::SetTop(element, y);

                    canvas.Children().Append(element);
                };

            // The control itself, so spare space around a small picture reads as part of it.
            shapes::Rectangle frame{};
            frame.Fill(media::SolidColorBrush{ winrt::Windows::UI::Color{ 48, 128, 128, 128 } });
            place(frame, m_cropWindowLeft, m_cropWindowTop, m_cropWindowWidth, m_cropWindowHeight);

            if (m_cropThumbnail != nullptr)
            {
                if (auto const svg = m_cropThumbnail.try_as<imaging::SvgImageSource>())
                {
                    svg.RasterizePixelWidth(std::max(m_cropImageWidth * 2.0, 16.0));
                    svg.RasterizePixelHeight(std::max(m_cropImageHeight * 2.0, 16.0));
                }

                controls::Image image{};
                image.Source(m_cropThumbnail);
                image.Stretch(media::Stretch::Fill);
                place(image, m_cropImageLeft, m_cropImageTop, m_cropImageWidth, m_cropImageHeight);
            }
            else
            {
                // Still loading, or a video the shell has no frame for.
                shapes::Rectangle standIn{};
                standIn.Fill(ThemeBrush(L"ControlStrongFillColorDefaultBrush"));
                standIn.Opacity(0.5);
                place(standIn, m_cropImageLeft, m_cropImageTop, m_cropImageWidth, m_cropImageHeight);
            }

            // Dim everything the control cuts off: the four strips of picture around the box.
            media::SolidColorBrush const shade{ winrt::Windows::UI::Color{ 150, 0, 0, 0 } };

            auto const unionLeft = originX + (left * scale);
            auto const unionTop = originY + (top * scale);
            auto const unionRight = originX + (right * scale);
            auto const unionBottom = originY + (bottom * scale);
            auto const windowRight = m_cropWindowLeft + m_cropWindowWidth;
            auto const windowBottom = m_cropWindowTop + m_cropWindowHeight;

            auto const strip = [&](double x, double y, double width, double height)
                {
                    if (width < 0.5 || height < 0.5)
                    {
                        return;
                    }

                    shapes::Rectangle cut{};
                    cut.Fill(shade);
                    place(cut, x, y, width, height);
                };

            strip(unionLeft, unionTop, unionRight - unionLeft, m_cropWindowTop - unionTop);
            strip(unionLeft, windowBottom, unionRight - unionLeft, unionBottom - windowBottom);
            strip(unionLeft, m_cropWindowTop, m_cropWindowLeft - unionLeft, m_cropWindowHeight);
            strip(windowRight, m_cropWindowTop, unionRight - windowRight, m_cropWindowHeight);

            // The box itself, in the accent color so it reads as the thing to grab.
            shapes::Rectangle outline{};
            outline.Stroke(ThemeBrush(L"AccentFillColorDefaultBrush"));
            outline.StrokeThickness(2.0);
            place(outline, m_cropWindowLeft - 1.0, m_cropWindowTop - 1.0, m_cropWindowWidth + 2.0, m_cropWindowHeight + 2.0);

            // A screen reader hears where the middle is, since it cannot see the box move.
            automation::AutomationProperties::SetItemStatus(
                PictureCropFrame(),
                winrt::hstring{ resources::FormatString(
                    L"PictureCropPositionFormat",
                    std::to_wstring(static_cast<int32_t>(std::lround(picture.CenterX * 100.0))),
                    std::to_wstring(static_cast<int32_t>(std::lround(picture.CenterY * 100.0)))) });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the picture crop box.")
    }

    // A checkerboard for see-through on the left fading into solid on the right, painted into
    // one bitmap so the rounded ends of the track clip it cleanly.
    void EditorWindow::DrawPictureOpacityRamp()
    {
        try
        {
            auto const ramp = PictureOpacityRamp();

            auto const width = ramp.ActualWidth();
            auto const height = ramp.ActualHeight();

            if (width < 4.0 || height < 4.0 || ramp.XamlRoot() == nullptr)
            {
                return;
            }

            auto const scale = std::max(ramp.XamlRoot().RasterizationScale(), 1.0);
            auto const pixelWidth = static_cast<int32_t>(std::ceil(width * scale));
            auto const pixelHeight = static_cast<int32_t>(std::ceil(height * scale));
            auto const square = std::max((height * scale) / 2.0, 1.0);

            auto const dark = ramp.ActualTheme() == xaml::ElementTheme::Dark;

            // Light squares on a dark theme would wash out against a white solid end, and the
            // reverse on a light one, so the board follows the theme.
            uint8_t const firstSquare = dark ? 0x5A : 0xFF;
            uint8_t const secondSquare = dark ? 0x3A : 0xCC;
            auto const solid = ThemeColorOf(L"TextFillColorPrimaryBrush");

            imaging::WriteableBitmap bitmap{ pixelWidth, pixelHeight };

            auto* const pixels = bitmap.PixelBuffer().data();

            for (int32_t y = 0; y < pixelHeight; ++y)
            {
                for (int32_t x = 0; x < pixelWidth; ++x)
                {
                    auto const checker = ((static_cast<int32_t>(x / square) + static_cast<int32_t>(y / square)) % 2) == 0
                        ? firstSquare
                        : secondSquare;

                    auto const amount = pixelWidth > 1 ? static_cast<double>(x) / (pixelWidth - 1) : 1.0;

                    auto const mix = [&](uint8_t channel)
                        {
                            return static_cast<uint8_t>(std::lround((checker * (1.0 - amount)) + (channel * amount)));
                        };

                    auto* const pixel = pixels + ((static_cast<size_t>(y) * pixelWidth + x) * 4);

                    pixel[0] = mix(solid.B);
                    pixel[1] = mix(solid.G);
                    pixel[2] = mix(solid.R);
                    pixel[3] = 0xFF;
                }
            }

            bitmap.Invalidate();

            media::ImageBrush brush{};
            brush.ImageSource(bitmap);
            brush.Stretch(media::Stretch::Fill);

            ramp.Fill(brush);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the opacity track.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureOpacityRampSizeChanged(
        foundation::IInspectable const& sender,
        xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        DrawPictureOpacityRamp();
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCropBoxSizeChanged(
        foundation::IInspectable const& sender,
        xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        DrawPictureCropBox();
    }

    // ---------------------------------------------------------------- editing

    _Use_decl_annotations_
    void EditorWindow::SetPictureCenter(double centerX, double centerY, bool coalesce)
    {
        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr || control->Image.IsEmpty())
        {
            return;
        }

        auto picture = control->Image;

        picture.CenterX = std::clamp(centerX, 0.0, 1.0);
        picture.CenterY = std::clamp(centerY, 0.0, 1.0);

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture, coalesce); });
    }

    // Where the middle of the box should be, in the crop box's own coordinates.
    _Use_decl_annotations_
    void EditorWindow::MovePictureCropTo(double boxX, double boxY)
    {
        auto const* const control = SingleSelectedControl();

        if (control == nullptr || control->Image.IsEmpty())
        {
            return;
        }

        auto const centerX = CenterAlong(
            boxX, m_cropImageLeft, m_cropImageWidth, m_cropWindowWidth, control->Image.CenterX);

        auto const centerY = CenterAlong(
            boxY, m_cropImageTop, m_cropImageHeight, m_cropWindowHeight, control->Image.CenterY);

        SetPictureCenter(centerX, centerY, true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCropPressed(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr || control->Image.IsEmpty())
            {
                return;
            }

            auto const point = args.GetCurrentPoint(PictureCropBox()).Position();
            auto const x = static_cast<double>(point.X);
            auto const y = static_cast<double>(point.Y);

            auto const inside =
                x >= m_cropWindowLeft && x <= m_cropWindowLeft + m_cropWindowWidth &&
                y >= m_cropWindowTop && y <= m_cropWindowTop + m_cropWindowHeight;

            // Taking hold of the box keeps the grip where it was taken. Pressing anywhere else
            // brings the box to the pointer, the way the navigator in a paint program does.
            m_cropGrabOffsetX = inside ? x - (m_cropWindowLeft + (m_cropWindowWidth * 0.5)) : 0.0;
            m_cropGrabOffsetY = inside ? y - (m_cropWindowTop + (m_cropWindowHeight * 0.5)) : 0.0;

            PictureCropFrame().Focus(xaml::FocusState::Pointer);

            m_cropDragging = PictureCropBox().CapturePointer(args.Pointer());

            MovePictureCropTo(x - m_cropGrabOffsetX, y - m_cropGrabOffsetY);

            args.Handled(true);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to start moving the crop.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCropMoved(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_cropDragging)
        {
            return;
        }

        try
        {
            auto const point = args.GetCurrentPoint(PictureCropBox()).Position();

            MovePictureCropTo(
                static_cast<double>(point.X) - m_cropGrabOffsetX,
                static_cast<double>(point.Y) - m_cropGrabOffsetY);

            args.Handled(true);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to move the crop.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCropReleased(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_cropDragging)
        {
            return;
        }

        m_cropDragging = false;

        try
        {
            PictureCropBox().ReleasePointerCapture(args.Pointer());
        }
        catch (...)
        {
        }

        // The whole drag is one undo step.
        m_editor.EndCoalescing();

        args.Handled(true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCropCaptureLost(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_cropDragging)
        {
            m_cropDragging = false;
            m_editor.EndCoalescing();
        }
    }

    // The arrow keys move the middle a percent at a time, or ten with Shift. On an axis where
    // the picture fits inside the control, that moves the picture through the spare space
    // instead, which is the same number doing its other job.
    _Use_decl_annotations_
    void EditorWindow::OnPictureCropKeyDown(
        foundation::IInspectable const& sender,
        xaml::Input::KeyRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr || control->Image.IsEmpty())
            {
                return;
            }

            auto const step = (::GetKeyState(VK_SHIFT) < 0) ? 0.1 : 0.01;

            auto deltaX = 0.0;
            auto deltaY = 0.0;

            switch (args.Key())
            {
            case winrt::Windows::System::VirtualKey::Left:
                deltaX = -step;
                break;

            case winrt::Windows::System::VirtualKey::Right:
                deltaX = step;
                break;

            case winrt::Windows::System::VirtualKey::Up:
                deltaY = -step;
                break;

            case winrt::Windows::System::VirtualKey::Down:
                deltaY = step;
                break;

            default:
                return;
            }

            SetPictureCenter(
                ToPercent(control->Image.CenterX + deltaX),
                ToPercent(control->Image.CenterY + deltaY),
                true);

            args.Handled(true);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to move the crop.")
    }
}
