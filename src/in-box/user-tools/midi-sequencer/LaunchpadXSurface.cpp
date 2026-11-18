// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXSurface.h"

#include <algorithm>
#include <unordered_map>

#include "RemoteControl.h"
#include "SequenceGenerator.h"

namespace midisequencer::launchpad
{
    namespace
    {
        inline constexpr uint8_t PaletteOff = 0;
        inline constexpr uint8_t PaletteRed = 5;
        inline constexpr uint8_t PaletteAmber = 9;
        inline constexpr uint8_t PaletteGreen = 21;

        [[nodiscard]] NoteLayoutConfig NormalizeNoteConfig(_In_ const NoteLayoutConfig& config) noexcept
        {
            NoteLayoutConfig normalized = config;
            normalized.rootNote = static_cast<uint8_t>(config.rootNote % 12u);
            normalized.octave = std::clamp(config.octave, LaunchpadXSurface::MinOctave, LaunchpadXSurface::MaxOctave);
            if (normalized.rowStepSemitones == 0)
            {
                normalized.rowStepSemitones = 5;
            }
            return normalized;
        }

        [[nodiscard]] LedSpec MakeRgbLed(_In_ uint8_t index, _In_ uint8_t r, _In_ uint8_t g, _In_ uint8_t b) noexcept
        {
            return LedSpec{ index, LedBehavior::Rgb, 0, 0, RgbColor{ r, g, b } };
        }

        [[nodiscard]] LedSpec MakeRgbLedFrom24(_In_ uint8_t index, _In_ uint32_t rgb24) noexcept
        {
            return LedSpec{ index, LedBehavior::Rgb, 0, 0, RgbColor::FromRgb24(rgb24) };
        }

        [[nodiscard]] LedSpec MakeFlashingLed(_In_ uint8_t index, _In_ uint8_t colorA, _In_ uint8_t colorB = PaletteOff) noexcept
        {
            return LedSpec{ index, LedBehavior::FlashingPalette, colorA, colorB, RgbColor{} };
        }

        [[nodiscard]] LedSpec MakePulsingLed(_In_ uint8_t index, _In_ uint8_t colorA) noexcept
        {
            return LedSpec{ index, LedBehavior::PulsingPalette, colorA, 0, RgbColor{} };
        }

        [[nodiscard]] size_t ScaleDegreeRowStep(_In_ midiapp::ScaleKind scale, _In_ size_t scaleSize, _In_ uint8_t rowStepSemitones) noexcept
        {
            if (scale == midiapp::ScaleKind::Chromatic)
            {
                return rowStepSemitones > 0 ? rowStepSemitones : 5;
            }

            if (scaleSize >= 7)
            {
                return 3;
            }

            if (scaleSize >= 4)
            {
                return 2;
            }

            return 1;
        }
    }

    _Use_decl_annotations_
    void LaunchpadXSurface::SetMode(ControllerMode mode, std::vector<SurfaceAction>& outReleaseActions)
    {
        outReleaseActions.clear();

        if (mode == m_mode)
        {
            return;
        }

        ReleaseAllHeldNotes(outReleaseActions);
        m_mode = mode;
        m_forceFullLedRefresh = true;
    }

    _Use_decl_annotations_
    void LaunchpadXSurface::SetViewport(uint32_t trackOffset, uint32_t sceneOffset) noexcept
    {
        m_trackOffset = trackOffset;
        m_sceneOffset = sceneOffset;
    }

    _Use_decl_annotations_
    void LaunchpadXSurface::SetNoteConfig(const NoteLayoutConfig& config, std::vector<SurfaceAction>& outReleaseActions)
    {
        outReleaseActions.clear();

        auto const normalized = NormalizeNoteConfig(config);
        if (normalized == m_noteConfig)
        {
            return;
        }

        ReleaseAllHeldNotes(outReleaseActions);
        m_noteConfig = normalized;
    }

