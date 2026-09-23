// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSequencePlayer.h"
#include "Utilities.Sequencing.MidiSequencePlayer.g.cpp"

#include "MidiSequence.h"
#include "MidiSequenceTrackRouting.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    _Use_decl_annotations_
    MidiSequencePlayer::MidiSequencePlayer(midi2::MidiEndpointConnection const& connection, midi2::MidiGroup const& group)
    {
        InternalAttach(connection, group);
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::InternalAttach(midi2::MidiEndpointConnection const& connection, midi2::MidiGroup const& group)
    {
        m_groupIndex = group == nullptr ? uint8_t{ 0 } : group.Index();

        m_engine.AttachConnection(connection);

        InternalHookCompletion();
    }

    void MidiSequencePlayer::InternalHookCompletion()
    {
        auto weak = get_weak();

        m_engine.SetCompletionHandler([weak]() noexcept
            {
                try
                {
                    if (auto strong = weak.get())
                    {
                        // The engine raises this from its worker. Handlers are told in the
                        // documentation not to call back in synchronously.
                        strong->m_playbackEndedEvent(*strong, nullptr);
                        strong->m_stateChangedEvent(*strong, nullptr);
                    }
                }
                catch (...)
                {
                    LOG_IF_FAILED(E_FAIL);
                }
            });
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Sequencing::MidiSequencePlayer> MidiSequencePlayer::CreateForEndpointAsync(
        midi2::MidiSession session,
        winrt::hstring endpointDeviceId,
        midi2::MidiGroup group)
    {
        co_await winrt::resume_background();

        auto player = winrt::make_self<implementation::MidiSequencePlayer>();

        player->m_groupIndex = group == nullptr ? uint8_t{ 0 } : group.Index();

        auto const result = player->m_engine.Open(session, std::wstring{ endpointDeviceId });

        if (result != ::midiplayer::OpenResult::Success)
        {
            co_return nullptr;
        }

        player->InternalHookCompletion();

        co_return *player;
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MidiSequencePlayer::SetSequenceAsync(Sequencing::MidiSequence sequence)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        if (sequence == nullptr)
        {
            m_engine.Unload();
            m_sequence = nullptr;

            co_return;
        }

        auto const* const self = winrt::get_self<implementation::MidiSequence>(sequence);

        if (self == nullptr)
        {
            co_return;
        }

        // The conversion to Universal MIDI Packets happens here, once, which is why this is the
        // expensive call and Play is not.
        if (m_engine.Load(self->InternalSequence(), m_groupIndex))
        {
            m_sequence = sequence;
        }
    }

    Sequencing::MidiSequencePlayerState MidiSequencePlayer::State() const noexcept
    {
        switch (m_engine.State())
        {
        case ::midiplayer::PlaybackState::Stopped:  return Sequencing::MidiSequencePlayerState::Stopped;
        case ::midiplayer::PlaybackState::Playing:  return Sequencing::MidiSequencePlayerState::Playing;
        case ::midiplayer::PlaybackState::Paused:   return Sequencing::MidiSequencePlayerState::Paused;
        default:                                    return Sequencing::MidiSequencePlayerState::NoSequence;
        }
    }

    Sequencing::MidiSequencePlayerPosition MidiSequencePlayer::Position() const noexcept
    {
        auto const position = m_engine.Position();

        return Sequencing::MidiSequencePlayerPosition{
            State(),
            position.Microseconds,
            position.DurationMicroseconds,
            position.Tick,
            position.Bar,
            position.Beat,
            position.BeatsPerMinute };
    }

    void MidiSequencePlayer::RaiseStateChanged()
    {
        try
        {
            m_stateChangedEvent(*this, nullptr);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);
        }
    }

    void MidiSequencePlayer::Play()
    {
        m_engine.Play();
        RaiseStateChanged();
    }

    void MidiSequencePlayer::Pause()
    {
        m_engine.Pause();
        RaiseStateChanged();
    }

    void MidiSequencePlayer::Stop()
    {
        m_engine.Stop();
        RaiseStateChanged();
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::SeekToMicroseconds(uint64_t const microseconds)
    {
        m_engine.SeekToMicroseconds(microseconds);
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::SeekToTick(uint32_t const tick)
    {
        if (m_sequence == nullptr)
        {
            return;
        }

        m_engine.SeekToMicroseconds(m_sequence.ConvertTickToMicroseconds(tick));
    }

    _Use_decl_annotations_
    Sequencing::MidiSequenceTrackRouting MidiSequencePlayer::GetTrackRouting(uint16_t const trackIndex)
    {
        auto const found = m_routing.find(trackIndex);

        if (found != m_routing.end())
        {
            return found->second;
        }

        auto routing = winrt::make_self<implementation::MidiSequenceTrackRouting>();

        routing->IsMuted(m_engine.IsTrackMuted(trackIndex));

        return *routing;
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::SetTrackRouting(uint16_t const trackIndex, Sequencing::MidiSequenceTrackRouting const& routing)
    {
        if (routing == nullptr)
        {
            m_routing.erase(trackIndex);
            m_engine.ClearTrackRoute(trackIndex);
            m_engine.SetTrackMuted(trackIndex, false);

            return;
        }

        m_routing.insert_or_assign(trackIndex, routing);

        ::midiplayer::TrackRoute route{};

        route.Connection = routing.Connection();

        if (auto const group = routing.Group())
        {
            route.GroupIndex = static_cast<int32_t>(group.Index());
        }

        if (auto const channel = routing.ChannelOverride())
        {
            route.ChannelOverride = static_cast<int32_t>(channel.Index());
        }

        m_engine.SetTrackRoute(trackIndex, route);
        m_engine.SetTrackMuted(trackIndex, routing.IsMuted());
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::SetTrackMuted(uint16_t const trackIndex, bool const muted)
    {
        m_engine.SetTrackMuted(trackIndex, muted);
    }

    _Use_decl_annotations_
    bool MidiSequencePlayer::IsTrackMuted(uint16_t const trackIndex)
    {
        return m_engine.IsTrackMuted(trackIndex);
    }

    int32_t MidiSequencePlayer::SoloTrackIndex() const noexcept
    {
        return m_engine.SoloTrack();
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::SoloTrackIndex(int32_t const value)
    {
        m_engine.SetSoloTrack(value);
    }

    void MidiSequencePlayer::SilenceAllNotes()
    {
        // Stopping is what the engine offers, and it sends the same panic. Calling it while
        // already stopped costs nothing.
        m_engine.Stop();
    }

    void MidiSequencePlayer::Close()
    {
        if (m_closed)
        {
            return;
        }

        m_closed = true;

        try
        {
            m_engine.SetCompletionHandler(nullptr);
            m_engine.Stop();
            m_engine.Close();
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);
        }

        m_sequence = nullptr;
        m_routing.clear();
    }

    _Use_decl_annotations_
    winrt::event_token MidiSequencePlayer::PlaybackEnded(
        foundation::TypedEventHandler<Sequencing::MidiSequencePlayer, foundation::IInspectable> const& handler)
    {
        return m_playbackEndedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::PlaybackEnded(winrt::event_token const& token) noexcept
    {
        m_playbackEndedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiSequencePlayer::StateChanged(
        foundation::TypedEventHandler<Sequencing::MidiSequencePlayer, foundation::IInspectable> const& handler)
    {
        return m_stateChangedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiSequencePlayer::StateChanged(winrt::event_token const& token) noexcept
    {
        m_stateChangedEvent.remove(token);
    }
}
