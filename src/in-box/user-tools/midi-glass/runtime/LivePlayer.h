// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "LayoutModel.h"
#include "BindingEngine.h"
#include "ActionPlan.h"
#include "LearnCapture.h"
#include "ValueThrottle.h"
#include "DeviceCatalog.h"
#include "OutputRouter.h"
#include "SequenceRunner.h"
#include "ClockGenerator.h"
#include "LfoGenerator.h"
#include "StepSequencer.h"
#include "PadVoices.h"

namespace glass
{
    // One message that reached a connection. What the editor's monitor rail draws, and the only
    // place the app can honestly say a thing was sent rather than merely built.
    struct SentMessage
    {
        uint64_t TimestampMilliseconds{ 0 };

        // The engine's index, counting every page in order. Not the item index of a page.
        uint32_t ControlIndex{ 0 };

        int32_t DestinationIndex{ -1 };

        uint32_t WordCount{ 0 };
        uint32_t Words[4]{};
    };

    // Everything between a value on the surface and words on the wire: the layout's device
    // table, the connections, the binding engine and the per-control throttles.
    //
    // The runtime window and the editor's Try mode both drive one of these. That is the whole
    // reason it exists: a fader has to send exactly the same thing while it is being built as it
    // does once the layout is running, and two copies of this code would drift apart.
    //
    // Held by shared_ptr, because it starts detached threads that block on the service and they
    // must not call into a freed object when a window closes under them.
    //
    // Not pure, and not in the unit tests: it opens connections and marshals through a dispatcher
    // queue. The arithmetic lives in BindingEngine and ValueThrottle, which are pure and are.
    class LivePlayer : public std::enable_shared_from_this<LivePlayer>
    {
    public:
        static std::shared_ptr<LivePlayer> Create();

        ~LivePlayer() noexcept;

        LivePlayer(LivePlayer const&) = delete;
        LivePlayer& operator=(LivePlayer const&) = delete;

        // ---- what the owner hears about. All raised on the dispatcher's thread. ----

        // The device table resolved, or something came or went. The owner redraws its status.
        std::function<void()> DevicesChanged{};

        // A device sent something that moves a control. The owner owns what that does on screen,
        // because the editor and the runtime window draw the same control differently.
        std::function<void(uint32_t controlIndex, double value)> FeedbackMoved{};

        // A device sent something a control is only watching for as traffic, so there is no
        // value to carry. Lit says whether it blinks, comes on, or goes dark.
        enum class ListenerState
        {
            Blink = 0,
            On = 1,
            Off = 2,
        };

        std::function<void(uint32_t controlIndex, ListenerState state)> ActivitySeen{};

        // A clock generator moved on. The owner draws the sweep and the pips.
        std::function<void(uint32_t controlIndex, int32_t beatInBar, double phase, bool running)> BeatMoved{};

        // An LFO swept. The owner moves the bead along the cycle it drew.
        std::function<void(uint32_t controlIndex, double value, double phase, bool running)> LfoMoved{};

        // A step sequencer moved on to a step, or stopped. The owner lights the step; -1 with
        // running false means nothing is playing.
        std::function<void(uint32_t controlIndex, int32_t stepIndex, bool running)> StepMoved{};

        // Every message that actually went out. Left empty by the runtime window, which pays
        // nothing for it; the editor's monitor rail sets it.
        std::function<void(SentMessage const& message)> Sent{};

        // A sequence step moved another control. The owner owns what that does on screen.
        std::function<void(uint32_t controlIndex, double value)> ControlValueSet{};

        // A control or a sequence step asked for another page.
        std::function<void(uint32_t pageIndex)> PageRequested{};

        // The tempo a clock generator is running at, for whatever draws it. Zero when it is
        // stopped.
        std::function<void(uint32_t controlIndex, double beatsPerMinute)> TempoChanged{};

        // Something arrived that a control could be bound to, while learning is armed. Raised
        // on the dispatcher's thread, and only while SetLearning(true) is in force.
        std::function<void(LearnedBinding const& learned)> Learned{};

        // ---- lifetime ----

        // ownerId separates one player's connections from another's inside the process-wide
        // router. A layout being edited and the same layout running are two owners on purpose,
        // so closing one does not take the other's connections down.
        void Start(
            _In_ LayoutDocument const& document,
            _In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
            _In_ std::wstring const& ownerId);

        // The document changed under an already-started player, which is what every edit does in
        // Try mode. Re-resolves and re-prepares without closing anything that is still wanted.
        void UpdateDocument(_In_ LayoutDocument const& document);

        void Stop();

        // The editor leaves Try mode far more often than it closes, and tearing the player down
        // each time would race a close against the next open on the same owner id. Muting is the
        // gate instead: the watcher can still reopen connections underneath, but nothing reaches
        // the wire. Without this, a MIDI device arriving while the editor sat in Edit mode sent a
        // whole layout's worth of startup values.
        void SetOutputEnabled(_In_ bool enabled) noexcept;