    _Use_decl_annotations_
    std::optional<uint8_t> LaunchpadXSurface::MapPadToMidiNote(
        uint8_t rowBottomUp0To7,
        uint8_t colLeftRight0To7,
        bool& outIsRoot,
        bool& outInScale) const
    {
        outIsRoot = false;
        outInScale = false;

        if (rowBottomUp0To7 >= GridRows || colLeftRight0To7 >= GridCols)
        {
            return std::nullopt;
        }

        auto const normalized = NormalizeNoteConfig(m_noteConfig);
        auto const rootPc = normalized.rootNote;
        int const baseNote = (normalized.octave + 2) * 12 + static_cast<int>(rootPc);

        if (!normalized.inScaleOnly)
        {
            int const note = baseNote +
                static_cast<int>(rowBottomUp0To7) * static_cast<int>(normalized.rowStepSemitones) +
                static_cast<int>(colLeftRight0To7);

            if (note < 0 || note > 127)
            {
                return std::nullopt;
            }

            auto const pitchClass = static_cast<uint8_t>(note % 12);
            outIsRoot = (pitchClass == rootPc);

            auto const scalePitchClasses = midiapp::ScalePitchClasses({ rootPc, normalized.scale });
            outInScale = std::find(scalePitchClasses.begin(), scalePitchClasses.end(), pitchClass) != scalePitchClasses.end();
            return static_cast<uint8_t>(note);
        }

        if (baseNote < 0 || baseNote > 127)
        {
            return std::nullopt;
        }

        auto const scalePitchClasses = midiapp::ScalePitchClasses({ rootPc, normalized.scale });
        auto const rowStepDegrees = ScaleDegreeRowStep(normalized.scale, scalePitchClasses.size(), normalized.rowStepSemitones);
        auto const allScaleNotes = ScaleNotes(rootPc, normalized.scale, 0, 127);

        auto const baseIt = std::find(allScaleNotes.begin(), allScaleNotes.end(), static_cast<uint8_t>(baseNote));
        if (baseIt == allScaleNotes.end())
        {
            return std::nullopt;
        }

        auto const baseIdx = static_cast<size_t>(std::distance(allScaleNotes.begin(), baseIt));
        auto const targetIdx = baseIdx +
            static_cast<size_t>(rowBottomUp0To7) * rowStepDegrees +
            static_cast<size_t>(colLeftRight0To7);

        if (targetIdx >= allScaleNotes.size())
        {
            return std::nullopt;
        }

        auto const note = allScaleNotes[targetIdx];
        outInScale = true;
        outIsRoot = (static_cast<uint8_t>(note % 12) == rootPc);
        return note;
    }

    _Use_decl_annotations_
    void LaunchpadXSurface::ReleasePadNote(uint8_t padSlotIndex, std::vector<SurfaceAction>& outActions)
    {
        if (padSlotIndex >= PadCount || !m_heldPadNotes[padSlotIndex].has_value())
        {
            return;
        }

        auto const note = *m_heldPadNotes[padSlotIndex];
        m_heldPadNotes[padSlotIndex] = std::nullopt;

        if (note < 128 && m_activeNoteRefCounts[note] > 0)
        {
            --m_activeNoteRefCounts[note];
            if (m_activeNoteRefCounts[note] == 0)
            {
                SurfaceAction release{};
                release.kind = SurfaceActionKind::EmitNoteOff;
                release.targetMode = ControllerMode::Note;
                release.midiNote = note;
                release.velocity = 0;
                outActions.push_back(release);
            }
        }
    }

    _Use_decl_annotations_
    void LaunchpadXSurface::ReleaseAllHeldNotes(std::vector<SurfaceAction>& outActions)
    {
        for (uint8_t i = 0; i < PadCount; ++i)
        {
            ReleasePadNote(i, outActions);
        }
    }

