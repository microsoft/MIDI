// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Novation Launchpad X controller surface state machine (Clip mode, Note/Scale mode,
// clip/scene/navigation/transport actions, active-note tracking, and differential 81-LED output).
// Pure C++20 with no pch.h and no XAML dependencies.

#include <sal.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ChordTheory.h"
#include "LaunchpadXProtocol.h"
#include "PlaybackEngine.h"
#include "SequenceModel.h"

namespace midisequencer::launchpad
{
    enum class ControllerMode : uint8_t
    {
        Clip,
        Note,
    };

    struct NoteLayoutConfig
    {
        uint8_t rootNote{ 0 };
        midiapp::ScaleKind scale{ midiapp::ScaleKind::Major };
        int octave{ 3 };
        bool inScaleOnly{ false };
        uint8_t rowStepSemitones{ 5 };

        bool operator==(NoteLayoutConfig const&) const = default;
    };

    struct ClipPadSnapshot
    {
        bool hasClip{ false };
        bool playing{ false };
        bool pending{ false };
        bool recording{ false };
        uint32_t trackColorRgb24{ 0 };

        bool operator==(ClipPadSnapshot const&) const = default;
    };

    struct SurfaceStateSnapshot
    {
        bool transportPlaying{ false };
        bool transportRecording{ false };
        bool selectedTrackArmed{ false };
        uint32_t totalTracks{ 0 };
        uint32_t totalScenes{ 0 };
        uint32_t selectedTrackIndex{ 0 };
        uint32_t selectedSceneIndex{ 0 };
        uint32_t activeTrackColorRgb24{ 0x4080FFu };
        NoteLayoutConfig noteConfig{};
        std::vector<ClipPadSnapshot> gridClips{};

        bool operator==(SurfaceStateSnapshot const&) const = default;
    };

    enum class SurfaceActionKind : uint8_t
    {
        TriggerOrStopSlot,
        LaunchScene,
        StopAll,
        ToggleRecord,
        SelectTrack,
        SetViewport,
        SwitchMode,
        ShiftOctave,
        EmitNoteOn,
        EmitNoteOff,
        EmitPolyAftertouch,
    };

    struct SurfaceAction
    {
        SurfaceActionKind kind{ SurfaceActionKind::TriggerOrStopSlot };
        uint32_t trackIndex{ 0 };
        uint32_t sceneIndex{ 0 };
        ControllerMode targetMode{ ControllerMode::Clip };
        int octaveDelta{ 0 };
        uint8_t midiNote{ 0 };
        uint8_t velocity{ 0 };

        bool operator==(SurfaceAction const&) const = default;
    };

    class LaunchpadXSurface
    {
    public:
        static constexpr int MinOctave = -2;
        static constexpr int MaxOctave = 8;

        void SetMode(_In_ ControllerMode mode, _Out_ std::vector<SurfaceAction>& outReleaseActions);
        [[nodiscard]] ControllerMode Mode() const noexcept { return m_mode; }

        void SetViewport(_In_ uint32_t trackOffset, _In_ uint32_t sceneOffset) noexcept;
        [[nodiscard]] uint32_t TrackOffset() const noexcept { return m_trackOffset; }
        [[nodiscard]] uint32_t SceneOffset() const noexcept { return m_sceneOffset; }

        void SetNoteConfig(_In_ const NoteLayoutConfig& config, _Out_ std::vector<SurfaceAction>& outReleaseActions);
        [[nodiscard]] const NoteLayoutConfig& NoteConfig() const noexcept { return m_noteConfig; }

        [[nodiscard]] std::optional<uint8_t> MapPadToMidiNote(
            _In_ uint8_t rowBottomUp0To7,
            _In_ uint8_t colLeftRight0To7,
            _Out_ bool& outIsRoot,
            _Out_ bool& outInScale) const;

        void HandleInput(
            _In_ const InputEvent& event,
            _In_ const SurfaceStateSnapshot& snapshot,
            _Out_ std::vector<SurfaceAction>& outActions);

        [[nodiscard]] std::vector<LedSpec> BuildLedFeedback(
            _In_ const SurfaceStateSnapshot& snapshot,
            _In_ bool forceFullRefresh);

    private:
        void ReleaseAllHeldNotes(_Inout_ std::vector<SurfaceAction>& outActions);
        void ReleasePadNote(_In_ uint8_t padSlotIndex, _Inout_ std::vector<SurfaceAction>& outActions);
        [[nodiscard]] std::array<LedSpec, TotalLedCount> ComputeDesiredLeds(_In_ const SurfaceStateSnapshot& snapshot) const;

        ControllerMode m_mode{ ControllerMode::Clip };
        uint32_t m_trackOffset{ 0 };
        uint32_t m_sceneOffset{ 0 };
        NoteLayoutConfig m_noteConfig{};

        std::array<std::optional<uint8_t>, PadCount> m_heldPadNotes{};
        std::array<uint8_t, 128> m_activeNoteRefCounts{};

        std::array<LedSpec, TotalLedCount> m_cachedLeds{};
        bool m_hasCachedFrame{ false };
        bool m_forceFullLedRefresh{ true };
    };

    [[nodiscard]] SurfaceStateSnapshot BuildSurfaceSnapshot(
        _In_ const Sequence& doc,
        _In_ const std::vector<TrackLaunchView>& launchViews,
        _In_ std::wstring_view selectedTrackId,
        _In_ uint32_t selectedScene,
        _In_ bool transportPlaying,
        _In_ bool transportRecording,
        _In_ bool slotRecording,
        _In_ std::wstring_view slotRecordingTrackId,
        _In_ uint32_t slotRecordingScene,
        _In_ const NoteLayoutConfig& noteConfig,
        _In_ bool selectedTrackArmed = false);
}
