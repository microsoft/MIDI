// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The videos on a page: when they play, which part of the file they play, and the bar along the
// bottom that scrubs through it.
//
// As far as the media player knows, every video is silent, paused and not looping. This file
// decides when it plays and holds it to the part the layout names, because the player's own loop
// always goes back to the start of the file, and a layout can ask for a few seconds out of the
// middle of one.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "StringResources.h"

#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>

#include <chrono>
#include <cmath>

namespace media = ::winrt::Microsoft::UI::Xaml::Media;
namespace controls = ::winrt::Microsoft::UI::Xaml::Controls;
namespace playback = ::winrt::Windows::Media::Playback;
namespace resources = ::midiglass::resources;

namespace glass
{
    namespace
    {
        // How often a playing video is checked against the end of the part it plays. Thirty
        // times a second puts the loop within a frame or two of the stop point.
        constexpr auto VideoTickInterval = std::chrono::milliseconds{ 33 };

        // How close to the stop point counts as there. A little early, because the next check
        // is a thirtieth of a second away and the jump back takes a moment of its own.
        constexpr double EndSlackSeconds = 0.02;

        // Further than this before the start is somewhere the video should not be while it plays.
        constexpr double StartSlackSeconds = 0.25;

        // The bar along the bottom of a video: the dark band behind it, the track and the thumb.
        // The band is tall enough for a finger even on a small control.
        constexpr double ScrubBandFraction = 0.14;
        constexpr double MinimumScrubBand = 24.0;
        constexpr double MaximumScrubBand = 36.0;
        constexpr double ScrubInset = 10.0;
        constexpr double ScrubTrackThickness = 4.0;
        constexpr double ScrubThumbSize = 12.0;

        double ToSeconds(_In_ winrt::Windows::Foundation::TimeSpan const& span) noexcept
        {
            auto const seconds = std::chrono::duration<double>(span).count();

            return std::isfinite(seconds) ? std::max(seconds, 0.0) : 0.0;
        }

        winrt::Windows::Foundation::TimeSpan FromSeconds(_In_ double seconds) noexcept
        {
            auto const safe = std::isfinite(seconds) ? std::clamp(seconds, 0.0, MaximumVideoSeconds) : 0.0;

            return std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(
                std::chrono::duration<double>(safe));
        }

        // The band the bar sits in, in the control's own units: along the bottom of what shows.
        PictureRect ScrubBand(_In_ PictureRect const& visible) noexcept
        {
            auto const height = std::min(
                std::clamp(visible.Height * ScrubBandFraction, MinimumScrubBand, MaximumScrubBand),
                visible.Height);

            return PictureRect{ visible.X, visible.Y + visible.Height - height, visible.Width, height };
        }

        // Where the file is right now, or zero when the player cannot say.
        double PositionOf(_In_ playback::MediaPlayer const& player) noexcept
        {
            try
            {
                return ToSeconds(player.PlaybackSession().Position());
            }
            catch (...)
            {
                return 0.0;
            }
        }

        void SeekTo(_In_ playback::MediaPlayer const& player, _In_ double seconds) noexcept
        {
            try
            {
                player.PlaybackSession().Position(FromSeconds(seconds));
            }
            catch (...)
            {
            }
        }

        media::SolidColorBrush White(_In_ uint8_t alpha)
        {
            return media::SolidColorBrush{ winrt::Windows::UI::Color{ alpha, 255, 255, 255 } };
        }
    }

    // ------------------------------------------------------------------ making one

