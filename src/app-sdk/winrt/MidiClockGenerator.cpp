// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiClockGenerator.h"
#include "Utilities.Sequencing.MidiClockGenerator.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    _Use_decl_annotations_
    MidiClockGenerator::MidiClockGenerator(
        collections::IVector<midi2::Utilities::Sequencing::MidiClockDestination> const& destinations,
        double bpm, 
        uint16_t const pulsesPerQuarterNote) noexcept
    {
        m_bpm = bpm;
        m_pulsesPerQuarterNote = pulsesPerQuarterNote;

        m_midiTimestampClockTicksPerMinute = internal::GetMidiTimestampFrequency() * 60;

        if (destinations != nullptr && destinations.Size() > 0)
        {
            for (auto const& destination : destinations)
            {
                m_destinations.Append(destination);
            }
        }

    }


    _Use_decl_annotations_
    void MidiClockGenerator::UpdateRunningClock(
        double const bpm) noexcept
    {
        m_bpm = bpm;
    }

    _Use_decl_annotations_
    void MidiClockGenerator::UpdateRunningClock(
        double const bpm, 
        uint16_t const pulsesPerQuarterNote) noexcept
    {
        m_bpm = bpm;
        m_pulsesPerQuarterNote = pulsesPerQuarterNote;
    }



    _Use_decl_annotations_
    bool MidiClockGenerator::Start(
        bool const sendMidiStartMessage) noexcept
    {
        m_sendMidiStartMessage = sendMidiStartMessage;

        // create the queue worker thread
        std::jthread workerThread(
            &MidiClockGenerator::ThreadWorker,
            this);

        m_workerThread = std::move(workerThread);
        m_workerThreadStopToken = m_workerThread.get_stop_token();

        m_workerThread.detach();

        return true;
    }

    _Use_decl_annotations_
    bool MidiClockGenerator::Stop(
        bool const sendMidiStopMessage) noexcept
    {
        m_sendMidiStopMessage = sendMidiStopMessage;

        return m_workerThread.get_stop_source().request_stop();
    }



    _Use_decl_annotations_
    void MidiClockGenerator::SendMessageToAllDestinations(internal::MidiTimestamp const timestamp, uint32_t const message)
    {
        for (auto const& destination : m_destinations)
        {
            for (auto const& group : destination.Groups())
            {
                destination.Connection().SendSingleMessageWords(
                    timestamp,
                    internal::GetFirstWordWithNewGroup(message, group.Index()));

            }
        }

    }

    _Use_decl_annotations_
    void MidiClockGenerator::SendClockToAllDestinations(internal::MidiTimestamp const timestamp)
    {
        for (auto const& destination : m_destinations)
        {
            destination.Connection().SendMultipleMessagesWordList(
                timestamp,
                winrt::get_self<midi2::Utilities::Sequencing::implementation::MidiClockDestination>(destination)->MessagesForAllGroups);
        }

    }



    void MidiClockGenerator::ThreadWorker()
    {
        // we send out 2 seconds of messages at a time
        const uint16_t maxScheduledSeconds = 2;

        if (m_sendMidiStartMessage)
        {
            SendMessageToAllDestinations(0, m_umpMidiStartMessage);
        }

        uint64_t now = internal::GetCurrentMidiTimestamp();
        uint64_t batchStartTimestamp{ now };

        while (!m_workerThreadStopToken.stop_requested())
        {
            uint16_t maxScheduledMessages = static_cast<uint16_t>(((m_pulsesPerQuarterNote * m_bpm) / 60 * maxScheduledSeconds));

            // calculate delta ticks at current bpm and ppqn
            double deltaTicks = (double)m_midiTimestampClockTicksPerMinute / (double)m_bpm / (double)m_pulsesPerQuarterNote;

            uint64_t messageTimestamp{ 0 };

            // schedule next chunk of clock messages
            for (int i = 0; i < maxScheduledMessages && !m_workerThreadStopToken.stop_requested(); i++)
            {
                // send for each connection for each group

                messageTimestamp = static_cast<uint64_t>((i * deltaTicks) + batchStartTimestamp);

                SendClockToAllDestinations(messageTimestamp);
            }

            // next batch starts at the last timestamp + the delta
            batchStartTimestamp = static_cast<uint64_t>(messageTimestamp + deltaTicks);

            // sleep. May need to adjust this timing. Need to keep track of last timing and go with delta from that.
            // this sleep is not really accurate enough
            
            //std::this_thread::sleep_until();
            Sleep(maxScheduledSeconds * 1000);
        }

        if (m_sendMidiStopMessage)
        {
            // we send the stop message immediately
            SendMessageToAllDestinations(0, m_umpMidiStopMessage);

            // TODO: it would be helpful here if we had a service API to clear the outgoing message scheduler queue for a connection.
            Sleep(maxScheduledSeconds * 1000 + 200);
        }

    }

}
