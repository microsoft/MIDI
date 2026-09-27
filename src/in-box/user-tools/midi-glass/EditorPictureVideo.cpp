// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// A video's own settings in the picture panel: the play button, the strip that picks the part of
// the file that plays, and the switches for when it plays and what a click on it does.
//
// The designer holds every video still on the first frame of the part that plays. The play button
// here is the one way to watch a clip while building, and dragging an end of the strip shows the
// frame at that end on the canvas, so the part can be picked by eye rather than by number.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "LayoutStore.h"

#include <chrono>
#include <cmath>
#include <limits>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

        // The strip: room at each end for a whole bracket, the track along the middle, and the
        // brackets standing above and below it.
        constexpr double TimelineInset = 10.0;
        constexpr double TimelineTrackHeight = 8.0;
        constexpr double TimelineHandleWidth = 8.0;
        constexpr double TimelineHandleInset = 3.0;
        constexpr double TimelinePlayheadWidth = 2.0;

        // How often the line on the strip moves while the video plays.
        constexpr auto PlayheadInterval = std::chrono::milliseconds{ 100 };

        // A stop this close to the end of the file is the end of the file, so a clip that is
        // re-encoded a frame shorter still plays to its end. The same at the start.
        constexpr double SnapSeconds = 0.05;

        // Play and stop, from Segoe Fluent Icons.
        constexpr wchar_t PlayGlyph[] = L"\xE768";
        constexpr wchar_t StopGlyph[] = L"\xE71A";

        media::Brush ThemeBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        // To the hundredth, which is finer than a frame and short enough to read.
        double ToHundredths(_In_ double seconds) noexcept
        {
            return std::isfinite(seconds) ? std::round(seconds * 100.0) / 100.0 : 0.0;
        }

        shapes::Rectangle MakeBar(_In_ media::Brush const& fill, _In_ double radius)
        {
            shapes::Rectangle bar{};

            bar.Fill(fill);
            bar.RadiusX(radius);
            bar.RadiusY(radius);
            bar.IsHitTestVisible(false);

            automation::AutomationProperties::SetAccessibilityView(
                bar, automation::Peers::AccessibilityView::Raw);

            return bar;
        }

        void Place(
            _In_ xaml::FrameworkElement const& element,
            _In_ double x,
            _In_ double y,
            _In_ double width,
            _In_ double height)
        {
            element.Width(std::max(width, 0.0));
            element.Height(std::max(height, 0.0));

            controls::Canvas::SetLeft(element, x);
            controls::Canvas::SetTop(element, y);
        }

        bool ShowsAVideo(_In_ glass::Control const& control) noexcept
        {
            return (control.Kind == glass::ControlKind::Image || control.Kind == glass::ControlKind::Panel) &&
                !control.Image.IsEmpty() &&
                glass::IsVideoFileName(control.Image.FileName);
        }
    }

    // ---------------------------------------------------------------- where things are

    _Use_decl_annotations_
    bool EditorWindow::TryFindCanvasItem(std::wstring const& controlId, size_t& itemIndex) const
    {
        itemIndex = 0;

        auto const* const page = m_editor.CurrentPage();

        if (page == nullptr)
        {
            return false;
        }

        for (size_t index = 0; index < page->Controls.size() && index < m_renderer.ItemCount(); ++index)
        {
            if (page->Controls[index].Id == controlId)
            {
                itemIndex = index;
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    double EditorWindow::KnownVideoDuration(glass::Control const& control) const
    {
        if (m_videoDurationSeconds > 0.0)
        {
            return m_videoDurationSeconds;
        }

        size_t item{ 0 };
        double position{ 0.0 };
        double duration{ 0.0 };
        bool playing{ false };

        if (TryFindCanvasItem(control.Id, item) &&
            m_renderer.TryGetVideoPosition(item, position, duration, playing))
        {
            return duration;
        }

        return 0.0;
    }

    // ---------------------------------------------------------------- filled in

    void EditorWindow::RefreshPictureVideoPanel()
    {
        try
        {
            auto const* const control = SingleSelectedControl();

            // A clip the play button started stops when the inspector moves on. It was a look at
            // that one clip, and the inspector no longer shows a button to stop it with.
            if (!m_previewingVideoId.empty() && (control == nullptr || control->Id != m_previewingVideoId))
            {
                size_t previewed{ 0 };

                if (TryFindCanvasItem(m_previewingVideoId, previewed))
                {
                    m_renderer.SetVideoPlaying(previewed, false);
                }

                m_previewingVideoId.clear();
            }

            if (control == nullptr || !ShowsAVideo(*control))
            {
                StopVideoPlayheadTimer();
                return;
            }

            size_t item{ 0 };
            auto const onCanvas = TryFindCanvasItem(control->Id, item) && m_renderer.HasVideo(item);
            auto const playing = onCanvas && m_renderer.IsVideoPlaying(item);

            auto const buttonText = resources::GetString(playing ? L"PictureVideoStop" : L"PictureVideoPlay");

            // Named in code as well as shown, because a button whose content is an icon and a
            // text block is not named from the text block.
            PictureVideoPlayText().Text(buttonText);
            automation::AutomationProperties::SetName(PictureVideoPlayButton(), buttonText);
            PictureVideoPlayGlyph().Glyph(playing ? StopGlyph : PlayGlyph);
            PictureVideoPlayButton().IsEnabled(onCanvas);

            auto const duration = KnownVideoDuration(*control);
            auto const known = duration > 0.0;
            auto const range = glass::VideoPlayRange(control->Image, duration);

            // The boxes are set, not typed in, so their change handlers must not write it back.
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            auto const maximum = known ? ToHundredths(duration) : glass::MaximumVideoSeconds;

            PictureVideoStartBox().Maximum(maximum);
            PictureVideoEndBox().Maximum(maximum);
            PictureVideoStartBox().Value(ToHundredths(range.StartSeconds));

            // Empty says the end of the file, which is what a stop of zero means.
            PictureVideoEndBox().Value(control->Image.VideoEndSeconds > 0.0 && range.EndSeconds > 0.0
                ? ToHundredths(range.EndSeconds)
                : std::numeric_limits<double>::quiet_NaN());

            m_updatingInspector = previous;

            PictureVideoRangeCaption().Text(known
                ? resources::FormatString(
                    L"PictureVideoRangeFormat",
                    glass::FormatVideoTime(range.StartSeconds),
                    glass::FormatVideoTime(range.EndSeconds),
                    glass::FormatVideoTime(duration))
                : resources::GetString(L"PictureVideoReadingCaption"));

            // Nothing to drag until the length of the clip is known.
            PictureTimeline().IsHitTestVisible(known);

            DrawPictureTimeline();
            StartVideoPlayheadTimer();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the video's settings.")
    }

    void EditorWindow::ResumeVideoPreview()
    {
        if (m_previewingVideoId.empty() || m_tryMode)
        {
            return;
        }

        size_t item{ 0 };

        if (!TryFindCanvasItem(m_previewingVideoId, item) || !m_renderer.HasVideo(item))
        {
            m_previewingVideoId.clear();
            return;
        }

        m_renderer.SetVideoPlaying(item, true);
    }

    // ---------------------------------------------------------------- edits

    _Use_decl_annotations_
    void EditorWindow::OnPictureVideoPlayClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const* const control = SingleSelectedControl();
            size_t item{ 0 };

            if (control == nullptr || !TryFindCanvasItem(control->Id, item) || !m_renderer.HasVideo(item))
            {
                return;
            }

            auto const play = !m_renderer.IsVideoPlaying(item);

            m_renderer.SetVideoPlaying(item, play);

            // Remembered only in the designer, where the next rebuild would otherwise stop it.
            // Try mode plays every video by the layout's own rules.
            m_previewingVideoId = play && !m_tryMode ? control->Id : std::wstring{};

            RefreshPictureVideoPanel();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to play or stop the video.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureVideoFlagChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto picture = control->Image;

        picture.AutoPlays = PictureAutoPlayCheck().IsChecked().GetBoolean();
        picture.ClickToPlay = PictureClickToPlayCheck().IsChecked().GetBoolean();
        picture.ShowsScrubber = PictureScrubberCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureVideoRangeChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto const duration = KnownVideoDuration(*control);
        auto const start = PictureVideoStartBox().Value();
        auto const end = PictureVideoEndBox().Value();

        auto picture = control->Image;

        picture.VideoStartSeconds = std::isfinite(start) && start > SnapSeconds
            ? ToHundredths(start)
            : 0.0;

        // Empty, or the end of the file, is stored as zero, which keeps meaning the end if the
        // clip is ever swapped for a longer one.
        picture.VideoEndSeconds =
            !std::isfinite(end) || end <= 0.0 || (duration > 0.0 && end >= duration - SnapSeconds)
            ? 0.0
            : ToHundredths(end);

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    // ---------------------------------------------------------------- the strip

    void EditorWindow::DrawPictureTimeline()
    {
        try
        {
            auto const canvas = PictureTimelineCanvas();

            if (m_timelineTrack == nullptr)
            {
                auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush");

                m_timelineTrack = MakeBar(ThemeBrush(L"ControlStrongFillColorDefaultBrush"), TimelineTrackHeight * 0.5);
                m_timelineTrack.Opacity(0.35);

                m_timelineRange = MakeBar(accent, TimelineTrackHeight * 0.5);
                m_timelineRange.Opacity(0.5);

                m_timelinePlayhead = MakeBar(ThemeBrush(L"TextFillColorPrimaryBrush"), 1.0);
                m_timelineStartHandle = MakeBar(accent, 2.0);
                m_timelineEndHandle = MakeBar(accent, 2.0);

                canvas.Children().Append(m_timelineTrack);
                canvas.Children().Append(m_timelineRange);
                canvas.Children().Append(m_timelinePlayhead);
                canvas.Children().Append(m_timelineStartHandle);
                canvas.Children().Append(m_timelineEndHandle);
            }

            auto const* const control = SingleSelectedControl();
            auto const width = PictureTimeline().ActualWidth();
            auto const height = PictureTimeline().ActualHeight();
            auto const trackWidth = width - (2.0 * TimelineInset);

            auto const hideMarks = [&]()
                {
                    m_timelineRange.Visibility(xaml::Visibility::Collapsed);
                    m_timelinePlayhead.Visibility(xaml::Visibility::Collapsed);
                    m_timelineStartHandle.Visibility(xaml::Visibility::Collapsed);
                    m_timelineEndHandle.Visibility(xaml::Visibility::Collapsed);
                };

            if (control == nullptr || trackWidth <= TimelineHandleWidth || height <= 0.0)
            {
                hideMarks();
                return;
            }

            auto const trackTop = (height - TimelineTrackHeight) * 0.5;

            Place(m_timelineTrack, TimelineInset, trackTop, trackWidth, TimelineTrackHeight);

            auto const duration = KnownVideoDuration(*control);

            if (duration <= 0.0)
            {
                hideMarks();
                return;
            }

            auto const xAt = [&](double seconds)
                {
                    return TimelineInset + (trackWidth * std::clamp(seconds / duration, 0.0, 1.0));
                };

            auto range = glass::VideoPlayRange(control->Image, duration);

            if (m_timelineDragging)
            {
                (m_timelineDraggingEnd ? range.EndSeconds : range.StartSeconds) = m_timelineDragSeconds;
            }

            auto const startX = xAt(range.StartSeconds);
            auto const endX = xAt(range.EndSeconds);
            auto const handleHeight = height - (2.0 * TimelineHandleInset);

            m_timelineRange.Visibility(xaml::Visibility::Visible);
            m_timelineStartHandle.Visibility(xaml::Visibility::Visible);
            m_timelineEndHandle.Visibility(xaml::Visibility::Visible);

            Place(m_timelineRange, startX, trackTop, endX - startX, TimelineTrackHeight);

            // Brackets rather than two bars on the same spot: the start stands to the left of its
            // time and the stop to the right of its own, so a few seconds out of a long clip still
            // shows two ends a finger can tell apart.
            Place(m_timelineStartHandle, startX - TimelineHandleWidth, TimelineHandleInset, TimelineHandleWidth, handleHeight);
            Place(m_timelineEndHandle, endX, TimelineHandleInset, TimelineHandleWidth, handleHeight);

            // Where the video on the canvas is now, whether the designer's play button or Try
            // mode is playing it.
            size_t item{ 0 };
            double position{ 0.0 };
            double known{ 0.0 };
            bool playing{ false };

            auto const located = TryFindCanvasItem(control->Id, item) &&
                m_renderer.TryGetVideoPosition(item, position, known, playing);

            m_timelinePlayhead.Visibility(located ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (located)
            {
                Place(m_timelinePlayhead, xAt(position) - (TimelinePlayheadWidth * 0.5), 0.0, TimelinePlayheadWidth, height);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the part of the video that plays.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureTimelineSizeChanged(
        foundation::IInspectable const& sender,
        xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        DrawPictureTimeline();
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureTimelinePressed(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr || m_timelineDragging)
            {
                return;
            }

            auto const duration = KnownVideoDuration(*control);
            auto const trackWidth = PictureTimeline().ActualWidth() - (2.0 * TimelineInset);

            if (duration <= 0.0 || trackWidth <= 0.0)
            {
                return;
            }

            auto const x = static_cast<double>(args.GetCurrentPoint(PictureTimeline()).Position().X);
            auto const range = glass::VideoPlayRange(control->Image, duration);

            // Measured to the middle of each bracket, which stands beside its time rather than
            // on it, so two ends at nearly the same time are told apart by which side was pressed.
            auto const startX = TimelineInset + (trackWidth * (range.StartSeconds / duration)) - (TimelineHandleWidth * 0.5);
            auto const endX = TimelineInset + (trackWidth * (range.EndSeconds / duration)) + (TimelineHandleWidth * 0.5);

            auto const toStart = std::abs(x - startX);
            auto const toEnd = std::abs(x - endX);

            m_timelineDraggingEnd = toEnd < toStart;
            m_timelineDragSeconds = m_timelineDraggingEnd ? range.EndSeconds : range.StartSeconds;

            if (!PictureTimeline().CapturePointer(args.Pointer()))
            {
                return;
            }

            m_timelineDragging = true;
            args.Handled(true);

            // The play button gives way. The canvas holds the frame under the end being moved.
            m_previewingVideoId.clear();

            DragPictureTimelineTo(x);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to start moving an end of the part that plays.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureTimelineMoved(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_timelineDragging)
        {
            return;
        }

        try
        {
            args.Handled(true);

            DragPictureTimelineTo(static_cast<double>(args.GetCurrentPoint(PictureTimeline()).Position().X));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to move an end of the part that plays.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureTimelineReleased(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_timelineDragging)
        {
            return;
        }

        args.Handled(true);

        // Releasing the capture raises the capture lost event, which commits. Committing twice
        // is harmless: the second finds nothing being dragged.
        PictureTimeline().ReleasePointerCapture(args.Pointer());
        CommitPictureTimelineDrag();
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureTimelineCaptureLost(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CommitPictureTimelineDrag();
    }

    _Use_decl_annotations_
    void EditorWindow::DragPictureTimelineTo(double x)
    {
        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto const duration = KnownVideoDuration(*control);
        auto const trackWidth = PictureTimeline().ActualWidth() - (2.0 * TimelineInset);

        if (duration <= 0.0 || trackWidth <= 0.0)
        {
            return;
        }

        auto const range = glass::VideoPlayRange(control->Image, duration);
        auto seconds = std::clamp((x - TimelineInset) / trackWidth, 0.0, 1.0) * duration;

        // An end cannot pass the other one, and the part between them stays long enough to play.
        if (m_timelineDraggingEnd)
        {
            seconds = std::clamp(seconds, std::min(range.StartSeconds + glass::MinimumVideoPlaySeconds, duration), duration);
        }
        else
        {
            seconds = std::clamp(seconds, 0.0, std::max(range.EndSeconds - glass::MinimumVideoPlaySeconds, 0.0));
        }

        m_timelineDragSeconds = seconds;

        // The frame at that end, on the canvas, so the part can be picked by eye.
        size_t item{ 0 };

        if (TryFindCanvasItem(control->Id, item))
        {
            m_renderer.ShowVideoFrame(item, seconds);
        }

        DrawPictureTimeline();

        PictureVideoRangeCaption().Text(resources::FormatString(
            L"PictureVideoRangeFormat",
            glass::FormatVideoTime(m_timelineDraggingEnd ? range.StartSeconds : seconds),
            glass::FormatVideoTime(m_timelineDraggingEnd ? seconds : range.EndSeconds),
            glass::FormatVideoTime(duration)));
    }

    void EditorWindow::CommitPictureTimelineDrag()
    {
        if (!m_timelineDragging)
        {
            return;
        }

        m_timelineDragging = false;

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                return;
            }

            auto const id = control->Id;
            auto const duration = KnownVideoDuration(*control);
            auto const seconds = ToHundredths(m_timelineDragSeconds);

            auto picture = control->Image;

            if (m_timelineDraggingEnd)
            {
                picture.VideoEndSeconds = duration > 0.0 && seconds >= duration - SnapSeconds ? 0.0 : seconds;
            }
            else
            {
                picture.VideoStartSeconds = seconds <= SnapSeconds ? 0.0 : seconds;
            }

            ApplyControlEdit(id, [&](std::wstring const& controlId)
                { return m_editor.SetControlPicture(controlId, picture); });

            // Back to the first frame of the part that plays, which is what the designer shows.
            // After an edit the page was rebuilt and this lands on the new player; after a press
            // that moved nothing it puts back the frame the press showed.
            if (auto const* const edited = m_editor.Document().FindControl(id))
            {
                size_t item{ 0 };

                if (TryFindCanvasItem(id, item))
                {
                    m_renderer.ShowVideoFrame(
                        item, glass::VideoPlayRange(edited->Image, KnownVideoDuration(*edited)).StartSeconds);
                }
            }

            RefreshPictureVideoPanel();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set the part of the video that plays.")
    }

    // ---------------------------------------------------------------- the line that moves

    void EditorWindow::StartVideoPlayheadTimer()
    {
        try
        {
            if (m_videoPlayheadTimer != nullptr)
            {
                return;
            }

            m_videoPlayheadTimer = xaml::DispatcherTimer{};
            m_videoPlayheadTimer.Interval(PlayheadInterval);

            m_videoPlayheadTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    try
                    {
                        auto const* const control = strong->SingleSelectedControl();

                        if (control == nullptr || !ShowsAVideo(*control) ||
                            strong->PictureVideoPanel().Visibility() != xaml::Visibility::Visible)
                        {
                            strong->StopVideoPlayheadTimer();
                            return;
                        }

                        if (!strong->m_timelineDragging)
                        {
                            strong->DrawPictureTimeline();
                        }

                        // The play button follows a video that stopped by itself at the end of
                        // the part that plays, or was clicked on the page in Try mode.
                        size_t item{ 0 };

                        if (strong->TryFindCanvasItem(control->Id, item))
                        {
                            auto const playing = strong->m_renderer.IsVideoPlaying(item);
                            auto const showsStop = strong->PictureVideoPlayGlyph().Glyph() == StopGlyph;

                            if (playing != showsStop)
                            {
                                if (!playing && strong->m_previewingVideoId == control->Id)
                                {
                                    strong->m_previewingVideoId.clear();
                                }

                                strong->RefreshPictureVideoPanel();
                            }
                        }
                    }
                    catch (...)
                    {
                    }
                });

            m_videoPlayheadTimer.Start();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to follow the video as it plays.")
    }

    void EditorWindow::StopVideoPlayheadTimer()
    {
        if (m_videoPlayheadTimer == nullptr)
        {
            return;
        }

        try
        {
            m_videoPlayheadTimer.Stop();
        }
        catch (...)
        {
        }

        m_videoPlayheadTimer = nullptr;
    }
}