    _Use_decl_annotations_
    std::shared_ptr<SurfaceRenderer::SurfaceVideo> SurfaceRenderer::CreateVideo(
        foundation::Uri const& uri,
        Picture const& picture,
        double width,
        double height,
        bool takesInput)
    {
        auto video = std::make_shared<SurfaceVideo>();

        video->Spec = picture;
        video->Width = width;
        video->Height = height;
        video->TakesInput = takesInput;
        video->Playing = m_videosLive && picture.AutoPlays;
        video->Visible = VisiblePictureRect(
            PictureCropRect(picture, width, height, 0.0, 0.0), width, height);

        playback::MediaPlayer player{};

        // Nothing plays until this file says so, and nothing loops by itself: the player's loop
        // goes back to the start of the file, and the part that plays may start somewhere else.
        player.AutoPlay(false);
        player.IsLoopingEnabled(false);

        // Silent. A layout is a control surface, and sound from a clip behind the controls
        // during a set is never what anybody wanted.
        player.IsMuted(true);
        player.Volume(0.0);

        // Everything from here to the source is a nicety. None of it is allowed to take the
        // picture down with it if a particular file or a particular Windows build disagrees.
        try
        {
            // Without this every clip on the page registers with the system media transport
            // controls, so the keyboard's play button and the volume flyout start driving a
            // piece of somebody's stage backdrop.
            player.CommandManager().IsEnabled(false);
        }
        catch (...)
        {
        }

        controls::MediaPlayerElement element{};

        element.AreTransportControlsEnabled(false);
        element.AutoPlay(false);

        // The element is put at the exact size the crop wants, so it must not do any fitting of
        // its own on top of that.
        element.Stretch(media::Stretch::Fill);
        element.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        element.VerticalAlignment(xaml::VerticalAlignment::Top);
        element.IsHitTestVisible(false);

        auto const rect = PictureCropRect(picture, width, height, 0.0, 0.0);

        element.Width(rect.Width);
        element.Height(rect.Height);
        controls::Canvas::SetLeft(element, rect.X);
        controls::Canvas::SetTop(element, rect.Y);

        element.SetMediaPlayer(player);

        video->Player = player;
        video->Element = element;

        // Every event arrives on a media thread and is handed to the UI thread, where the page
        // may since have been rebuilt. The weak pointer is how a late event finds that out.
        std::weak_ptr<SurfaceVideo> weak = video;
        auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        if (queue != nullptr)
        {
            player.MediaOpened([this, weak, queue](auto const&, auto const&)
                {
                    queue.TryEnqueue([this, weak]()
                        {
                            if (auto const strong = weak.lock())
                            {
                                OnVideoOpened(*strong);
                            }
                        });
                });

            player.MediaEnded([this, weak, queue](auto const&, auto const&)
                {
                    queue.TryEnqueue([this, weak]()
                        {
                            if (auto const strong = weak.lock())
                            {
                                OnVideoEnded(*strong);
                            }
                        });
                });

            // The natural size is not known until the file has been opened. Until then the
            // clip fills the control.
            player.PlaybackSession().NaturalVideoSizeChanged([this, weak, queue](auto const& session, auto const&)
                {
                    double naturalWidth{ 0.0 };
                    double naturalHeight{ 0.0 };

                    try
                    {
                        naturalWidth = static_cast<double>(session.NaturalVideoWidth());
                        naturalHeight = static_cast<double>(session.NaturalVideoHeight());
                    }
                    catch (...)
                    {
                        return;
                    }

                    if (naturalWidth <= 0.0 || naturalHeight <= 0.0)
                    {
                        return;
                    }

                    queue.TryEnqueue([this, weak, naturalWidth, naturalHeight]()
                        {
                            auto const strong = weak.lock();

                            if (strong == nullptr || strong->Element == nullptr)
                            {
                                return;
                            }

                            try
                            {
                                auto const fitted = PictureCropRect(
                                    strong->Spec, strong->Width, strong->Height, naturalWidth, naturalHeight);

                                strong->Element.Width(fitted.Width);
                                strong->Element.Height(fitted.Height);
                                controls::Canvas::SetLeft(strong->Element, fitted.X);
                                controls::Canvas::SetTop(strong->Element, fitted.Y);

                                strong->Visible = VisiblePictureRect(fitted, strong->Width, strong->Height);

                                LayoutScrubber(*strong);
                            }
                            catch (...)
                            {
                            }
                        });
                });
        }

        auto const source = winrt::Windows::Media::Core::MediaSource::CreateFromUri(uri);

        try
        {
            // Muting stops the sound; deselecting the track stops it being decoded at all. The
            // list is empty until the file has been opened, so the event is where the work
            // really happens and this first call is only for a source that opened early.
            playback::MediaPlaybackItem const item{ source };

            item.AudioTracksChanged([](auto const& sender, auto const&)
                {
                    try
                    {
                        sender.AudioTracks().SelectedIndex(-1);
                    }
                    catch (...)
                    {
                    }
                });

            try
            {
                item.AudioTracks().SelectedIndex(-1);
            }
            catch (...)
            {
            }

            player.Source(item);
        }
        catch (...)
        {
            player.Source(source);
        }

        return video;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::CloseVideo(SurfaceVideo& video) noexcept
    {
        try
        {
            if (video.Element != nullptr)
            {
                video.Element.SetMediaPlayer(nullptr);
            }

            if (video.Player != nullptr)
            {
                video.Player.Pause();
                video.Player.Close();
            }
        }
        catch (...)
        {
        }

        video.Player = nullptr;
        video.Element = nullptr;
        video.Owner = nullptr;
        video.Scrubber = nullptr;
        video.ScrubTrack = nullptr;
        video.ScrubFill = nullptr;
        video.ScrubThumb = nullptr;
        video.Opened = false;
        video.Playing = false;
    }

    _Use_decl_annotations_
    SurfaceRenderer::SurfaceVideo* SurfaceRenderer::VideoAt(size_t itemIndex) const noexcept
    {
        if (itemIndex >= m_videos.size() || m_videos[itemIndex] == nullptr ||
            m_videos[itemIndex]->Player == nullptr)
        {
            return nullptr;
        }

        return m_videos[itemIndex].get();
    }

    // ------------------------------------------------------------------ what it does

    _Use_decl_annotations_
    void SurfaceRenderer::ApplyVideoState(SurfaceVideo& video) noexcept
    {
        // Before the file opens there is nothing to tell. Opening tells it.
        if (!video.Opened || video.Player == nullptr)
        {
            return;
        }

        try
        {
            if (video.Playing && !video.Scrubbing)
            {
                video.Player.Play();
            }
            else
            {
                video.Player.Pause();
            }
        }
        catch (...)
        {
        }

        if (video.Playing)
        {
            StartVideoTimerIfNeeded();
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::OnVideoOpened(SurfaceVideo& video) noexcept
    {
        if (video.Player == nullptr)
        {
            return;
        }

        try
        {
            video.Opened = true;
            video.DurationSeconds = ToSeconds(video.Player.PlaybackSession().NaturalDuration());
        }
        catch (...)
        {
        }

        auto const range = VideoPlayRange(video.Spec, video.DurationSeconds);

        auto const target = video.PendingFrameSeconds >= 0.0
            ? std::min(video.PendingFrameSeconds, video.DurationSeconds)
            : range.StartSeconds;

        video.PendingFrameSeconds = -1.0;

        // Asked for even at zero: a paused player shows the frame it was last sent to, and a
        // file that has only been opened has not been sent anywhere.
        SeekTo(video.Player, target);

        ApplyVideoState(video);
        RefreshScrubber(video, target);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::OnVideoEnded(SurfaceVideo& video) noexcept
    {
        if (video.Player == nullptr || video.Scrubbing)
        {
            return;
        }

        // The file ran out before a stop point did, which is what a stop of zero means.
        auto const range = VideoPlayRange(video.Spec, video.DurationSeconds);

        if (video.Playing && video.Spec.Loops)
        {
            SeekTo(video.Player, range.StartSeconds);
            ApplyVideoState(video);
            RefreshScrubber(video, range.StartSeconds);
            return;
        }

        video.Playing = false;
        RefreshScrubber(video, range.EndSeconds);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetVideosLive(bool live) noexcept
    {
        if (m_videosLive == live)
        {
            return;
        }

        m_videosLive = live;

        // Either way it goes back to the start of the part it plays: the designer shows that
        // frame, and a run starts from it.
        auto const reset = [&](SurfaceVideo& video)
            {
                video.Scrubbing = false;
                video.Playing = live && video.Spec.AutoPlays;
                video.PendingFrameSeconds = -1.0;

                if (!video.Opened || video.Player == nullptr)
                {
                    return;
                }

                auto const range = VideoPlayRange(video.Spec, video.DurationSeconds);

                SeekTo(video.Player, range.StartSeconds);
                ApplyVideoState(video);
                RefreshScrubber(video, range.StartSeconds);
            };

        for (auto const& video : m_videos)
        {
            if (video != nullptr)
            {
                reset(*video);
            }
        }

        if (m_backgroundVideo != nullptr)
        {
            reset(*m_backgroundVideo);
        }
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::HasVideo(size_t itemIndex) const noexcept
    {
        return VideoAt(itemIndex) != nullptr;
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::VideoTakesClicks(size_t itemIndex) const noexcept
    {
        auto const* const video = VideoAt(itemIndex);

        return video != nullptr && video->TakesInput && video->Spec.ClickToPlay;
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::VideoShowsScrubber(size_t itemIndex) const noexcept
    {
        auto const* const video = VideoAt(itemIndex);

        return video != nullptr && video->TakesInput && video->Spec.ShowsScrubber;
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::IsVideoPlaying(size_t itemIndex) const noexcept
    {
        auto const* const video = VideoAt(itemIndex);

        return video != nullptr && video->Playing;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetVideoPlaying(size_t itemIndex, bool playing) noexcept
    {
        auto* const video = VideoAt(itemIndex);

        if (video == nullptr)
        {
            return;
        }

        video->Scrubbing = false;
        video->Playing = playing;
        video->PendingFrameSeconds = -1.0;

        if (!video->Opened)
        {
            return;
        }

        auto position = PositionOf(video->Player);

        if (playing)
        {
            // From the start again when it stopped at the end, or was left outside the part
            // that plays by a frame shown from the inspector.
            auto const range = VideoPlayRange(video->Spec, video->DurationSeconds);

            if (position < range.StartSeconds - EndSlackSeconds ||
                (range.EndSeconds > 0.0 && position >= range.EndSeconds - EndSlackSeconds))
            {
                SeekTo(video->Player, range.StartSeconds);
                position = range.StartSeconds;
            }
        }

        ApplyVideoState(*video);
        RefreshScrubber(*video, position);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ToggleVideo(size_t itemIndex) noexcept
    {
        SetVideoPlaying(itemIndex, !IsVideoPlaying(itemIndex));
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ShowVideoFrame(size_t itemIndex, double seconds) noexcept
    {
        auto* const video = VideoAt(itemIndex);

        if (video == nullptr)
        {
            return;
        }

        video->Scrubbing = false;
        video->Playing = false;

        auto const safe = std::isfinite(seconds) ? std::clamp(seconds, 0.0, MaximumVideoSeconds) : 0.0;

        if (!video->Opened)
        {
            video->PendingFrameSeconds = safe;
            return;
        }

        auto const target = video->DurationSeconds > 0.0 ? std::min(safe, video->DurationSeconds) : safe;

        ApplyVideoState(*video);
        SeekTo(video->Player, target);
        RefreshScrubber(*video, target);
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::TryGetVideoPosition(
        size_t itemIndex,
        double& seconds,
        double& durationSeconds,
        bool& playing) const noexcept
    {
        seconds = 0.0;
        durationSeconds = 0.0;
        playing = false;

        auto const* const video = VideoAt(itemIndex);

        if (video == nullptr || !video->Opened)
        {
            return false;
        }

        seconds = PositionOf(video->Player);
        durationSeconds = video->DurationSeconds;
        playing = video->Playing;

        return true;
    }

    // ------------------------------------------------------------------ the bar

    _Use_decl_annotations_
    bool SurfaceRenderer::TryGetScrubFraction(
        size_t itemIndex,
        double x,
        double y,
        bool anywhere,
        double& fraction) const noexcept
    {
        fraction = 0.0;

        auto const* const video = VideoAt(itemIndex);

        if (video == nullptr || !video->TakesInput || !video->Spec.ShowsScrubber ||
            video->Scrubber == nullptr)
        {
            return false;
        }

        try
        {
            if (video->Scrubber.Visibility() != xaml::Visibility::Visible)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        auto const band = ScrubBand(video->Visible);

        if (band.Width <= 0.0 || band.Height <= 0.0)
        {
            return false;
        }

        if (!anywhere &&
            (x < band.X || x > band.X + band.Width || y < band.Y || y > band.Y + band.Height))
        {
            return false;
        }

        auto const trackWidth = std::max(band.Width - (2.0 * ScrubInset), 1.0);

        fraction = std::clamp((x - band.X - ScrubInset) / trackWidth, 0.0, 1.0);

        return true;
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ScrubVideo(size_t itemIndex, double fraction) noexcept
    {
        auto* const video = VideoAt(itemIndex);

        if (video == nullptr)
        {
            return;
        }

        if (!video->Scrubbing)
        {
            video->Scrubbing = true;
            video->ResumeAfterScrub = video->Playing;

            ApplyVideoState(*video);
        }

        auto const range = VideoPlayRange(video->Spec, video->DurationSeconds);
        auto const seconds = VideoRangeSeconds(range, fraction);

        if (!video->Opened)
        {
            video->PendingFrameSeconds = seconds;
            return;
        }

        SeekTo(video->Player, seconds);
        RefreshScrubber(*video, seconds);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::EndScrub(size_t itemIndex) noexcept
    {
        auto* const video = VideoAt(itemIndex);

        if (video == nullptr || !video->Scrubbing)
        {
            return;
        }

        video->Scrubbing = false;
        video->Playing = video->ResumeAfterScrub;

        ApplyVideoState(*video);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutScrubber(SurfaceVideo& video) noexcept
    {
        // Only an image control that asked for one. A panel's fill sits behind the controls on
        // the panel, and a bar there would be under every one of them.
        if (!video.TakesInput || !video.Spec.ShowsScrubber || video.Element == nullptr)
        {
            return;
        }

        try
        {
            auto const container = video.Element.Parent().try_as<controls::Canvas>();

            if (container == nullptr)
            {
                return;
            }

            auto const band = ScrubBand(video.Visible);

            if (video.Scrubber == nullptr)
            {
                controls::Canvas bar{};

                bar.IsHitTestVisible(false);
                bar.Background(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x80, 0, 0, 0 } });

                xaml::Automation::AutomationProperties::SetAccessibilityView(
                    bar, xaml::Automation::Peers::AccessibilityView::Raw);

                xaml::Shapes::Rectangle track{};
                track.Fill(White(0x60));
                track.RadiusX(ScrubTrackThickness * 0.5);
                track.RadiusY(ScrubTrackThickness * 0.5);

                xaml::Shapes::Rectangle fill{};
                fill.Fill(White(0xF0));
                fill.RadiusX(ScrubTrackThickness * 0.5);
                fill.RadiusY(ScrubTrackThickness * 0.5);

                xaml::Shapes::Ellipse thumb{};
                thumb.Fill(White(0xFF));
                thumb.Width(ScrubThumbSize);
                thumb.Height(ScrubThumbSize);

                bar.Children().Append(track);
                bar.Children().Append(fill);
                bar.Children().Append(thumb);

                // Over the picture and over its wash, so it can always be seen.
                container.Children().Append(bar);

                video.Scrubber = bar;
                video.ScrubTrack = track;
                video.ScrubFill = fill;
                video.ScrubThumb = thumb;
            }

            // Too small to hold a bar that a finger could find.
            if (band.Width < ScrubInset * 4.0 || band.Height <= 0.0)
            {
                video.Scrubber.Visibility(xaml::Visibility::Collapsed);
                return;
            }

            video.Scrubber.Visibility(xaml::Visibility::Visible);
            video.Scrubber.Width(band.Width);
            video.Scrubber.Height(band.Height);
            controls::Canvas::SetLeft(video.Scrubber, band.X);
            controls::Canvas::SetTop(video.Scrubber, band.Y);

            auto const trackWidth = std::max(band.Width - (2.0 * ScrubInset), 1.0);
            auto const trackTop = (band.Height - ScrubTrackThickness) * 0.5;

            video.ScrubTrack.Width(trackWidth);
            video.ScrubTrack.Height(ScrubTrackThickness);
            controls::Canvas::SetLeft(video.ScrubTrack, ScrubInset);
            controls::Canvas::SetTop(video.ScrubTrack, trackTop);

            video.ScrubFill.Height(ScrubTrackThickness);
            controls::Canvas::SetLeft(video.ScrubFill, ScrubInset);
            controls::Canvas::SetTop(video.ScrubFill, trackTop);

            controls::Canvas::SetTop(video.ScrubThumb, (band.Height - ScrubThumbSize) * 0.5);
        }
        catch (...)
        {
        }

        auto const range = VideoPlayRange(video.Spec, video.DurationSeconds);

        RefreshScrubber(video, video.Opened ? PositionOf(video.Player) : range.StartSeconds);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::RefreshScrubber(SurfaceVideo& video, double seconds) noexcept
    {
        try
        {
            // What a screen reader hears about a video on an image control, and what a test
            // reads to see where it is. Only set when it changes, which is ten times a second
            // at most, because it is shown to the tenth.
            if (video.Owner != nullptr)
            {
                auto status = std::wstring{ resources::FormatString(
                    video.Playing && !video.Scrubbing ? L"VideoStatusPlayingFormat" : L"VideoStatusStoppedFormat",
                    FormatVideoTime(seconds)) };

                if (status != video.Status)
                {
                    video.Status = std::move(status);

                    xaml::Automation::AutomationProperties::SetItemStatus(
                        video.Owner, winrt::hstring{ video.Status });
                }
            }

            if (video.Scrubber == nullptr || video.ScrubFill == nullptr || video.ScrubThumb == nullptr)
            {
                return;
            }

            auto const range = VideoPlayRange(video.Spec, video.DurationSeconds);
            auto const fraction = VideoRangeFraction(range, seconds);
            auto const band = ScrubBand(video.Visible);
            auto const trackWidth = std::max(band.Width - (2.0 * ScrubInset), 1.0);

            video.ScrubFill.Width(trackWidth * fraction);
            controls::Canvas::SetLeft(
                video.ScrubThumb, ScrubInset + (trackWidth * fraction) - (ScrubThumbSize * 0.5));
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------------------ the clock

    void SurfaceRenderer::RefreshVideos() noexcept
    {
        auto anyPlaying = false;

        auto const visit = [&](SurfaceVideo& video)
            {
                if (!video.Opened || video.Player == nullptr)
                {
                    return;
                }

                auto position = PositionOf(video.Player);

                if (video.Playing && !video.Scrubbing)
                {
                    anyPlaying = true;

                    auto const range = VideoPlayRange(video.Spec, video.DurationSeconds);

                    if (range.EndSeconds > 0.0 && position >= range.EndSeconds - EndSlackSeconds)
                    {
                        if (video.Spec.Loops)
                        {
                            SeekTo(video.Player, range.StartSeconds);
                            position = range.StartSeconds;
                        }
                        else
                        {
                            // Held on the last frame of the part it plays. A click or the
                            // play button starts it from the top again.
                            video.Playing = false;
                            ApplyVideoState(video);
                            position = range.EndSeconds;
                        }
                    }
                    else if (position < range.StartSeconds - StartSlackSeconds)
                    {
                        SeekTo(video.Player, range.StartSeconds);
                        position = range.StartSeconds;
                    }
                }

                RefreshScrubber(video, position);
            };

        for (auto const& video : m_videos)
        {
            if (video != nullptr)
            {
                visit(*video);
            }
        }

        if (m_backgroundVideo != nullptr)
        {
            visit(*m_backgroundVideo);
        }

        if (!anyPlaying)
        {
            StopVideoTimer();
        }
    }

    void SurfaceRenderer::StartVideoTimerIfNeeded()
    {
        try
        {
            if (m_videoTimer != nullptr || m_host == nullptr)
            {
                return;
            }

            auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            if (queue == nullptr)
            {
                return;
            }

            m_videoTimer = queue.CreateTimer();
            m_videoTimer.Interval(VideoTickInterval);
            m_videoTimer.Tick([this](auto&&, auto&&) { RefreshVideos(); });
            m_videoTimer.Start();
        }
        catch (...)
        {
        }
    }

    void SurfaceRenderer::StopVideoTimer() noexcept
    {
        if (m_videoTimer == nullptr)
        {
            return;
        }

        try
        {
            m_videoTimer.Stop();
        }
        catch (...)
        {
        }

        m_videoTimer = nullptr;
    }
}
