// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

#include "MidiWordsReceivedEventArgs.g.h"

namespace internal = ::Windows::Devices::Midi2::Internal;

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiWordsReceivedEventArgs : MidiWordsReceivedEventArgsT<MidiWordsReceivedEventArgs>
    {
        MidiWordsReceivedEventArgs() = default;
        
        MidiWordsReceivedEventArgs(internal::MidiTimestamp timestamp, void* data, uint32_t wordCount)
        {
            WINRT_ASSERT(data != nullptr);
            WINRT_ASSERT(wordCount > 0);

            _timestamp = timestamp;
            _words = std::make_unique<uint32_t[]>(wordCount);

            memcpy(_words.get(), data, wordCount * sizeof(uint32_t));
        }

        com_array<uint32_t> Words() { return com_array<uint32_t>(_words.get() [_wordCount]); }

        uint32_t WordCount() { return _wordCount; }
        uint64_t Timestamp() { return _timestamp; }

    private:
        internal::MidiTimestamp _timestamp{};
        std::unique_ptr<uint32_t[]> _words;
        uint32_t _wordCount{};

    };
}