        // Listen for something to bind to. Off by default, because reading every message that
        // arrives on every device costs something and nothing wants it until somebody asks.
        void SetLearning(_In_ bool learning) noexcept { m_learning = learning; }
        bool IsLearning() const noexcept { return m_learning; }

        // ---- sending. Called on the UI thread, straight out of a pointer handler. ----

        void ValueChanged(_In_ uint32_t controlIndex, _In_ double value, _In_ bool isFinal);

        // The other axis of a two axis control. Throttled on its own, because a pad dragged in
        // a circle is two streams of values and limiting them together would halve both.
        void ValueYChanged(_In_ uint32_t controlIndex, _In_ double value, _In_ bool isFinal);

        // A key on a piano keyboard, counted from the leftmost drawn.
        void KeyChanged(
            _In_ uint32_t controlIndex,
            _In_ int32_t key,
            _In_ double velocity,
            _In_ bool isDown);

        // A finger on a note pad or hex pad control. Touch is the pointer, so every finger holds
        // its own note and a chord is a chord. Pitch is the pitch under the finger, which only a
        // note that bends to follow it uses.
        void PadPressed(
            _In_ uint32_t controlIndex,
            _In_ uint32_t touch,
            _In_ int32_t note,
            _In_ double velocity,
            _In_ double pitch);

        void PadMoved(
            _In_ uint32_t controlIndex,
            _In_ uint32_t touch,
            _In_ int32_t note,
            _In_ double velocity,
            _In_ double pitch);

        void PadReleased(_In_ uint32_t controlIndex, _In_ uint32_t touch);

        // Assistive technology setting a value is one discrete change, not a drag, so it is a
        // whole gesture. Going straight to the release would find nothing held back.
        void SetDirectly(_In_ uint32_t controlIndex, _In_ double value);

        void Switched(_In_ uint32_t controlIndex, _In_ bool isOn);

        // The same, for a pad that takes its velocity from how hard it was hit. Everything else
        // hits at full, which is what the plain overload passes.
        void Switched(_In_ uint32_t controlIndex, _In_ bool isOn, _In_ double velocity);

        // The touch and release triggers, which is what a system exclusive dump or a sequence
        // usually hangs off. Separate from Switched because a fader is touched too.
        void Touched(_In_ uint32_t controlIndex, _In_ bool isTouched);

        // Where the nearest stop is, or the position unchanged.
        double SnapToDetent(_In_ uint32_t controlIndex, _In_ double position) const;

        // The number a control shows inside itself: the figure that would go on the wire when
        // the customer is working in a device's own units, and a percentage when they are not.
        std::wstring DescribeValue(
            _In_ uint32_t controlIndex,
            _In_ ValueAxis axis,
            _In_ double position) const;

        // Sent once per run, in keyboard order. Does nothing until something is connected, so
        // the layout is not marked initialized before it actually was.
        void SendStartupValues();

        // One sequence, right now, without a control pressing it. The sequence is passed in
        // rather than named so the editor can test the edit on screen rather than the copy that
        // is already in the document. False when nothing was connected or it built to nothing.
        //
        // The control index is only used to label what the monitor shows, so passing the control
        // the sequence is being edited from makes the rows land under the right filter.
        bool RunSequenceNow(_In_ Sequence const& sequence, _In_ uint32_t controlIndex);

        void StopSequenceNow(_In_ uint32_t controlIndex) noexcept;

        // Clock generators. Starting one is what pressing it does; a layout can also ask for it
        // to be running the moment it opens.
        void StartClocks();
        bool IsClockRunning(_In_ uint32_t controlIndex) const noexcept;

        bool IsLfoRunning(_In_ uint32_t controlIndex) const noexcept;

        // Runs while held rather than latching. The surface needs this to decide whether a
        // press is a toggle or a hold, and the answer is per control rather than per kind.
        bool LfoLatchesAt(_In_ uint32_t controlIndex) const noexcept;

        // The same two questions for a step sequencer.
        bool AreStepsRunning(_In_ uint32_t controlIndex) const noexcept;
        bool StepsLatchAt(_In_ uint32_t controlIndex) const noexcept;

        // A control feeding a clock its tempo moved. Does nothing unless some clock on this
        // layout named that control.
        void TempoSourceMoved(_In_ uint32_t controlIndex, _In_ double value);

        // Everything this process is driving, not just this player. Blocking work is detached.
        static void Panic();

        // ---- what the owner shows ----

        std::vector<ResolvedDevice> Devices() const { return m_devices.Devices(); }

        bool IsConnected() const noexcept { return !m_sendTable.empty(); }

        BindingEngine const& Engine() const noexcept { return m_engine; }

    private:
        LivePlayer() = default;

        void RebuildThrottles();
        void ReopenConnections();
        void SendPrepared(_In_ uint32_t controlIndex, _In_ uint32_t count) noexcept;

        // Anything this control does for this trigger beyond the immediate channel voice
        // messages: a dump, a raw message, a sequence, a page change.
        void RunPlan(_In_ uint32_t controlIndex, _In_ MessageTrigger trigger) noexcept;