    _Use_decl_annotations_
    void LaunchpadXSurface::HandleInput(
        const InputEvent& event,
        const SurfaceStateSnapshot& snapshot,
        std::vector<SurfaceAction>& outActions)
    {
        outActions.clear();

        if (event.kind == InputEventKind::ButtonRelease || event.kind == InputEventKind::DeviceInquiryReply)
        {
            return;
        }

        if (event.kind == InputEventKind::ButtonPress)
        {
            switch (event.index)
            {
            case ButtonSession:
                if (m_mode != ControllerMode::Clip)
                {
                    SetMode(ControllerMode::Clip, outActions);
                    SurfaceAction sw{};
                    sw.kind = SurfaceActionKind::SwitchMode;
                    sw.targetMode = ControllerMode::Clip;
                    outActions.push_back(sw);
                }
                return;

            case ButtonNote:
                if (m_mode != ControllerMode::Note)
                {
                    SetMode(ControllerMode::Note, outActions);
                    SurfaceAction sw{};
                    sw.kind = SurfaceActionKind::SwitchMode;
                    sw.targetMode = ControllerMode::Note;
                    outActions.push_back(sw);
                }
                return;

            case ButtonCustom:
            {
                SurfaceAction stop{};
                stop.kind = SurfaceActionKind::StopAll;
                outActions.push_back(stop);
                return;
            }

            case ButtonRecord:
            {
                SurfaceAction rec{};
                rec.kind = SurfaceActionKind::ToggleRecord;
                outActions.push_back(rec);
                return;
            }

            case ButtonUp:
            case ButtonDown:
            case ButtonLeft:
            case ButtonRight:
                if (m_mode == ControllerMode::Clip)
                {
                    uint32_t const maxTrackOffset = snapshot.totalTracks > GridCols ? snapshot.totalTracks - GridCols : 0u;
                    uint32_t const maxSceneOffset = snapshot.totalScenes > GridRows ? snapshot.totalScenes - GridRows : 0u;

                    auto newTrack = std::min(m_trackOffset, maxTrackOffset);
                    auto newScene = std::min(m_sceneOffset, maxSceneOffset);

                    if (event.index == ButtonUp && newScene > 0)
                    {
                        --newScene;
                    }
                    else if (event.index == ButtonDown && newScene < maxSceneOffset)
                    {
                        ++newScene;
                    }
                    else if (event.index == ButtonLeft && newTrack > 0)
                    {
                        --newTrack;
                    }
                    else if (event.index == ButtonRight && newTrack < maxTrackOffset)
                    {
                        ++newTrack;
                    }

                    if (newTrack != m_trackOffset || newScene != m_sceneOffset)
                    {
                        m_trackOffset = newTrack;
                        m_sceneOffset = newScene;

                        SurfaceAction vp{};
                        vp.kind = SurfaceActionKind::SetViewport;
                        vp.trackIndex = m_trackOffset;
                        vp.sceneIndex = m_sceneOffset;
                        outActions.push_back(vp);
                    }
                }
                else
                {
                    if (event.index == ButtonUp || event.index == ButtonDown)
                    {
                        int const currentOctave = std::clamp(m_noteConfig.octave, MinOctave, MaxOctave);
                        int const nextOctave = std::clamp(
                            currentOctave + (event.index == ButtonUp ? 1 : -1),
                            MinOctave,
                            MaxOctave);

                        if (nextOctave != currentOctave)
                        {
                            ReleaseAllHeldNotes(outActions);
                            m_noteConfig.octave = nextOctave;

                            SurfaceAction shift{};
                            shift.kind = SurfaceActionKind::ShiftOctave;
                            shift.targetMode = ControllerMode::Note;
                            shift.octaveDelta = nextOctave - currentOctave;
                            outActions.push_back(shift);
                        }
                    }
                    else if (event.index == ButtonLeft)
                    {
                        if (snapshot.totalTracks > 0 && snapshot.selectedTrackIndex > 0)
                        {
                            SurfaceAction sel{};
                            sel.kind = SurfaceActionKind::SelectTrack;
                            sel.trackIndex = std::min(snapshot.selectedTrackIndex - 1, snapshot.totalTracks - 1);
                            outActions.push_back(sel);
                        }
                    }
                    else if (event.index == ButtonRight)
                    {
                        if (snapshot.selectedTrackIndex + 1 < snapshot.totalTracks)
                        {
                            SurfaceAction sel{};
                            sel.kind = SurfaceActionKind::SelectTrack;
                            sel.trackIndex = snapshot.selectedTrackIndex + 1;
                            outActions.push_back(sel);
                        }
                    }
                }
                return;

            default:
                if (m_mode == ControllerMode::Clip && IsSceneButtonIndex(event.index))
                {
                    uint32_t const topDownRow = GridRows - static_cast<uint32_t>(event.index / 10);
                    uint32_t const sceneIndex = m_sceneOffset + topDownRow;
                    if (sceneIndex < snapshot.totalScenes)
                    {
                        SurfaceAction launch{};
                        launch.kind = SurfaceActionKind::LaunchScene;
                        launch.sceneIndex = sceneIndex;
                        outActions.push_back(launch);
                    }
                }
                return;
            }
        }

        uint8_t rowBottomUp = 0;
        uint8_t colLeftRight = 0;
        if (!PadRowColFromIndex(event.index, rowBottomUp, colLeftRight))
        {
            return;
        }

        if (m_mode == ControllerMode::Clip)
        {
            if (event.kind != InputEventKind::PadPress)
            {
                return;
            }

            uint32_t const topDownRow = (GridRows - 1u) - rowBottomUp;
            uint32_t const trackIndex = m_trackOffset + colLeftRight;
            uint32_t const sceneIndex = m_sceneOffset + topDownRow;

            if (trackIndex < snapshot.totalTracks && sceneIndex < snapshot.totalScenes)
            {
                SurfaceAction trigger{};
                trigger.kind = SurfaceActionKind::TriggerOrStopSlot;
                trigger.trackIndex = trackIndex;
                trigger.sceneIndex = sceneIndex;
                outActions.push_back(trigger);
            }
            return;
        }

        auto const padSlot = static_cast<uint8_t>(rowBottomUp * GridCols + colLeftRight);

        if (event.kind == InputEventKind::PadPress)
        {
            ReleasePadNote(padSlot, outActions);

            bool isRoot = false;
            bool inScale = false;
            auto const mappedNote = MapPadToMidiNote(rowBottomUp, colLeftRight, isRoot, inScale);
            if (!mappedNote.has_value() || *mappedNote >= 128)
            {
                return;
            }

            auto const note = *mappedNote;
            m_heldPadNotes[padSlot] = note;
            ++m_activeNoteRefCounts[note];

            SurfaceAction noteOn{};
            noteOn.kind = SurfaceActionKind::EmitNoteOn;
            noteOn.trackIndex = snapshot.selectedTrackIndex;
            noteOn.sceneIndex = snapshot.selectedSceneIndex;
            noteOn.targetMode = ControllerMode::Note;
            noteOn.midiNote = note;
            noteOn.velocity = event.value > 0 ? event.value : 1;
            outActions.push_back(noteOn);
        }
        else if (event.kind == InputEventKind::PadRelease)
        {
            ReleasePadNote(padSlot, outActions);
        }
        else if (event.kind == InputEventKind::PadAftertouch)
        {
            if (m_heldPadNotes[padSlot].has_value())
            {
                SurfaceAction at{};
                at.kind = SurfaceActionKind::EmitPolyAftertouch;
                at.trackIndex = snapshot.selectedTrackIndex;
                at.sceneIndex = snapshot.selectedSceneIndex;
                at.targetMode = ControllerMode::Note;
                at.midiNote = *m_heldPadNotes[padSlot];
                at.velocity = event.value;
                outActions.push_back(at);
            }
        }
    }

