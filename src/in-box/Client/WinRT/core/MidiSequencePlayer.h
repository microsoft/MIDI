// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiSequencePlayer.g.h"

#include "midi_sequence_playback_engine.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiSequencePlayer : MidiSequencePlayerT<MidiSequencePlayer>
    {
        MidiSequencePlayer() = default;
        MidiSequencePlayer(_In_ midi2::MidiEndpointConnection const& connection, _In_ midi2::MidiGroup const& group);

        ~MidiSequencePlayer() noexcept { Close(); }

        static foundation::IAsyncOperation<Sequencing::MidiSequencePlayer> CreateForEndpointAsync(
            _In_ midi2::MidiSession session,
            _In_ winrt::hstring endpointDeviceId,
            _In_ midi2::MidiGroup group);

        bool OwnsConnection() const noexcept { return m_engine.OwnsConnection(); }
        midi2::MidiEndpointConnection Connection() const noexcept { return m_engine.Connection(); }

        foundation::IAsyncAction SetSequenceAsync(_In_ Sequencing::MidiSequence sequence);
        Sequencing::MidiSequence Sequence() const noexcept { return m_sequence; }

        Sequencing::MidiSequencePlayerState State() const noexcept;
        Sequencing::MidiSequencePlayerPosition Position() const noexcept;

        void Play();
        void Pause();
        void Stop();

        void SeekToMicroseconds(_In_ uint64_t const microseconds);
        void SeekToTick(_In_ uint32_t const tick);

        Sequencing::MidiSequenceTrackRouting GetTrackRouting(_In_ uint16_t const trackIndex);
        void SetTrackRouting(_In_ uint16_t const trackIndex, _In_ Sequencing::MidiSequenceTrackRouting const& routing);

        void SetTrackMuted(_In_ uint16_t const trackIndex, _In_ bool const muted);
        bool IsTrackMuted(_In_ uint16_t const trackIndex);

        int32_t SoloTrackIndex() const noexcept;
        void SoloTrackIndex(_In_ int32_t const value);

        void SilenceAllNotes();

        void Close();

        winrt::event_token PlaybackEnded(_In_ foundation::TypedEventHandler<Sequencing::MidiSequencePlayer, foundation::IInspectable> const& handler);
        void PlaybackEnded(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token StateChanged(_In_ foundation::TypedEventHandler<Sequencing::MidiSequencePlayer, foundation::IInspectable> const& handler);
        void StateChanged(_In_ winrt::event_token const& token) noexcept;

        void InternalAttach(_In_ midi2::MidiEndpointConnection const& connection, _In_ midi2::MidiGroup const& group);
        void InternalHookCompletion();

    private:
        void RaiseStateChanged();

        ::midiplayer::PlaybackEngine m_engine{};

        Sequencing::MidiSequence m_sequence{ nullptr };
        uint8_t m_groupIndex{ 0 };
        bool m_closed{ false };

        std::map<uint16_t, Sequencing::MidiSequenceTrackRouting> m_routing{};

        winrt::event<foundation::TypedEventHandler<Sequencing::MidiSequencePlayer, foundation::IInspectable>> m_playbackEndedEvent{};
        winrt::event<foundation::TypedEventHandler<Sequencing::MidiSequencePlayer, foundation::IInspectable>> m_stateChangedEvent{};
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::factory_implementation
{
    struct MidiSequencePlayer : MidiSequencePlayerT<MidiSequencePlayer, implementation::MidiSequencePlayer>
    {
    };
}