        // One already-built burst of words straight to a connection. What the sequence runner
        // calls, on the dispatcher's thread.
        void SendWords(
            _In_ uint32_t controlIndex,
            _In_ int32_t destinationIndex,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount) noexcept;

        // On a service callback thread. Resolves what moved and marshals only the answer.
        void OnFeedbackWords(
            _In_ std::wstring const& endpointDeviceId,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        // Everything on the layout that said it follows this clock control's beat.
        void PulseTempoFollowers(
            _In_ uint32_t clockControlIndex,
            _In_ double phase,
            _In_ bool running);

        // Who is holding which note on each pad grid, and what they were set up to do with it.
        struct PadEntry
        {
            uint32_t ControlIndex{ 0 };
            int32_t StartNote{ 48 };
            int32_t BendRange{ 48 };
            PadVoices Voices{};
        };

        PadEntry* FindPadControl(_In_ uint32_t controlIndex) noexcept;

        // What a finger's event came to, turned into words and sent.
        void SendPadActions(
            _In_ uint32_t controlIndex,
            _In_ int32_t bendRange,
            _In_ std::span<PadAction const> actions) noexcept;

        // Every note every pad grid is holding, ended. Run before the engine that started them
        // is replaced, so the note off goes where the note on went.
        void ReleaseAllPads() noexcept;

        // A step sequencer's step started or its note ended.
        void OnStepChanged(
            _In_ uint32_t controlIndex,
            _In_ uint64_t run,
            _In_ int32_t stepIndex,
            _In_ bool starts) noexcept;

        // Stops one sequencer and ends the note it was playing, now, on this thread. Announced
        // unless it is about to be started again, so the surface does not let go of its latch.
        void StopSteps(_In_ uint32_t controlIndex, _In_ bool announce) noexcept;

        // Every sequencer stopped and every note they were playing ended. Run before the engine
        // that started them is replaced, for the same reason as ReleaseAllPads.
        void StopAllSteps(_In_ bool announce) noexcept;

        LayoutDocument m_document{};

        BindingEngine m_engine{};
        ActionPlanSet m_plans{};
        DeviceCatalog m_devices{};

        std::shared_ptr<SequenceRunner> m_runner{};
        std::shared_ptr<ClockGenerator> m_clocks{};
        std::shared_ptr<LfoGenerator> m_lfos{};
        std::shared_ptr<StepSequencer> m_steps{};

        // Which control index each clock generator is, so the page can be walked once at load
        // rather than on every tick.
        struct ClockEntry
        {
            uint32_t ControlIndex{ 0 };
            std::wstring ControlId{};
            ClockSpec Spec{};

            // The control whose value sets the tempo, already resolved. -1 for none.
            int32_t TempoSourceIndex{ -1 };
        };

        std::vector<ClockEntry> m_clockControls{};

        // The same for the sweeps, so a press does not have to walk the document to find out
        // what it is pressing.
        struct LfoEntry
        {
            uint32_t ControlIndex{ 0 };
            LfoSpec Spec{};
        };

        std::vector<LfoEntry> m_lfoControls{};

        // The step sequencers, and the note each one has sounding. 0xFFFF is none. Only ever
        // touched on the dispatcher's thread, which is also the thread every note is sent from,
        // so a stop always knows which note to end.
        struct StepsEntry
        {
            uint32_t ControlIndex{ 0 };
            std::wstring ControlId{};
            StepsSpec Spec{};
            uint64_t Run{ 0 };
            uint16_t SoundingNote{ 0xFFFF };
        };

        std::vector<StepsEntry> m_stepControls{};

        StepsEntry* FindStepControl(_In_ uint32_t controlIndex) noexcept;

        std::vector<PadEntry> m_padControls{};

        // One per control, so a fader on a DIN cable can be limited without touching a note on.
        std::vector<ValueThrottle> m_throttles{};

        // The second axis has its own limiter for the same reason it has its own messages.
        std::vector<ValueThrottle> m_throttlesY{};

        // What each keyboard is playing, so releasing a key sends the note it started rather
        // than whatever the key would be after an edit.
        std::vector<uint16_t> m_soundingNotes{};

        // How many clock messages have arrived for each control following the wire's beat.
        // Twenty four of them is a quarter note, and that is when the lamp blinks.
        std::vector<int32_t> m_clockTickCounts{};

        // Rebuilt when the device table changes, read only on the UI thread, so the path a finger
        // takes never waits on a lock. Swapped whole rather than edited.
        std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>> m_sendTable{};

        // Caller-owned, reused, never resized on the hot path.
        std::array<PreparedSend, MaximumSendsPerEvent> m_sends{};

        std::wstring m_ownerId{};

        // What the device table resolved to last time. An unrelated device arriving must not
        // rebuild connections that did not change, and must not replay the startup values.
        std::wstring m_destinationSignature{};

        bool m_startupValuesSent{ false };
        bool m_started{ false };
        bool m_stopping{ false };
        bool m_outputEnabled{ true };

        // Read on a service callback thread and written on the UI thread, so it is atomic rather
        // than a plain bool.
        std::atomic<bool> m_learning{ false };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
    };
}
