// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML, Win2D and the MIDI SDK. What a control puts on the wire is
// arithmetic on a document, and it is tested as arithmetic. Opening a connection and actually
// sending is the router's job, one layer up.

#include <sal.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // What the endpoint on the other end will end up receiving. This is NOT what the engine
    // builds: the engine always sends MIDI 2.0 protocol UMP and the service downscales, both from
    // MIDI 2.0 protocol to MIDI 1.0 protocol and from UMP to MIDI 1.0 byte format, for whichever
    // of the client or the device needs it. This is here so the editor can show a customer what
    // their message becomes on this particular device.
    enum class DestinationProtocol
    {
        Midi1 = 0,
        Midi2 = 1,
    };

    // One entry of the layout's device table, once it has been matched to something real.
    struct PreparedDestination
    {
        std::wstring Name{};
        DestinationProtocol Protocol{ DestinationProtocol::Midi1 };

        // False while the device is missing. Controls bound to it stop sending rather than
        // throwing, and come back on their own when it returns.
        bool IsAvailable{ false };
    };

    // One message, with every name already resolved to an index. No string comparison and no map
    // lookup happens after a layout is loaded.
    struct PreparedMessage
    {
        MessageTrigger Trigger{ MessageTrigger::Changes };
        MessageKind Kind{ MessageKind::ControlChange };

        // -1 when the message names a device that is not in the table. It is kept rather than
        // dropped so the editor can still show the row and say what is wrong.
        int32_t DestinationIndex{ -1 };

        uint8_t GroupIndex{ 0 };
        uint8_t ChannelIndex{ 0 };
        uint16_t Number{ 0 };

        MessageValue Minimum{ 0.0, ValueScaling::Fraction };
        MessageValue Maximum{ 1.0, ValueScaling::Fraction };

        MessageDetents Detents{};

        // Which of a two axis control's values drives this. Every other control only ever has
        // the one, so everything that does not ask leaves this at X and behaves as before.
        ValueAxis Axis{ ValueAxis::X };

        // Send as MIDI 1.0 protocol so the seven bit value is exactly what was typed.
        bool UseMidi1Protocol{ false };
    };

    struct PreparedControl
    {
        uint32_t FirstMessage{ 0 };
        uint32_t MessageCount{ 0 };

        float DefaultValue{ 0.0f };
        bool SendsValueOnStart{ false };
    };

    // One message ready to hand to a connection. Fixed size on purpose: the hot path fills a
    // caller-owned array of these and never allocates.
    struct PreparedSend
    {
        int32_t DestinationIndex{ -1 };
        uint32_t WordCount{ 0 };
        uint32_t Words[4]{};
    };

    // The most messages one control can produce from one event. A control has a list of messages
    // and they are all the same trigger at most, so this is the per-control cap.
    constexpr size_t MaximumSendsPerEvent = MaximumMessagesPerControl;

    // Turns a control moving into the words that leave the process.
    //
    // From a finger touching glass to a message leaving there is one frame of budget and no
    // allocations, so everything expensive happens in Prepare and Evaluate only does arithmetic.
    class BindingEngine
    {
    public:
        // Resolves every message's device name to an index into the destination table. Call it
        // when a layout loads, and again when the device table changes. Not on the hot path.
        void Prepare(
            _In_ LayoutDocument const& document,
            _In_ std::vector<PreparedDestination> destinations) noexcept;

        // Control order is page order then control order within the page, which is the order
        // Prepare walked them and the order the surface indexes them by.
        size_t ControlCount() const noexcept { return m_controls.size(); }

        std::vector<PreparedDestination> const& Destinations() const noexcept { return m_destinations; }

        // Fills as many sends as fit and returns how many were written. Never allocates, never
        // throws, and never touches a string.
        //
        // A message whose destination is missing or unresolved is skipped rather than queued:
        // stale MIDI arriving late is worse than nothing.
        uint32_t Evaluate(
            _In_ size_t controlIndex,
            _In_ MessageTrigger trigger,
            _In_ double value,
            _Inout_ std::span<PreparedSend> sends) const noexcept;

        // The same, for one axis of a two axis control. A message says which axis it follows,
        // so an XY pad moving sideways sends only the rows bound to sideways.
        uint32_t EvaluateAxis(
            _In_ size_t controlIndex,
            _In_ MessageTrigger trigger,
            _In_ double value,
            _In_ ValueAxis axis,
            _Inout_ std::span<PreparedSend> sends) const noexcept;

        // A key on a piano keyboard. The note number comes from the key rather than from the
        // message's own number, which is the whole difference between a keyboard and a pad.
        uint32_t EvaluateNote(
            _In_ size_t controlIndex,
            _In_ uint16_t note,
            _In_ double velocity,
            _In_ bool isOn,
            _Inout_ std::span<PreparedSend> sends) const noexcept;

        // A system real time byte from a clock generator, to every device this control names.
        // Real time messages carry no channel and no value, so there is nothing to interpolate.
        uint32_t EvaluateSystemRealTime(
            _In_ size_t controlIndex,
            _In_ uint8_t status,
            _Inout_ std::span<PreparedSend> sends) const noexcept;

        // The initialization pass a layout runs when it opens, in keyboard order, so a synth can
        // be put into a known state. Returns how many sends were written.
        uint32_t EvaluateStartupValues(_Inout_ std::span<PreparedSend> sends) const noexcept;

        // Incoming feedback, the mirror of Evaluate: which control this message addresses and
        // what value it carries. Returns false when nothing on this layout wants it.
        bool TryResolveFeedback(
            _In_ uint32_t const* words,
            _In_ uint32_t wordCount,
            _Out_ size_t& controlIndex,
            _Out_ double& value) const noexcept;

        // What a listening control saw. An activity light blinks, a transport light latches,
        // and a beat light has to be told about every clock message so somebody can count them.
        enum class FeedbackHitKind
        {
            // Something arrived. Blink.
            Pulse = 0,

            // Start or continue. Stays lit until told otherwise.
            On = 1,

            // Stop. Goes dark.
            Off = 2,

            // One MIDI clock message. Twenty four of them is a quarter note; counting them is
            // the caller's job, because this class holds no state between messages.
            ClockTick = 3,
        };

        struct FeedbackHit
        {
            size_t ControlIndex{ 0 };
            FeedbackHitKind Kind{ FeedbackHitKind::Pulse };
        };

        // Every control watching for something this message satisfies. Fills the caller's span
        // and returns how many were written.
        //
        // Separate from TryResolveFeedback because one arriving message can light several of
        // them at once, and because none of these modes carries a value to move a control to.
        uint32_t CollectFeedbackHits(
            _In_ uint32_t const* words,
            _In_ uint32_t wordCount,
            _In_ int32_t destinationIndex,
            _Inout_ std::span<FeedbackHit> hits) const noexcept;

        // Which controls follow a tempo, and where each one gets it from. Empty when nothing
        // on the layout is watching the beat.
        struct TempoWatcher
        {
            size_t ControlIndex{ 0 };

            // Empty means whatever clock is arriving from the device it names.
            std::wstring ClockControlId{};
        };

        std::vector<TempoWatcher> const& TempoWatchers() const noexcept { return m_tempoWatchers; }

        // How many stops this control has, so the surface can snap a finger to them and draw the
        // notches. 0 means it is smooth. Where a control sends several messages the widest set of
        // stops wins, because a finger has one position and the messages have to agree on it.
        uint32_t DetentCountForControl(_In_ size_t controlIndex) const noexcept;

        // The nearest stop to this position, or the position unchanged where there are none.
        double SnapToDetent(_In_ size_t controlIndex, _In_ double position) const noexcept;

        // What this control's first message would put on the wire at this position, for the
        // number a control can show inside itself.
        //
        // Returns false when the control sends nothing on that axis. `isAbsolute` is what decides
        // how it should be read: somebody who typed 0 to 127 out of a device manual wants to see
        // 64, and somebody who left it at 0 % to 100 % wants to see a percentage. Showing a
        // percentage to the first of them is exactly the complaint the ranges exist to answer.
        bool TryDescribeValue(
            _In_ size_t controlIndex,
            _In_ ValueAxis axis,
            _In_ double position,
            _Out_ uint32_t& value,
            _Out_ bool& isAbsolute) const noexcept;

    private:
        struct PreparedFeedback
        {
            size_t ControlIndex{ 0 };
            int32_t DestinationIndex{ -1 };
            FeedbackMode Mode{ FeedbackMode::Message };
            MessageKind Kind{ MessageKind::ControlChange };
            uint8_t GroupIndex{ 0 };
            uint8_t ChannelIndex{ 0 };
            uint16_t Number{ 0 };

            // Activity mode: whether the group and the channel narrow it, or any traffic from
            // the device counts.
            bool AnyGroup{ false };
            bool AnyChannel{ true };

            // Tempo mode: whether the beat comes from the wire or from a clock on this layout.
            bool TempoFromWire{ true };
        };

        std::vector<PreparedDestination> m_destinations{};
        std::vector<PreparedControl> m_controls{};
        std::vector<PreparedMessage> m_messages{};
        std::vector<PreparedFeedback> m_feedback{};
        std::vector<TempoWatcher> m_tempoWatchers{};

        // Keyboard order, resolved once, so the startup pass does not sort on every run.
        std::vector<size_t> m_startupOrder{};
    };

    // ---- the wire, exposed so it can be tested on its own ----

    // A fraction of full scale to an unsigned value of the given width. 0.0 is the bottom of the
    // range and 1.0 is the top, so a fader at the top of its travel sends the maximum the wire can
    // carry rather than one short of it.
    uint32_t ScaleToBits(_In_ double fraction, _In_ uint32_t bits) noexcept;

    // An exact value into the same field, clamped rather than scaled. This is what a customer
    // copying a device's documentation gets.
    uint32_t ClampToBits(_In_ double value, _In_ uint32_t bits) noexcept;

    // One end of a range, in the units of a field this wide.
    uint32_t ResolveEnd(_In_ MessageValue const& end, _In_ uint32_t bits) noexcept;

    // Where a control sitting at this position lands between the two ends. Rounding is what
    // quantizes a fader limited to 0 to 127 onto whole numbers.
    uint32_t InterpolateValue(
        _In_ PreparedMessage const& message,
        _In_ double position,
        _In_ uint32_t bits) noexcept;

    // How many stops this message has, so the surface can snap a finger to them and draw them.
    // 0 means it is smooth. The engine produces the right value either way; this is only needed
    // by whatever moves the control.
    uint32_t DetentCount(_In_ PreparedMessage const& message, _In_ uint32_t bits) noexcept;

    // The position, 0 to 1, of one stop. Stops are evenly spaced in travel.
    double DetentPosition(_In_ uint32_t index, _In_ uint32_t count) noexcept;

    // How wide the field this message lands in is, which is what the two ends and the stops are
    // measured against. MIDI 2.0 protocol unless the message asked for MIDI 1.0.
    uint32_t FieldBitsFor(_In_ PreparedMessage const& message) noexcept;

    // MIDI 1.0 channel voice, message type 2. One word.
    uint32_t BuildMidi1ChannelVoice(
        _In_ uint8_t group,
        _In_ uint8_t status,
        _In_ uint8_t channel,
        _In_ uint8_t data1,
        _In_ uint8_t data2) noexcept;

    // MIDI 2.0 channel voice, message type 4. Two words.
    void BuildMidi2ChannelVoice(
        _In_ uint8_t group,
        _In_ uint8_t status,
        _In_ uint8_t channel,
        _In_ uint8_t index1,
        _In_ uint8_t index2,
        _In_ uint32_t data,
        _Out_writes_(2) uint32_t* words) noexcept;

    // The words one prepared message produces at this value.
    //
    // MIDI 2.0 protocol by default, because nothing here should fold to seven bits: the service
    // owns the canonical downscale, it is the same one every other app on the machine gets, and
    // it can do things this app cannot, such as expanding one registered controller into the four
    // MIDI 1.0 messages that carry it, or converting UMP to MIDI 1.0 byte format for a WinMM
    // client. A message marked UseMidi1Protocol is built as MIDI 1.0 protocol instead, so a value
    // that is a code rather than a position lands on the wire exactly as typed.
    //
    // Returns the word count, 0 when the message does not go on the wire as a channel voice
    // message.
    uint32_t BuildMessageWords(
        _In_ PreparedMessage const& message,
        _In_ double value,
        _Out_writes_(4) uint32_t* words) noexcept;

    // The MIDI 1.0 protocol form, used when a message asks for it and by the editor to show what
    // a MIDI 1.0 device will receive.
    uint32_t BuildMidi1ProtocolWords(
        _In_ PreparedMessage const& message,
        _In_ double value,
        _Out_writes_(4) uint32_t* words) noexcept;
}