    _Use_decl_annotations_
    std::array<LedSpec, TotalLedCount> LaunchpadXSurface::ComputeDesiredLeds(const SurfaceStateSnapshot& snapshot) const
    {
        std::array<LedSpec, TotalLedCount> leds{};
        size_t cursor = 0;

        // 1. 64 grid pads (11..88, rowBottomUp 0..7, col 0..7)
        for (uint8_t row = 0; row < GridRows; ++row)
        {
            for (uint8_t col = 0; col < GridCols; ++col)
            {
                auto const padIndex = GridPadIndex(row, col);

                if (m_mode == ControllerMode::Clip)
                {
                    uint32_t const topDownRow = (GridRows - 1u) - row;
                    uint32_t const trackIndex = m_trackOffset + col;
                    uint32_t const sceneIndex = m_sceneOffset + topDownRow;

                    if (trackIndex >= snapshot.totalTracks || sceneIndex >= snapshot.totalScenes)
                    {
                        leds[cursor++] = MakeRgbLed(padIndex, 0, 0, 0);
                        continue;
                    }

                    size_t const gridIdx = static_cast<size_t>(sceneIndex) * snapshot.totalTracks + trackIndex;
                    ClipPadSnapshot const cell = gridIdx < snapshot.gridClips.size() ? snapshot.gridClips[gridIdx] : ClipPadSnapshot{};

                    if (cell.recording)
                    {
                        leds[cursor++] = MakeFlashingLed(padIndex, PaletteRed);
                    }
                    else if (cell.pending)
                    {
                        leds[cursor++] = MakeFlashingLed(padIndex, PaletteAmber);
                    }
                    else if (cell.playing)
                    {
                        leds[cursor++] = MakePulsingLed(padIndex, PaletteGreen);
                    }
                    else if (cell.hasClip)
                    {
                        uint32_t const rgb24 = cell.trackColorRgb24 != 0 ? cell.trackColorRgb24 : snapshot.activeTrackColorRgb24;
                        leds[cursor++] = MakeRgbLedFrom24(padIndex, rgb24);
                    }
                    else
                    {
                        leds[cursor++] = MakeRgbLed(padIndex, 4, 4, 4);
                    }
                }
                else
                {
                    auto const padSlot = static_cast<uint8_t>(row * GridCols + col);
                    bool isRoot = false;
                    bool inScale = false;
                    auto const mappedNote = MapPadToMidiNote(row, col, isRoot, inScale);

                    if (!mappedNote.has_value())
                    {
                        leds[cursor++] = MakeRgbLed(padIndex, 0, 0, 0);
                    }
                    else if (m_heldPadNotes[padSlot].has_value())
                    {
                        leds[cursor++] = MakeRgbLed(padIndex, 127, 127, 127);
                    }
                    else if (isRoot)
                    {
                        leds[cursor++] = MakeRgbLed(padIndex, 127, 88, 16);
                    }
                    else if (inScale)
                    {
                        uint32_t const trackRgb = snapshot.activeTrackColorRgb24 != 0 ? snapshot.activeTrackColorRgb24 : 0x4080FFu;
                        auto scaled = RgbColor::FromRgb24(trackRgb);
                        if (scaled.r == 0 && scaled.g == 0 && scaled.b == 0)
                        {
                            scaled = { 24, 64, 127 };
                        }
                        leds[cursor++] = LedSpec{ padIndex, LedBehavior::Rgb, 0, 0, scaled };
                    }
                    else
                    {
                        leds[cursor++] = MakeRgbLed(padIndex, 4, 4, 6);
                    }
                }
            }
        }

        // 2. 8 right-column scene buttons (89..19, top-down 0..7)
        for (uint8_t topDownRow = 0; topDownRow < GridRows; ++topDownRow)
        {
            auto const btnIndex = SceneButtonIndex(topDownRow);
            if (m_mode == ControllerMode::Clip)
            {
                uint32_t const sceneIndex = m_sceneOffset + topDownRow;
                if (sceneIndex < snapshot.totalScenes)
                {
                    if (sceneIndex == snapshot.selectedSceneIndex)
                    {
                        leds[cursor++] = MakeRgbLed(btnIndex, 96, 127, 96);
                    }
                    else
                    {
                        leds[cursor++] = MakeRgbLed(btnIndex, 24, 24, 28);
                    }
                }
                else
                {
                    leds[cursor++] = MakeRgbLed(btnIndex, 0, 0, 0);
                }
            }
            else
            {
                leds[cursor++] = MakeRgbLed(btnIndex, 0, 0, 0);
            }
        }

        // 3. 8 top-row buttons (91..98)
        if (m_mode == ControllerMode::Clip)
        {
            bool const canUp = m_sceneOffset > 0;
            bool const canDown = (m_sceneOffset + GridRows) < snapshot.totalScenes;
            bool const canLeft = m_trackOffset > 0;
            bool const canRight = (m_trackOffset + GridCols) < snapshot.totalTracks;

            leds[cursor++] = canUp ? MakeRgbLed(ButtonUp, 64, 64, 64) : MakeRgbLed(ButtonUp, 0, 0, 0);
            leds[cursor++] = canDown ? MakeRgbLed(ButtonDown, 64, 64, 64) : MakeRgbLed(ButtonDown, 0, 0, 0);
            leds[cursor++] = canLeft ? MakeRgbLed(ButtonLeft, 64, 64, 64) : MakeRgbLed(ButtonLeft, 0, 0, 0);
            leds[cursor++] = canRight ? MakeRgbLed(ButtonRight, 64, 64, 64) : MakeRgbLed(ButtonRight, 0, 0, 0);
        }
        else
        {
            bool const canOctUp = m_noteConfig.octave < MaxOctave;
            bool const canOctDown = m_noteConfig.octave > MinOctave;
            bool const canTrackLeft = snapshot.totalTracks > 0 && snapshot.selectedTrackIndex > 0;
            bool const canTrackRight = snapshot.selectedTrackIndex + 1 < snapshot.totalTracks;

            leds[cursor++] = canOctUp ? MakeRgbLed(ButtonUp, 64, 64, 64) : MakeRgbLed(ButtonUp, 0, 0, 0);
            leds[cursor++] = canOctDown ? MakeRgbLed(ButtonDown, 64, 64, 64) : MakeRgbLed(ButtonDown, 0, 0, 0);
            leds[cursor++] = canTrackLeft ? MakeRgbLed(ButtonLeft, 32, 48, 64) : MakeRgbLed(ButtonLeft, 0, 0, 0);
            leds[cursor++] = canTrackRight ? MakeRgbLed(ButtonRight, 32, 48, 64) : MakeRgbLed(ButtonRight, 0, 0, 0);
        }

        leds[cursor++] = (m_mode == ControllerMode::Clip)
            ? MakeRgbLed(ButtonSession, 20, 127, 40)
            : MakeRgbLed(ButtonSession, 8, 24, 12);
        leds[cursor++] = (m_mode == ControllerMode::Note)
            ? MakeRgbLed(ButtonNote, 40, 80, 127)
            : MakeRgbLed(ButtonNote, 8, 16, 28);
        leds[cursor++] = snapshot.transportPlaying
            ? MakeRgbLed(ButtonCustom, 96, 48, 0)
            : MakeRgbLed(ButtonCustom, 16, 8, 0);
        leds[cursor++] = snapshot.transportRecording
            ? MakeFlashingLed(ButtonRecord, PaletteRed)
            : (snapshot.selectedTrackArmed ? MakeRgbLed(ButtonRecord, 64, 8, 8) : MakeRgbLed(ButtonRecord, 20, 4, 4));

        // 4. Logo LED (99)
        if (snapshot.transportRecording)
        {
            leds[cursor++] = MakeRgbLed(LogoLed, 127, 0, 0);
        }
        else if (m_mode == ControllerMode::Clip)
        {
            leds[cursor++] = MakeRgbLed(LogoLed, 0, 127, 40);
        }
        else
        {
            uint32_t const logoRgb = snapshot.activeTrackColorRgb24 != 0 ? snapshot.activeTrackColorRgb24 : 0x4080FFu;
            leds[cursor++] = MakeRgbLedFrom24(LogoLed, logoRgb);
        }

        return leds;
    }

