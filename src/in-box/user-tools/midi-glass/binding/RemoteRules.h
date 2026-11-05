// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK. Which rule an arriving message sets off, and
// what that rule then does to the layout, is arithmetic on a document, and it is tested as
// arithmetic. Running the answer is LivePlayer's job (LivePlayerRemote.cpp).

#include <sal.h>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // One rule an arriving message set off, and what the message carried.
    struct RemoteHit
    {
        uint32_t RuleIndex{ 0 };

        // The number a rule can take from the message: a program change's program, a control
        // change's value, or a note's number. 0 to 127.
        int32_t Carried{ 0 };

        // The message's value, 0 to 1: a control change's value, or a note's velocity. A program
        // change has none, and carries its program over 127 instead.
        double Value{ 0.0 };

        // Where it came from and what it was, filled in by whoever runs the rule, so nothing
        // sent in answer is the same message going straight back.
        int32_t DeviceIndex{ -1 };
        uint32_t Words[2]{};
        uint32_t WordCount{ 0 };
    };

    // The most rules one message can set off. More than this on one message is a layout from a
    // stranger rather than a set list.
    constexpr size_t MaximumRemoteHitsPerMessage = 16;

    // Whether a rule reads the number or the value of the message that set it off. Such a rule
    // fires on every value a control change sends, including 0; every other rule fires on a
    // control change only above 0, so a foot switch's release does not run it a second time.
    bool TakesFromMessage(_In_ RemoteRule const& rule) noexcept;

    // The layout's rules with every device name resolved to an index, matched against what
    // arrives. Built on the UI thread, read on the service's callback thread, so it is built whole
    // and never changed afterwards.
    class RemoteRuleTable
    {
    public:
        void Prepare(_In_ LayoutDocument const& document);

        bool IsEmpty() const noexcept { return m_rules.empty(); }

        // Every rule this message sets off, in list order. deviceIndex is the entry in the
        // layout's device table it came in on, or -1. Fills the caller's span and returns how many
        // were written. Never allocates and never throws.
        uint32_t Match(
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount,
            _In_ int32_t deviceIndex,
            _Inout_ std::span<RemoteHit> hits) const noexcept;

    private:
        struct PreparedRule
        {
            uint32_t RuleIndex{ 0 };

            // -1 is any device. A name not in the table never matches.
            int32_t DeviceIndex{ -1 };
            bool DeviceMissing{ false };

            MessageKind Kind{ MessageKind::ProgramChange };
            int32_t GroupIndex{ 0 };
            int32_t ChannelIndex{ 0 };
            int32_t Number{ 0 };
            bool TakesValue{ false };
        };

        std::vector<PreparedRule> m_rules{};
    };

    // What a rule does for one message, with every name resolved to an index.
    struct RemoteCommand
    {
        RemoteActionKind Action{ RemoteActionKind::NextPage };

        // Page actions: the page to show.
        int32_t PageIndex{ -1 };

        // The controls acted on, by the index the binding engine counts with. Show only this: the
        // control to show.
        std::vector<uint32_t> Controls{};

        // Show only this: the other controls in its group, to hide.
        std::vector<uint32_t> HideControls{};

        // Set control value: 0 to 1.
        double Value{ 0.0 };

        PadGridFunction PadFunction{ PadGridFunction::OctaveUp };
        PadGridTargets PadTargets{};

        // Set key root: 0 to 11. Set scale: a MusicalScale.
        int32_t KeyRoot{ -1 };
        int32_t Scale{ -1 };

        std::wstring SequenceName{};
    };

    // False when the rule does nothing for this message: it is off, it names something the layout
    // does not have, or the message picks a page or a scale past the end. currentPageIndex is the
    // page showing, which next and previous count from.
    bool ResolveRemoteCommand(
        _In_ LayoutDocument const& document,
        _In_ RemoteRule const& rule,
        _In_ RemoteHit const& hit,
        _In_ size_t currentPageIndex,
        _Out_ RemoteCommand& command);

    // Which controls are turned off or out of sight while a layout runs. Starts from what each
    // control says it starts as, and is never saved.
    class ControlStateSet
    {
    public:
        // Every control as the layout starts it. A control that is still there with the same
        // starting state keeps what a rule did to it, so an edit in Try mode does not undo a
        // rule.
        void Prepare(_In_ LayoutDocument const& document);

        // Back to what every control starts as.
        void Reset() noexcept;

        bool IsEnabled(_In_ size_t controlIndex) const noexcept;
        bool IsShown(_In_ size_t controlIndex) const noexcept;

        // A control takes a finger only while it is on and in sight.
        bool TakesInput(_In_ size_t controlIndex) const noexcept
        {
            return IsEnabled(controlIndex) && IsShown(controlIndex);
        }

        // Whether any control is off or out of sight, so a page with none pays nothing.
        bool AnyChanged() const noexcept;

        // What an enable, disable, show or hide command does. Returns the controls that changed.
        std::vector<uint32_t> Apply(_In_ RemoteCommand const& command);

        size_t Count() const noexcept { return m_entries.size(); }

    private:
        struct Entry
        {
            std::wstring Id{};
            bool StartsEnabled{ true };
            bool StartsShown{ true };
            bool Enabled{ true };
            bool Shown{ true };
        };

        bool Set(_In_ uint32_t controlIndex, _In_ bool enabledField, _In_ bool value) noexcept;

        std::vector<Entry> m_entries{};
    };

    // Messages this layout sent lately, by device, so a device that sends back what it hears, or
    // a cable looped back, cannot set a rule off with the layout's own output and start a loop.
    // Matched by kind, group, channel and number, not by the exact words, because the service
    // can turn a MIDI 2.0 message into MIDI 1.0 on the way out and back.
    class RemoteEchoGuard
    {
    public:
        static constexpr uint64_t EchoMilliseconds = 250;

        void NoteSent(
            _In_ int32_t deviceIndex,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount,
            _In_ uint64_t nowMilliseconds) noexcept;

        bool IsEcho(
            _In_ int32_t deviceIndex,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount,
            _In_ uint64_t nowMilliseconds) const noexcept;

        void Clear() noexcept;

    private:
        struct Sent
        {
            uint64_t Milliseconds{ 0 };
            int32_t DeviceIndex{ -1 };
            MessageKind Kind{ MessageKind::Note };
            int32_t GroupIndex{ 0 };
            int32_t ChannelIndex{ 0 };
            uint32_t Number{ 0 };
            bool InUse{ false };
        };

        std::array<Sent, 64> m_sent{};
        size_t m_next{ 0 };
    };

    // At most so many rule actions in one second. A loop the echo guard cannot see, through
    // another app and back, still stops rather than taking the window down with it.
    class RemoteRateLimit
    {
    public:
        static constexpr uint32_t MaximumPerSecond = 200;

        bool Allow(_In_ uint64_t nowMilliseconds) noexcept;

    private:
        uint64_t m_windowStart{ 0 };
        uint32_t m_count{ 0 };
    };

    // The words that tell the controller which page is showing. Returns 0 when telling the page
    // is off, its device is not in the table, or the page is one it can't name.
    uint32_t BuildPageFeedback(
        _In_ LayoutDocument const& document,
        _In_ size_t pageIndex,
        _Out_ int32_t& deviceIndex,
        _Out_writes_(1) uint32_t* words) noexcept;

    // A page's position among the pages a running layout moves between, or -1.
    int32_t PagePosition(_In_ LayoutDocument const& document, _In_ size_t pageIndex);
}
