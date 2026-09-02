// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "Arpeggiator.h"

namespace midikeyboard
{
    namespace
    {
        // how much of each step the note sounds for
        constexpr double GateFraction = 0.6;

        constexpr int64_t TicksPerMillisecond = 10000;
    }

    _Use_decl_annotations_
    void Arpeggiator::Initialize(
        winrt::Microsoft::UI::Dispatching::DispatcherQueue const& queue,
        NoteOnHandler onNoteOn,
        NoteOffHandler onNoteOff) noexcept
    {
        try
        {
            m_queue = queue;
            m_onNoteOn = std::move(onNoteOn);
            m_onNoteOff = std::move(onNoteOff);

            if (m_queue == nullptr)
            {
                return;
            }

            m_stepTimer = m_queue.CreateTimer();
            m_stepTimer.IsRepeating(true);
            m_stepTimer.Tick([this](auto&&, auto&&) { OnStep(); });

            m_gateTimer = m_queue.CreateTimer();
            m_gateTimer.IsRepeating(false);
            m_gateTimer.Tick([this](auto&&, auto&&) { SilenceSoundingNote(); });

            ApplyInterval();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to set up the arpeggiator.")
    }

    void Arpeggiator::Shutdown() noexcept
    {
        try
        {
            Stop();

            m_stepTimer = nullptr;
            m_gateTimer = nullptr;
            m_queue = nullptr;
            m_onNoteOn = nullptr;
            m_onNoteOff = nullptr;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to shut down the arpeggiator.")
    }

    _Use_decl_annotations_
    uint32_t Arpeggiator::StepsPerBeat(ArpeggiatorDivision division) noexcept
    {
        switch (division)
        {
        case ArpeggiatorDivision::Quarter:          return 1;
        case ArpeggiatorDivision::Eighth:           return 2;
        case ArpeggiatorDivision::EighthTriplet:    return 3;
        case ArpeggiatorDivision::Sixteenth:        return 4;
        case ArpeggiatorDivision::SixteenthTriplet: return 6;
        case ArpeggiatorDivision::ThirtySecond:     return 8;
        default:                                    return 4;
        }
    }

    _Use_decl_annotations_
    void Arpeggiator::Mode(ArpeggiatorMode value) noexcept
    {
        if (m_mode == value)
        {
            return;
        }

        m_mode = value;

        if (m_mode == ArpeggiatorMode::Off)
        {
            Stop();
            return;
        }

        m_stepIndex = 0;

        if (!m_held.empty())
        {
            Start();
        }
    }

    _Use_decl_annotations_
    void Arpeggiator::Rate(uint32_t bpm, ArpeggiatorDivision division) noexcept
    {
        m_bpm = std::max(1u, bpm);
        m_division = division;

        ApplyInterval();
    }

    void Arpeggiator::ApplyInterval() noexcept
    {
        try
        {
            if (m_stepTimer == nullptr)
            {
                return;
            }

            auto const stepsPerMinute = static_cast<double>(m_bpm) *
                static_cast<double>(StepsPerBeat(m_division));

            auto const milliseconds = std::max(5.0, 60000.0 / std::max(1.0, stepsPerMinute));

            m_stepTimer.Interval(winrt::Windows::Foundation::TimeSpan{
                static_cast<int64_t>(milliseconds * TicksPerMillisecond) });

            m_gateTimer.Interval(winrt::Windows::Foundation::TimeSpan{
                static_cast<int64_t>(milliseconds * GateFraction * TicksPerMillisecond) });
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to set the arpeggiator rate.")
    }

    _Use_decl_annotations_
    void Arpeggiator::HoldNote(int32_t noteNumber, uint16_t velocity) noexcept
    {
        try
        {
            auto const existing = std::find_if(m_held.begin(), m_held.end(),
                [noteNumber](HeldNote const& held) { return held.NoteNumber == noteNumber; });

            if (existing != m_held.end())
            {
                existing->Velocity = velocity;
                return;
            }

            auto const wasEmpty = m_held.empty();

            m_held.push_back(HeldNote{ noteNumber, velocity });

            if (!IsEnabled())
            {
                return;
            }

            if (wasEmpty)
            {
                // a fresh chord always starts at the beginning of the pattern
                m_stepIndex = 0;
                Start();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to hold the note.")
    }

    _Use_decl_annotations_
    void Arpeggiator::ReleaseNote(int32_t noteNumber) noexcept
    {
        try
        {
            std::erase_if(m_held,
                [noteNumber](HeldNote const& held) { return held.NoteNumber == noteNumber; });

            if (m_held.empty())
            {
                Stop();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to release the note.")
    }

    void Arpeggiator::Reset() noexcept
    {
        m_held.clear();
        Stop();
    }

    void Arpeggiator::Start() noexcept
    {
        try
        {
            if (m_stepTimer == nullptr)
            {
                return;
            }

            m_stepTimer.Start();

            // the first note of a chord should sound now, not one step from now
            OnStep();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to start the arpeggiator.")
    }

    void Arpeggiator::Stop() noexcept
    {
        try
        {
            if (m_stepTimer != nullptr)
            {
                m_stepTimer.Stop();
            }

            if (m_gateTimer != nullptr)
            {
                m_gateTimer.Stop();
            }

            SilenceSoundingNote();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to stop the arpeggiator.")
    }

    void Arpeggiator::SilenceSoundingNote() noexcept
    {
        try
        {
            if (m_soundingNote < 0)
            {
                return;
            }

            auto const note = m_soundingNote;
            m_soundingNote = -1;

            if (m_onNoteOff)
            {
                m_onNoteOff(note);
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to silence the arpeggiated note.")
    }

    std::vector<Arpeggiator::HeldNote> Arpeggiator::BuildSequence() const noexcept
    {
        std::vector<HeldNote> sequence{};

        try
        {
            if (m_held.empty())
            {
                return sequence;
            }

            auto sorted = m_held;
            std::sort(sorted.begin(), sorted.end(),
                [](HeldNote const& left, HeldNote const& right)
                {
                    return left.NoteNumber < right.NoteNumber;
                });

            switch (m_mode)
            {
            case ArpeggiatorMode::AsPlayed:
                return m_held;

            case ArpeggiatorMode::Up:
            case ArpeggiatorMode::Random:
                return sorted;

            case ArpeggiatorMode::Down:
                std::reverse(sorted.begin(), sorted.end());
                return sorted;

            case ArpeggiatorMode::UpDown:
                sequence = sorted;

                // the top and bottom notes are not repeated on the turn
                for (size_t i = sorted.size() - 1; i > 1; i--)
                {
                    sequence.push_back(sorted[i - 1]);
                }
                return sequence;

            case ArpeggiatorMode::UpDownRepeat:
                sequence = sorted;

                for (size_t i = sorted.size(); i > 0; i--)
                {
                    sequence.push_back(sorted[i - 1]);
                }
                return sequence;

            default:
                return sorted;
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to build the arpeggiator sequence.")

        return sequence;
    }

    void Arpeggiator::OnStep() noexcept
    {
        try
        {
            SilenceSoundingNote();

            auto const sequence = BuildSequence();

            if (sequence.empty())
            {
                Stop();
                return;
            }

            size_t index{ 0 };

            if (m_mode == ArpeggiatorMode::Random)
            {
                std::uniform_int_distribution<size_t> distribution{ 0, sequence.size() - 1 };
                index = distribution(m_random);
            }
            else
            {
                // the chord can change under us between steps, so the index is always wrapped
                index = m_stepIndex % sequence.size();
                m_stepIndex = index + 1;
            }

            auto const& step = sequence[index];

            m_soundingNote = step.NoteNumber;

            if (m_onNoteOn)
            {
                m_onNoteOn(step.NoteNumber, step.Velocity);
            }

            if (m_gateTimer != nullptr)
            {
                m_gateTimer.Start();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to advance the arpeggiator.")
    }
}
