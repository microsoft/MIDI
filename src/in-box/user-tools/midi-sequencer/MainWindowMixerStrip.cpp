// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The MIDI Mixer's strips, drawn the way a mixing desk draws them (comp 9): knobs for the sends
// and pan, a long fader with a scale, a meter for the notes the track is holding, and the track's
// name on its color at the bottom. Each knob and the fader is a Slider underneath, drawn by code,
// so the keyboard, focus and screen readers work as they do for any slider (design section 26).

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "MixerSlots.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    using namespace mixer;

    namespace
    {
        constexpr double StripWidth = 92.0;
        constexpr double SmallKnobSize = 24.0;
        constexpr double PanKnobSize = 34.0;
        constexpr double CapWidth = 28.0;
        constexpr double CapHeight = 15.0;
        constexpr double FaderMargin = 4.0;
        constexpr int32_t MeterSegments = 24;

        // The meter's top segments are yellow, for the hardest notes.
        constexpr double MeterHotFrom = 0.82;

        // Where the fader's groove runs across its area: a little right of the middle, to leave
        // room for the scale on the left and the meter on the right.
        constexpr double FaderAreaWidth = StripWidth - 2.0 - 2.0 * FaderMargin;
        constexpr double Groove = FaderAreaWidth / 2.0 + 2.0;

        // A drag this far moves a knob from one end to the other. With Shift, four times as far.
        constexpr double KnobDragPixels = 160.0;

        constexpr double Pi = 3.14159265358979323846;

        // Every knob and the fader show whatever their Header holds, and draw nothing else.
        constexpr wchar_t SliderTemplateXaml[] =
            LR"(<ControlTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" TargetType="Slider">)"
            LR"(<ContentPresenter Content="{TemplateBinding Header}" HorizontalAlignment="Stretch" VerticalAlignment="Stretch" />)"
            LR"(</ControlTemplate>)";

        // The strips' colors. The shapes and sizes are comp 9's (src/prototypes/midi-sequencer/
        // design/9-mixer.html). The cards, knobs, grooves, readout and buttons are see-through,
        // like the rest of the window, so they take on its color instead of a plain gray. The
        // readout matches the position and tempo displays, and M, S and W match the track
        // headers' buttons. The fader cap stays solid, like a real one.
        struct MixerLook
        {
            seq::Color StripTop{};
            seq::Color StripBottom{};
            seq::Color StripBorder{};
            seq::Color Knob{};
            seq::Color KnobRim{};
            seq::Color KnobShine{};
            seq::Color Track{};
            seq::Color Pointer{};
            seq::Color Groove{};
            seq::Color GrooveRim{};
            seq::Color CapTop{};
            seq::Color CapBottom{};
            seq::Color CapRim{};
            seq::Color CapLine{};
            seq::Color Tick{};
            seq::Color TickStrong{};
            seq::Color Lcd{};
            seq::Color LcdStroke{};
            seq::Color LcdText{};
            seq::Color Button{};
            seq::Color ButtonHover{};
            seq::Color ButtonBorder{};
            seq::Color ButtonText{};
            seq::Color MeterOff{};
            seq::Color MeterOn{};
            seq::Color MeterHot{};
        };

        MixerLook LookFor(seq::Palette const& palette)
        {
            using seq::FromRgb;
            using seq::Rgba;

            MixerLook look{};
            look.Pointer = palette.Text;
            look.LcdText = palette.Text;
            look.ButtonText = palette.Text3;
            look.ButtonBorder = palette.StrokeStrong;
            look.MeterOn = FromRgb(0x6CCB5F);
            look.MeterHot = FromRgb(0xF7C948);

            if (palette.Light)
            {
                // The editor and Track setup are white at 45%, so a strip is a little lighter.
                look.StripTop = Rgba(255, 255, 255, 0.70);
                look.StripBottom = Rgba(255, 255, 255, 0.45);
                look.StripBorder = Rgba(0, 0, 0, 0.10);
                look.Knob = Rgba(255, 255, 255, 0.85);
                look.KnobRim = Rgba(0, 0, 0, 0.22);
                look.KnobShine = Rgba(255, 255, 255, 0.90);
                look.Track = Rgba(0, 0, 0, 0.12);
                look.Groove = Rgba(0, 0, 0, 0.16);
                look.GrooveRim = Rgba(0, 0, 0, 0.10);
                look.CapTop = FromRgb(0xFFFFFF);
                look.CapBottom = FromRgb(0xDADADA);
                look.CapRim = Rgba(0, 0, 0, 0.35);
                look.CapLine = FromRgb(0x1A1A1A);
                look.Tick = Rgba(0, 0, 0, 0.25);
                look.TickStrong = Rgba(0, 0, 0, 0.55);
                look.Lcd = FromRgb(0xFFFFFF);
                look.LcdStroke = Rgba(0, 0, 0, 0.06);
                look.Button = Rgba(0, 0, 0, 0.03);
                look.ButtonHover = Rgba(0, 0, 0, 0.06);
                look.MeterOff = Rgba(0, 0, 0, 0.06);
                return look;
            }

            look.StripTop = Rgba(255, 255, 255, 0.06);
            look.StripBottom = Rgba(255, 255, 255, 0.025);
            look.StripBorder = Rgba(255, 255, 255, 0.10);
            look.Knob = Rgba(255, 255, 255, 0.09);
            look.KnobRim = Rgba(255, 255, 255, 0.16);
            look.KnobShine = Rgba(255, 255, 255, 0.08);
            look.Track = Rgba(255, 255, 255, 0.13);
            look.Groove = Rgba(0, 0, 0, 0.40);
            look.GrooveRim = Rgba(255, 255, 255, 0.10);
            look.CapTop = FromRgb(0x6A6A6A);
            look.CapBottom = FromRgb(0x3E3E3E);
            look.CapRim = Rgba(0, 0, 0, 0.55);
            look.CapLine = Rgba(255, 255, 255, 0.85);
            look.Tick = Rgba(255, 255, 255, 0.28);
            look.TickStrong = Rgba(255, 255, 255, 0.55);
            look.Lcd = Rgba(0, 0, 0, 0.32);
            look.LcdStroke = Rgba(255, 255, 255, 0.07);
            look.Button = Rgba(255, 255, 255, 0.04);
            look.ButtonHover = Rgba(255, 255, 255, 0.08);
            look.MeterOff = Rgba(255, 255, 255, 0.06);
            return look;
        }

        media::SolidColorBrush Brush(seq::Color const color)
        {
            return media::SolidColorBrush{ color };
        }

        media::LinearGradientBrush TopToBottom(seq::Color const top, seq::Color const bottom)
        {
            media::LinearGradientBrush brush{};
            brush.StartPoint(foundation::Point{ 0.5f, 0.0f });
            brush.EndPoint(foundation::Point{ 0.5f, 1.0f });

            media::GradientStop first{};
            first.Color(top);
            first.Offset(0.0);
            brush.GradientStops().Append(first);

            media::GradientStop last{};
            last.Color(bottom);
            last.Offset(1.0);
            brush.GradientStops().Append(last);

            return brush;
        }

        // 0 degrees is straight up; positive is clockwise.
        foundation::Point Polar(double const cx, double const cy, double const r, double const degrees)
        {
            auto const a = (degrees - 90.0) * Pi / 180.0;
            return foundation::Point{ static_cast<float>(cx + r * std::cos(a)), static_cast<float>(cy + r * std::sin(a)) };
        }

        media::Geometry Arc(double const c, double const r, double const from, double const to)
        {
            if (std::abs(to - from) < 0.5)
            {
                return nullptr;
            }

            auto const a = std::min(from, to);
            auto const b = std::max(from, to);

            media::PathFigure figure{};
            figure.StartPoint(Polar(c, c, r, a));
            figure.IsClosed(false);
            figure.IsFilled(false);

            media::ArcSegment arc{};
            arc.Point(Polar(c, c, r, b));
            arc.Size(foundation::Size{ static_cast<float>(r), static_cast<float>(r) });
            arc.IsLargeArc(b - a > 180.0);
            arc.SweepDirection(media::SweepDirection::Clockwise);
            figure.Segments().Append(arc);

            media::PathGeometry geometry{};
            geometry.Figures().Append(figure);
            return geometry;
        }

        // A knob turns from 7:30 to 4:30. Pan's middle is straight up.
        double KnobAngle(double const position, bool const fromCenter)
        {
            if (fromCenter)
            {
                return position >= 64.0 ? (position - 64.0) / 63.0 * 135.0 : (position - 64.0) / 64.0 * 135.0;
            }

            return -135.0 + 270.0 * position / 127.0;
        }

        // A knob's moving parts, drawn again when its value changes.
        struct KnobParts
        {
            shapes::Path Lit{ nullptr };
            shapes::Line Pointer{ nullptr };
            double Size{ SmallKnobSize };
            bool FromCenter{ false };
        };

        double KnobStroke(double const size)
        {
            return size > 26.0 ? 3.0 : 2.4;
        }

        void DrawKnob(KnobParts const& parts, double const position)
        {
            auto const c = parts.Size / 2.0;
            auto const r = c - 1.6;
            auto const body = r - KnobStroke(parts.Size) - 1.6;
            auto const angle = KnobAngle(position, parts.FromCenter);

            parts.Lit.Data(Arc(c, r, parts.FromCenter ? 0.0 : -135.0, angle));

            auto const start = Polar(c, c, body * 0.25, angle);
            auto const end = Polar(c, c, body * 0.92, angle);
            parts.Pointer.X1(start.X);
            parts.Pointer.Y1(start.Y);
            parts.Pointer.X2(end.X);
            parts.Pointer.Y2(end.Y);
        }

        // The fader's moving parts: its cap, placed again when the value or the height changes.
        struct FaderParts
        {
            media::TranslateTransform CapShift{ nullptr };
            double Height{ 0 };
        };

        double CapTravelTop() noexcept
        {
            return CapHeight / 2.0 + 1.0;
        }

        void PlaceCap(FaderParts const& parts, double const position)
        {
            auto const top = CapTravelTop();
            auto const bottom = std::max(top, parts.Height - top);
            auto const y = bottom - (bottom - top) * position / 127.0;
            parts.CapShift.Y(y - CapHeight / 2.0);
        }

        // Drags, double-clicks and the wheel, on top of the slider's own keyboard handling. The
        // drag is relative, so pressing a control never makes it jump.
        void AttachMoves(controls::Slider const& slider, std::function<double()> const& pixelsEndToEnd, double const standard)
        {
            struct Drag
            {
                bool Active{ false };
                double StartY{ 0 };
                double StartValue{ 0 };
            };

            auto const drag = std::make_shared<Drag>();
            winrt::weak_ref<controls::Slider> const weakSlider{ slider };

            slider.AddHandler(xaml::UIElement::PointerPressedEvent(), winrt::box_value(input::PointerEventHandler(
                [drag, weakSlider](foundation::IInspectable const&, input::PointerRoutedEventArgs const& args)
                {
                    auto const control = weakSlider.get();

                    if (!control || !control.IsEnabled())
                    {
                        return;
                    }

                    auto const point = args.GetCurrentPoint(control);

                    if (!point.Properties().IsLeftButtonPressed())
                    {
                        return;
                    }

                    drag->Active = true;
                    drag->StartY = point.Position().Y;
                    drag->StartValue = control.Value();
                    control.CapturePointer(args.Pointer());
                    control.Focus(xaml::FocusState::Pointer);
                    args.Handled(true);
                })), true);

            slider.AddHandler(xaml::UIElement::PointerMovedEvent(), winrt::box_value(input::PointerEventHandler(
                [drag, weakSlider, pixelsEndToEnd](foundation::IInspectable const&, input::PointerRoutedEventArgs const& args)
                {
                    auto const control = weakSlider.get();

                    if (!control || !drag->Active)
                    {
                        return;
                    }

                    auto const fine = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Shift) == winrt::Windows::System::VirtualKeyModifiers::Shift;
                    auto const span = std::max(1.0, pixelsEndToEnd()) * (fine ? 4.0 : 1.0);
                    auto const moved = drag->StartY - args.GetCurrentPoint(control).Position().Y;
                    control.Value(std::round(std::clamp(drag->StartValue + moved * 127.0 / span, 0.0, 127.0)));
                    args.Handled(true);
                })), true);

            auto const end = [drag, weakSlider](foundation::IInspectable const&, input::PointerRoutedEventArgs const& args)
            {
                drag->Active = false;

                if (auto const control = weakSlider.get())
                {
                    control.ReleasePointerCapture(args.Pointer());
                }
            };

            slider.AddHandler(xaml::UIElement::PointerReleasedEvent(), winrt::box_value(input::PointerEventHandler(end)), true);
            slider.AddHandler(xaml::UIElement::PointerCaptureLostEvent(), winrt::box_value(input::PointerEventHandler(
                [drag](foundation::IInspectable const&, input::PointerRoutedEventArgs const&)
                {
                    drag->Active = false;
                })), true);

            // A double-click puts it back where a General MIDI synth starts.
            slider.DoubleTapped([weakSlider, standard](foundation::IInspectable const&, input::DoubleTappedRoutedEventArgs const& args)
            {
                if (auto const control = weakSlider.get())
                {
                    control.Value(standard);
                    args.Handled(true);
                }
            });

            slider.PointerWheelChanged([weakSlider](foundation::IInspectable const&, input::PointerRoutedEventArgs const& args)
            {
                auto const control = weakSlider.get();

                if (!control)
                {
                    return;
                }

                auto const delta = args.GetCurrentPoint(control).Properties().MouseWheelDelta();
                auto const fine = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Shift) == winrt::Windows::System::VirtualKeyModifiers::Shift;
                auto const step = (delta > 0 ? 1.0 : delta < 0 ? -1.0 : 0.0) * (fine ? 1.0 : 2.0);
                control.Value(std::clamp(control.Value() + step, 0.0, 127.0));
                args.Handled(true);
            });
        }

        controls::Slider BaseSlider(controls::ControlTemplate const& shape)
        {
            controls::Slider slider{};
            slider.Template(shape);
            slider.Minimum(0);
            slider.Maximum(127);
            slider.StepFrequency(1);
            slider.SmallChange(1);
            slider.LargeChange(8);
            slider.IsThumbToolTipEnabled(false);
            slider.UseSystemFocusVisuals(true);
            slider.IsTabStop(true);
            return slider;
        }

        controls::Slider MakeKnob(controls::ControlTemplate const& shape, MixerLook const& look, seq::Color const accent, double const size, bool const fromCenter,
            std::shared_ptr<KnobParts>& parts)
        {
            auto slider = BaseSlider(shape);
            slider.Width(size);
            slider.Height(size);

            auto const c = size / 2.0;
            auto const r = c - 1.6;
            auto const stroke = KnobStroke(size);
            auto const body = r - stroke - 1.6;

            controls::Canvas canvas{};
            canvas.Width(size);
            canvas.Height(size);
            canvas.Background(Brush(winrt::Microsoft::UI::Colors::Transparent()));

            shapes::Path track{};
            track.Data(Arc(c, r, -135.0, 135.0));
            track.Stroke(Brush(look.Track));
            track.StrokeThickness(stroke);
            track.StrokeStartLineCap(media::PenLineCap::Round);
            track.StrokeEndLineCap(media::PenLineCap::Round);
            canvas.Children().Append(track);

            shapes::Path lit{};
            lit.Stroke(Brush(accent));
            lit.StrokeThickness(stroke);
            lit.StrokeStartLineCap(media::PenLineCap::Round);
            lit.StrokeEndLineCap(media::PenLineCap::Round);
            canvas.Children().Append(lit);

            // Pan's middle is marked, like the center detent on a desk.
            if (fromCenter)
            {
                auto const mark = Polar(c, c, r + 2.6, 0.0);
                shapes::Ellipse dot{};
                dot.Width(2.2);
                dot.Height(2.2);
                dot.Fill(Brush(look.TickStrong));
                controls::Canvas::SetLeft(dot, mark.X - 1.1);
                controls::Canvas::SetTop(dot, mark.Y - 1.1);
                canvas.Children().Append(dot);
            }

            shapes::Ellipse knob{};
            knob.Width(body * 2.0);
            knob.Height(body * 2.0);
            knob.Fill(Brush(look.Knob));
            knob.Stroke(Brush(look.KnobRim));
            knob.StrokeThickness(1);
            controls::Canvas::SetLeft(knob, c - body);
            controls::Canvas::SetTop(knob, c - body);
            canvas.Children().Append(knob);

            auto const shineRadius = body * 0.55;
            shapes::Ellipse shine{};
            shine.Width(shineRadius * 2.0);
            shine.Height(shineRadius * 2.0);
            shine.Fill(Brush(look.KnobShine));
            controls::Canvas::SetLeft(shine, c - body * 0.22 - shineRadius);
            controls::Canvas::SetTop(shine, c - body * 0.28 - shineRadius);
            canvas.Children().Append(shine);

            shapes::Line pointer{};
            pointer.Stroke(Brush(look.Pointer));
            pointer.StrokeThickness(2);
            pointer.StrokeStartLineCap(media::PenLineCap::Round);
            pointer.StrokeEndLineCap(media::PenLineCap::Round);
            canvas.Children().Append(pointer);

            slider.Header(canvas);

            parts = std::make_shared<KnobParts>(KnobParts{ lit, pointer, size, fromCenter });
            DrawKnob(*parts, slider.Value());
            return slider;
        }

        // The fader's groove and cap, inside the slider. The scale and the meter sit beside it.
        controls::Slider MakeFader(controls::ControlTemplate const& shape, MixerLook const& look, std::shared_ptr<FaderParts>& parts)
        {
            auto slider = BaseSlider(shape);
            slider.Orientation(controls::Orientation::Vertical);
            slider.Width(CapWidth);
            slider.VerticalAlignment(xaml::VerticalAlignment::Stretch);
            slider.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            slider.Margin(xaml::Thickness{ Groove - CapWidth / 2.0, 0, 0, 0 });

            controls::Grid inside{};
            inside.Background(Brush(winrt::Microsoft::UI::Colors::Transparent()));

            shapes::Rectangle groove{};
            groove.Width(5);
            groove.RadiusX(2.5);
            groove.RadiusY(2.5);
            groove.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            groove.Margin(xaml::Thickness{ 0, CapTravelTop() - 2.0, 0, CapTravelTop() - 2.0 });
            groove.Fill(Brush(look.Groove));
            groove.Stroke(Brush(look.GrooveRim));
            groove.StrokeThickness(1);
            inside.Children().Append(groove);

            controls::Grid cap{};
            cap.Width(CapWidth);
            cap.Height(CapHeight);
            cap.VerticalAlignment(xaml::VerticalAlignment::Top);

            media::TranslateTransform shift{};
            cap.RenderTransform(shift);

            shapes::Rectangle face{};
            face.RadiusX(3);
            face.RadiusY(3);
            face.Fill(TopToBottom(look.CapTop, look.CapBottom));
            face.Stroke(Brush(look.CapRim));
            face.StrokeThickness(1);
            cap.Children().Append(face);

            shapes::Rectangle line{};
            line.Height(1.4);
            line.Margin(xaml::Thickness{ 4, 0, 4, 0 });
            line.VerticalAlignment(xaml::VerticalAlignment::Center);
            line.Fill(Brush(look.CapLine));
            cap.Children().Append(line);

            inside.Children().Append(cap);
            slider.Header(inside);

            parts = std::make_shared<FaderParts>(FaderParts{ shift, 0 });
            return slider;
        }

        // The scale beside the fader: 0 to 127, with 100 marked where General MIDI synths start.
        void DrawScale(controls::Canvas const& scale, MixerLook const& look, double const height)
        {
            scale.Children().Clear();

            auto const top = CapTravelTop();
            auto const bottom = std::max(top, height - top);
            auto const yOf = [top, bottom](double value) { return bottom - (bottom - top) * value / 127.0; };

            for (int32_t value = 8; value < 127; value += 8)
            {
                if (value == 32 || value == 64 || value == 96)
                {
                    continue;
                }

                shapes::Line tick{};
                tick.X1(Groove - 15.0);
                tick.X2(Groove - 12.0);
                tick.Y1(yOf(value));
                tick.Y2(yOf(value));
                tick.Stroke(Brush(look.Tick));
                tick.StrokeThickness(1);
                tick.Opacity(0.6);
                scale.Children().Append(tick);
            }

            for (int32_t const value : { 127, 100, 64, 32, 0 })
            {
                auto const strong = value == Volume.Standard;
                auto const y = yOf(value);

                shapes::Line tick{};
                tick.X1(Groove - 18.0);
                tick.X2(Groove - (strong ? 9.0 : 12.0));
                tick.Y1(y);
                tick.Y2(y);
                tick.Stroke(Brush(strong ? look.TickStrong : look.Tick));
                tick.StrokeThickness(1);
                scale.Children().Append(tick);

                controls::TextBlock label{};
                label.Text(winrt::hstring{ std::to_wstring(value) });
                label.FontSize(9);
                label.Width(20);
                label.TextAlignment(xaml::TextAlignment::Right);
                label.Foreground(Brush(strong ? look.TickStrong : look.Tick));
                controls::Canvas::SetLeft(label, Groove - 21.0 - 20.0);
                controls::Canvas::SetTop(label, y - 6.5);
                scale.Children().Append(label);
            }
        }

        // Black or white text on a track's color, whichever reads better (WCAG contrast).
        seq::Color TextOn(uint32_t const rgb)
        {
            auto const channel = [](uint32_t value)
            {
                auto const c = static_cast<double>(value & 0xFF) / 255.0;
                return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
            };

            auto const luminance = 0.2126 * channel(rgb >> 16) + 0.7152 * channel(rgb >> 8) + 0.0722 * channel(rgb);
            constexpr double DarkLuminance = 0.0103;

            return (luminance + 0.05) / (DarkLuminance + 0.05) >= 4.5 ? seq::FromRgb(0x1A1A1A) : seq::FromRgb(0xFFFFFF);
        }
    }

    _Use_decl_annotations_
    void MainWindow::ColorMixerPlate(MixerStripView const& view, uint32_t rgb)
    {
        auto const color = rgb & 0xFFFFFF;
        auto const text = BrushFor(TextOn(color));

        view.Bar.Fill(BrushFor(seq::FromRgb(color)));
        view.Name.Foreground(text);
        view.Destination.Foreground(text);
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeMixerStrip(seq::Track const& track)
    {
        auto weak = get_weak();
        auto const trackId = track.Id;
        auto const trackName = winrt::hstring{ track.Name };
        auto const look = LookFor(m_palette);
        auto const text3 = BrushFor(m_palette.Text3);

        if (m_mixerSliderTemplate == nullptr)
        {
            m_mixerSliderTemplate = xaml::Markup::XamlReader::Load(SliderTemplateXaml).as<controls::ControlTemplate>();
        }

        // The outline shows when the track is selected.
        controls::Border frame{};
        frame.Width(StripWidth + 6.0);
        frame.Padding(xaml::Thickness{ 1, 1, 1, 1 });
        frame.CornerRadius(xaml::CornerRadius{ 9, 9, 9, 9 });
        frame.BorderThickness(xaml::Thickness{ 2, 2, 2, 2 });
        frame.BorderBrush(Brush(winrt::Microsoft::UI::Colors::Transparent()));

        controls::Border strip{};
        strip.CornerRadius(xaml::CornerRadius{ 7, 7, 7, 7 });
        strip.BorderThickness(xaml::Thickness{ 1, 1, 1, 1 });
        strip.BorderBrush(Brush(look.StripBorder));
        strip.Background(TopToBottom(look.StripTop, look.StripBottom));
        frame.Child(strip);

        controls::Grid grid{};

        for (auto const height : { xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::Auto(), xaml::GridLength{ 1, xaml::GridUnitType::Star },
            xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::Auto() })
        {
            controls::RowDefinition row{};
            row.Height(height);
            grid.RowDefinitions().Append(row);
        }

        auto const place = [&grid](xaml::FrameworkElement const& element, int32_t row)
        {
            controls::Grid::SetRow(element, row);
            grid.Children().Append(element);
        };

        auto const laneDot = [this]()
        {
            shapes::Ellipse mark{};
            mark.Width(5);
            mark.Height(5);
            mark.Fill(BrushFor(m_palette.Accent));
            mark.Visibility(xaml::Visibility::Collapsed);
            mark.IsHitTestVisible(false);
            controls::ToolTipService::SetToolTip(mark, winrt::box_value(res::GetString(L"MixerFollowsLane")));
            return mark;
        };

        // Moving a control sends it, unless the window is the one moving it.
        auto const sendWhenMoved = [weak, trackId](controls::Slider const& slider, uint8_t controller, std::function<void(double)> const& draw)
        {
            slider.ValueChanged([weak, trackId, controller, draw](auto&&, primitives::RangeBaseValueChangedEventArgs const& args)
            {
                draw(args.NewValue());

                auto strong = weak.get();

                if (strong && !strong->m_mixerUpdating)
                {
                    strong->OnMixerValueChanged(trackId, controller, ToValue(args.NewValue()));
                }
            });
        };

        auto const nameControl = [&trackName](controls::Slider const& slider, MixerSlot const& slot)
        {
            automation::AutomationProperties::SetName(slider, res::FormatString(L"MixerControlNameFormat", res::GetString(slot.NameKey), trackName));
            controls::ToolTipService::SetToolTip(slider, winrt::box_value(res::FormatString(L"MixerControlTipFormat", res::GetString(slot.NameKey), slot.Controller)));
            slider.Value(slot.Standard);
        };

        // ---- expression, reverb and chorus ----

        controls::Grid knobs{};
        knobs.Padding(xaml::Thickness{ 7, 9, 7, 0 });

        for (size_t i = 0; i < Knobs.size(); ++i)
        {
            controls::ColumnDefinition column{};
            column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
            knobs.ColumnDefinitions().Append(column);

            auto const& slot = Knobs[i];
            std::shared_ptr<KnobParts> parts{};
            auto knob = MakeKnob(m_mixerSliderTemplate, look, m_palette.Accent, SmallKnobSize, false, parts);
            knob.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            nameControl(knob, slot);
            AttachMoves(knob, []() { return KnobDragPixels; }, slot.Standard);
            sendWhenMoved(knob, slot.Controller, [parts](double position) { DrawKnob(*parts, position); });
            DrawKnob(*parts, slot.Standard);

            controls::StackPanel cell{};
            cell.Spacing(1);
            cell.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            cell.Children().Append(knob);

            controls::TextBlock label{};
            label.Text(res::GetString(slot.ShortKey));
            label.FontSize(10);
            label.Foreground(text3);
            label.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            cell.Children().Append(label);

            // A dot by the knob when it follows an automation lane.
            controls::Grid holder{};
            holder.Children().Append(cell);
            auto mark = laneDot();
            mark.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            mark.VerticalAlignment(xaml::VerticalAlignment::Top);
            holder.Children().Append(mark);

            controls::Grid::SetColumn(holder, static_cast<int32_t>(i));
            knobs.Children().Append(holder);

            MixerControl control{};
            control.TrackId = trackId;
            control.Controller = slot.Controller;
            control.Slider = knob;
            control.LaneMark = mark;
            m_mixerControls.push_back(std::move(control));
        }

        place(knobs, 0);

        // ---- pan, lit from the middle, with L, C or R beside it ----

        controls::Grid pan{};
        pan.Padding(xaml::Thickness{ 9, 9, 8, 0 });
        pan.ColumnSpacing(7);

        for (auto const width : { xaml::GridLengthHelper::Auto(), xaml::GridLength{ 1, xaml::GridUnitType::Star } })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            pan.ColumnDefinitions().Append(column);
        }

        controls::TextBlock panValue{};
        panValue.FontSize(12);
        panValue.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        panValue.FontFamily(media::FontFamily{ L"Cascadia Mono,Consolas" });
        panValue.Text(winrt::hstring{ ValueText(Pan.Controller, Pan.Standard) });

        std::shared_ptr<KnobParts> panParts{};
        auto panKnob = MakeKnob(m_mixerSliderTemplate, look, m_palette.Accent, PanKnobSize, true, panParts);
        nameControl(panKnob, Pan);
        AttachMoves(panKnob, []() { return KnobDragPixels; }, Pan.Standard);

        sendWhenMoved(panKnob, Pan.Controller, [panParts, weakText = winrt::make_weak(panValue), weakKnob = winrt::make_weak(panKnob)](double position)
        {
            DrawKnob(*panParts, position);

            if (auto const text = weakText.get())
            {
                text.Text(winrt::hstring{ ValueText(Pan.Controller, position) });
            }

            // A screen reader hears "left 24" rather than a number from 0 to 127.
            if (auto const knob = weakKnob.get())
            {
                automation::AutomationProperties::SetItemStatus(knob, winrt::hstring{ SpokenPan(position) });
            }
        });

        DrawKnob(*panParts, Pan.Standard);
        automation::AutomationProperties::SetItemStatus(panKnob, winrt::hstring{ SpokenPan(Pan.Standard) });

        controls::Grid panHolder{};
        panHolder.Children().Append(panKnob);
        auto panMark = laneDot();
        panMark.HorizontalAlignment(xaml::HorizontalAlignment::Right);
        panMark.VerticalAlignment(xaml::VerticalAlignment::Top);
        panHolder.Children().Append(panMark);
        pan.Children().Append(panHolder);

        controls::StackPanel panWords{};
        panWords.Spacing(1);
        panWords.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::TextBlock panLabel{};
        panLabel.Text(res::GetString(Pan.ShortKey));
        panLabel.FontSize(10);
        panLabel.Foreground(text3);
        panWords.Children().Append(panLabel);
        panWords.Children().Append(panValue);

        controls::Grid::SetColumn(panWords, 1);
        pan.Children().Append(panWords);
        place(pan, 1);

        MixerControl panControl{};
        panControl.TrackId = trackId;
        panControl.Controller = Pan.Controller;
        panControl.Slider = panKnob;
        panControl.Value = panValue;
        panControl.LaneMark = panMark;
        m_mixerControls.push_back(std::move(panControl));

        // ---- the volume fader, with its scale and the meter ----

        controls::Grid area{};
        area.Margin(xaml::Thickness{ FaderMargin, 10, FaderMargin, 0 });
        area.MinHeight(48);

        controls::Canvas scale{};
        scale.IsHitTestVisible(false);
        area.Children().Append(scale);

        std::shared_ptr<FaderParts> faderParts{};
        auto fader = MakeFader(m_mixerSliderTemplate, look, faderParts);
        nameControl(fader, Volume);
        AttachMoves(fader, [faderParts]() { return faderParts->Height - CapHeight - 2.0; }, Volume.Standard);
        area.Children().Append(fader);

        // How hard the track's notes play, segment by segment from the bottom. The clear padding
        // gives the tooltip something wider than 4 pixels to land on.
        controls::Grid meter{};
        meter.Width(12);
        meter.Padding(xaml::Thickness{ 4, 0, 4, 0 });
        meter.Background(Brush(winrt::Microsoft::UI::Colors::Transparent()));
        meter.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        meter.Margin(xaml::Thickness{ Groove + 15.0, CapTravelTop() - 2.0, 0, CapTravelTop() - 2.0 });
        automation::AutomationProperties::SetAccessibilityView(meter, automation::Peers::AccessibilityView::Raw);
        controls::ToolTipService::SetToolTip(meter, winrt::box_value(res::GetString(L"MixerMeterToolTip")));

        std::vector<shapes::Rectangle> segments{};
        auto const offBrush = Brush(look.MeterOff);

        for (int32_t i = 0; i < MeterSegments; ++i)
        {
            controls::RowDefinition row{};
            row.Height(xaml::GridLength{ 1, xaml::GridUnitType::Star });
            meter.RowDefinitions().Append(row);
        }

        for (int32_t i = 0; i < MeterSegments; ++i)
        {
            shapes::Rectangle segment{};
            segment.RadiusX(0.8);
            segment.RadiusY(0.8);
            segment.Margin(xaml::Thickness{ 0, 0.6, 0, 0.6 });
            segment.Fill(offBrush);
            controls::Grid::SetRow(segment, MeterSegments - 1 - i);
            meter.Children().Append(segment);
            segments.push_back(segment);
        }

        area.Children().Append(meter);

        controls::TextBlock readoutText{};
        readoutText.FontSize(12);
        readoutText.FontFamily(media::FontFamily{ L"Cascadia Mono,Consolas" });
        readoutText.Foreground(Brush(look.LcdText));
        readoutText.Text(winrt::hstring{ ValueText(Volume.Controller, Volume.Standard) });

        sendWhenMoved(fader, Volume.Controller, [faderParts, weakText = winrt::make_weak(readoutText)](double position)
        {
            PlaceCap(*faderParts, position);

            if (auto const text = weakText.get())
            {
                text.Text(winrt::hstring{ ValueText(Volume.Controller, position) });
            }
        });

        area.SizeChanged([weakScale = winrt::make_weak(scale), weakFader = winrt::make_weak(fader), faderParts, look](foundation::IInspectable const&, xaml::SizeChangedEventArgs const& args)
        {
            faderParts->Height = args.NewSize().Height;

            if (auto const canvas = weakScale.get())
            {
                DrawScale(canvas, look, faderParts->Height);
            }

            if (auto const control = weakFader.get())
            {
                PlaceCap(*faderParts, control.Value());
            }
        });

        place(area, 2);

        // ---- the volume as a number ----

        controls::Border readout{};
        readout.HorizontalAlignment(xaml::HorizontalAlignment::Center);
        readout.MinWidth(46);
        readout.Height(20);
        readout.Margin(xaml::Thickness{ 0, 7, 0, 0 });
        readout.Padding(xaml::Thickness{ 6, 0, 6, 0 });
        readout.CornerRadius(xaml::CornerRadius{ 4, 4, 4, 4 });
        readout.Background(Brush(look.Lcd));
        readout.BorderBrush(Brush(look.LcdStroke));
        readout.BorderThickness(xaml::Thickness{ 1, 1, 1, 1 });

        controls::StackPanel readoutRow{};
        readoutRow.Orientation(controls::Orientation::Horizontal);
        readoutRow.HorizontalAlignment(xaml::HorizontalAlignment::Center);
        readoutRow.VerticalAlignment(xaml::VerticalAlignment::Center);
        readoutRow.Spacing(5);

        auto volumeMark = laneDot();
        volumeMark.VerticalAlignment(xaml::VerticalAlignment::Center);
        readoutRow.Children().Append(volumeMark);
        readoutRow.Children().Append(readoutText);
        readout.Child(readoutRow);
        place(readout, 3);

        MixerControl volumeControl{};
        volumeControl.TrackId = trackId;
        volumeControl.Controller = Volume.Controller;
        volumeControl.Slider = fader;
        volumeControl.Value = readoutText;
        volumeControl.LaneMark = volumeMark;
        m_mixerControls.push_back(std::move(volumeControl));

        // ---- M, S and W ----

        controls::StackPanel buttons{};
        buttons.Orientation(controls::Orientation::Horizontal);
        buttons.HorizontalAlignment(xaml::HorizontalAlignment::Center);
        buttons.Spacing(4);
        buttons.Margin(xaml::Thickness{ 0, 8, 0, 8 });

        auto const makeFlag = [&](wchar_t const* caption, bool on, seq::Color onFill, seq::Color onText, winrt::hstring const& tip, std::function<void(bool)> changed)
        {
            primitives::ToggleButton toggle{};
            toggle.Width(24);
            toggle.Height(20);
            toggle.MinWidth(0);
            toggle.MinHeight(0);
            toggle.Padding(xaml::Thickness{ 0, 0, 0, 0 });
            toggle.CornerRadius(xaml::CornerRadius{ 4, 4, 4, 4 });
            toggle.FontSize(10);
            toggle.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
            toggle.Content(winrt::box_value(winrt::hstring{ caption }));
            toggle.IsChecked(on);

            auto const toggleResources = toggle.Resources();
            auto const set = [&toggleResources](std::initializer_list<wchar_t const*> keys, media::Brush const& brush)
            {
                for (auto const key : keys)
                {
                    toggleResources.Insert(winrt::box_value(key), brush);
                }
            };

            set({ L"ToggleButtonBackground", L"ToggleButtonBackgroundPressed" }, Brush(look.Button));
            set({ L"ToggleButtonBackgroundPointerOver" }, Brush(look.ButtonHover));
            set({ L"ToggleButtonBorderBrush", L"ToggleButtonBorderBrushPointerOver", L"ToggleButtonBorderBrushPressed" }, Brush(look.ButtonBorder));
            set({ L"ToggleButtonForeground", L"ToggleButtonForegroundPointerOver", L"ToggleButtonForegroundPressed" }, Brush(look.ButtonText));
            set({ L"ToggleButtonBackgroundChecked", L"ToggleButtonBackgroundCheckedPointerOver", L"ToggleButtonBackgroundCheckedPressed",
                L"ToggleButtonBorderBrushChecked", L"ToggleButtonBorderBrushCheckedPointerOver", L"ToggleButtonBorderBrushCheckedPressed" }, Brush(onFill));
            set({ L"ToggleButtonForegroundChecked", L"ToggleButtonForegroundCheckedPointerOver", L"ToggleButtonForegroundCheckedPressed" }, Brush(onText));

            automation::AutomationProperties::SetName(toggle, tip);
            controls::ToolTipService::SetToolTip(toggle, winrt::box_value(tip));

            toggle.Click([changed](foundation::IInspectable const& sender, auto&&)
            {
                auto const button = sender.try_as<primitives::ToggleButton>();
                changed(button != nullptr && button.IsChecked() != nullptr && button.IsChecked().GetBoolean());
            });

            buttons.Children().Append(toggle);
            return toggle;
        };

        auto const dark = seq::FromRgb(0x1A1A1A);

        MixerStripView view{};
        view.TrackId = trackId;
        view.Frame = frame;
        view.Meter = std::move(segments);

        view.Mute = makeFlag(L"M", track.Muted, m_palette.Mute, dark, res::FormatString(L"MuteTrackFormat", track.Name), [weak, trackId](bool on)
        {
            if (auto strong = weak.get()) { strong->ToggleTrackFlag(trackId, L'M', on); }
        });

        view.Solo = makeFlag(L"S", track.Soloed, m_palette.Solo, dark, res::FormatString(L"SoloTrackFormat", track.Name), [weak, trackId](bool on)
        {
            if (auto strong = weak.get()) { strong->ToggleTrackFlag(trackId, L'S', on); }
        });

        view.Write = makeFlag(L"W", m_automationArmed.contains(trackId), m_palette.Record, seq::FromRgb(0xFFFFFF), res::FormatString(L"MixerWriteFormat", track.Name), [weak, trackId](bool on)
        {
            if (auto strong = weak.get())
            {
                strong->CommitMixerEdit();

                if (on)
                {
                    strong->m_automationArmed.insert(trackId);
                }
                else
                {
                    strong->m_automationArmed.erase(trackId);
                }
            }
        });

        place(buttons, 4);

        // ---- the name plate, in the track's color; selecting it selects the track ----

        controls::Grid plate{};
        plate.Margin(xaml::Thickness{ 4, 0, 4, 4 });

        shapes::Rectangle bar{};
        bar.RadiusX(4);
        bar.RadiusY(4);
        plate.Children().Append(bar);

        controls::StackPanel words{};
        words.Padding(xaml::Thickness{ 6, 4, 6, 5 });

        controls::TextBlock name{};
        name.Text(trackName);
        name.FontSize(12);
        name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        words.Children().Append(name);

        controls::TextBlock destination{};
        destination.Text(winrt::hstring{ DescribeDestination(track) });
        destination.FontSize(10);
        destination.Opacity(0.72);
        destination.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        words.Children().Append(destination);

        plate.Children().Append(words);
        controls::ToolTipService::SetToolTip(plate, winrt::box_value(DestinationToolTip(track)));

        plate.PointerPressed([weak, trackId](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                strong->SelectTrack(trackId);
                args.Handled(true);
            }
        });

        place(plate, 5);

        view.Name = name;
        view.Destination = destination;
        view.Bar = bar;
        ColorMixerPlate(view, track.Color);
        m_mixerStrips.push_back(std::move(view));

        strip.Child(grid);
        automation::AutomationProperties::SetName(frame, trackName);
        return frame;
    }

    void MainWindow::UpdateMixerSelection() noexcept
    {
        try
        {
            for (auto& strip : m_mixerStrips)
            {
                auto const selected = strip.TrackId == m_selectedTrackId;

                if (strip.Frame == nullptr || (selected == strip.Selected && strip.SelectionShown))
                {
                    continue;
                }

                strip.Selected = selected;
                strip.SelectionShown = true;
                strip.Frame.BorderBrush(selected ? BrushFor(m_palette.Accent) : Brush(winrt::Microsoft::UI::Colors::Transparent()));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show which track is selected in the mixer.")
    }

    void MainWindow::UpdateMixerMeters() noexcept
    {
        try
        {
            if (m_mixerStrips.empty())
            {
                return;
            }

            std::vector<std::pair<std::wstring, float>> levels{};

            // Everything held since the last look, so a short note between two looks still shows.
            if (m_engine != nullptr && m_engine->IsPlaying())
            {
                auto const now = midi2::MidiClock::Now();
                auto const from = m_meterFrom != 0 && m_meterFrom < now ? m_meterFrom : now;
                levels = m_engine->HeldNoteLevels(from, now + 1);
                m_meterFrom = now;
            }
            else
            {
                m_meterFrom = 0;
            }

            auto const look = LookFor(m_palette);
            media::Brush on{ nullptr };
            media::Brush hot{ nullptr };
            media::Brush off{ nullptr };
            auto const hotFrom = static_cast<int32_t>(std::ceil(MeterHotFrom * MeterSegments));

            for (auto& strip : m_mixerStrips)
            {
                float level{ 0.0f };

                for (auto const& [id, value] : levels)
                {
                    if (id == strip.TrackId)
                    {
                        level = value;
                        break;
                    }
                }

                // Any note at all lights the bottom segment.
                auto const lit = level <= 0.0f ? 0 : std::clamp(static_cast<int32_t>(std::ceil(level * MeterSegments)), 1, MeterSegments);

                if (lit == strip.MeterLit)
                {
                    continue;
                }

                strip.MeterLit = lit;

                if (on == nullptr)
                {
                    on = Brush(look.MeterOn);
                    hot = Brush(look.MeterHot);
                    off = Brush(look.MeterOff);
                }

                for (int32_t i = 0; i < static_cast<int32_t>(strip.Meter.size()); ++i)
                {
                    strip.Meter[static_cast<size_t>(i)].Fill(i < lit ? (i >= hotFrom ? hot : on) : off);
                }
            }
        }
        catch (...)
        {
        }
    }
}
