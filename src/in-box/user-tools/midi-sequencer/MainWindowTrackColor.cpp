// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Any color for a track (October 22, Pete), beside the palette's ten: the same color picker the
// Appearance settings use for the window's color. The track changes as the color does, and the
// whole change is one undo step when the picker closes.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    _Use_decl_annotations_
    void MainWindow::ShowTrackColorPicker(std::wstring const& trackId, xaml::UIElement const& anchor, std::optional<foundation::Point> point) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr)
            {
                return;
            }

            auto const original = track->Color;

            // Set up as AppearanceFlyout's picker is (midi-app-shared).
            controls::ColorPicker picker{};
            picker.IsAlphaEnabled(false);
            picker.IsColorSliderVisible(true);
            picker.IsColorChannelTextInputVisible(true);
            picker.IsHexInputVisible(true);
            picker.ColorSpectrumShape(controls::ColorSpectrumShape::Box);
            picker.Orientation(controls::Orientation::Horizontal);
            picker.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            picker.Margin(xaml::Thickness{ 0, 0, 0, -56 });
            picker.Color(winrt::Windows::UI::Color{ 0xFF,
                static_cast<uint8_t>((original >> 16) & 0xFF), static_cast<uint8_t>((original >> 8) & 0xFF), static_cast<uint8_t>(original & 0xFF) });
            automation::AutomationProperties::SetName(picker, res::FormatString(L"TrackColorPickerNameFormat", track->Name));

            controls::TextBlock title{};
            title.Text(res::FormatString(L"TrackColorPickerTitleFormat", track->Name));
            title.Style(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"BodyStrongTextBlockStyle")).as<xaml::Style>());

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Children().Append(title);
            panel.Children().Append(picker);

            controls::Flyout flyout{};
            flyout.Content(panel);
            midiapp::UseMenuBackdrop(flyout);

            auto weak = get_weak();

            // While it's open the track shows the color, without an undo step for every move.
            picker.ColorChanged([weak, trackId](controls::ColorPicker const&, controls::ColorChangedEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    if (auto t = seq::FindTrack(strong->m_doc, trackId); t != nullptr)
                    {
                        auto const c = args.NewColor();
                        t->Color = (static_cast<uint32_t>(c.R) << 16) | (static_cast<uint32_t>(c.G) << 8) | c.B;
                        strong->m_renderer.InvalidateCaches();
                        strong->InvalidateArrange();
                        strong->InvalidateLaunchers();
                    }
                }
            });

            flyout.Closed([weak, trackId, original](auto&&, auto&&)
            {
                auto strong = weak.get();

                if (!strong)
                {
                    return;
                }

                auto const t = seq::FindTrack(strong->m_doc, trackId);

                if (t == nullptr || t->Color == original)
                {
                    return;
                }

                // Back to where it was, then forward again as one undoable change.
                auto const chosen = t->Color;
                t->Color = original;

                strong->EditTracks(std::wstring{ res::GetString(L"UndoColor") }, [&](seq::Sequence& doc)
                {
                    if (auto edited = seq::FindTrack(doc, trackId); edited != nullptr)
                    {
                        edited->Color = chosen;
                    }
                });
            });

            if (point.has_value())
            {
                controls::Primitives::FlyoutShowOptions options{};
                options.Position(*point);
                flyout.ShowAt(anchor, options);
            }
            else
            {
                flyout.ShowAt(anchor.as<xaml::FrameworkElement>());
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the track color picker.")
    }
}