    _Use_decl_annotations_
    std::vector<LedSpec> LaunchpadXSurface::BuildLedFeedback(
        const SurfaceStateSnapshot& snapshot,
        bool forceFullRefresh)
    {
        auto const desired = ComputeDesiredLeds(snapshot);
        bool const emitAll = forceFullRefresh || !m_hasCachedFrame || m_forceFullLedRefresh;

        std::vector<LedSpec> diff{};
        diff.reserve(emitAll ? TotalLedCount : 16);

        for (size_t i = 0; i < TotalLedCount; ++i)
        {
            if (emitAll || desired[i] != m_cachedLeds[i])
            {
                diff.push_back(desired[i]);
            }
        }

        m_cachedLeds = desired;
        m_hasCachedFrame = true;
        m_forceFullLedRefresh = false;

        return diff;
    }

    _Use_decl_annotations_
    SurfaceStateSnapshot BuildSurfaceSnapshot(
        const Sequence& doc,
        const std::vector<TrackLaunchView>& launchViews,
        std::wstring_view selectedTrackId,
        uint32_t selectedScene,
        bool transportPlaying,
        bool transportRecording,
        bool slotRecording,
        std::wstring_view slotRecordingTrackId,
        uint32_t slotRecordingScene,
        const NoteLayoutConfig& noteConfig,
        bool selectedTrackArmed)
    {
        SurfaceStateSnapshot snapshot{};
        auto const order = RemoteTrackOrder(doc);

        snapshot.totalTracks = static_cast<uint32_t>(order.size());
        snapshot.totalScenes = static_cast<uint32_t>(doc.Scenes.size());
        snapshot.transportPlaying = transportPlaying;
        snapshot.transportRecording = transportRecording;
        snapshot.selectedTrackArmed = selectedTrackArmed;
        snapshot.selectedSceneIndex = selectedScene;
        snapshot.noteConfig = noteConfig;

        for (size_t i = 0; i < order.size(); ++i)
        {
            if (order[i] == selectedTrackId)
            {
                snapshot.selectedTrackIndex = static_cast<uint32_t>(i);
                break;
            }
        }

        if (auto const* selectedTrack = FindTrack(doc, selectedTrackId))
        {
            uint32_t const rgb = selectedTrack->Color & 0xFFFFFFu;
            snapshot.activeTrackColorRgb24 = rgb != 0 ? rgb : 0x4080FFu;
        }
        else
        {
            snapshot.activeTrackColorRgb24 = 0x4080FFu;
        }

        std::unordered_map<std::wstring_view, TrackLaunchView const*> viewsByTrack{};
        viewsByTrack.reserve(launchViews.size());
        for (auto const& view : launchViews)
        {
            viewsByTrack.emplace(view.TrackId, &view);
        }

        snapshot.gridClips.resize(static_cast<size_t>(snapshot.totalScenes) * snapshot.totalTracks);

        std::wstring const emptyClip{};

        for (uint32_t t = 0; t < snapshot.totalTracks; ++t)
        {
            auto const& trackId = order[t];
            auto const* track = FindTrack(doc, trackId);
            auto const viewIt = viewsByTrack.find(trackId);
            auto const* view = (viewIt != viewsByTrack.end()) ? viewIt->second : nullptr;
            uint32_t const trackColor = track != nullptr ? (track->Color & 0xFFFFFFu) : 0u;

            for (uint32_t s = 0; s < snapshot.totalScenes; ++s)
            {
                std::wstring const& clipId = (track != nullptr && s < track->Slots.size()) ? track->Slots[s] : emptyClip;

                ClipPadSnapshot cell{};
                cell.hasClip = !clipId.empty();
                cell.playing = (view != nullptr) && view->Plays(clipId, s);
                cell.pending = (view != nullptr) && view->Queues(clipId, s);
                cell.recording = slotRecording && slotRecordingTrackId == trackId && slotRecordingScene == s;
                cell.trackColorRgb24 = trackColor;

                snapshot.gridClips[static_cast<size_t>(s) * snapshot.totalTracks + t] = cell;
            }
        }

        return snapshot;
    }
}
