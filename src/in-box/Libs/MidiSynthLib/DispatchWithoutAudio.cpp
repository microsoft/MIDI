// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/DispatchWithoutAudio.h"

namespace MidiSynth
{
    namespace
    {
        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;
        constexpr uint32_t StatusNoteOn = 0x9;
    }

    _Use_decl_annotations_
    WithoutAudioResult DispatchWithoutAudio(UmpInboundQueue& queue, UmpDispatcher& dispatcher, bool discardNotes) noexcept
    {
        QueuedUmp message{};

        // Looks before it takes, so a channel voice message is only ever taken to be dropped.
        while (queue.TryPeek(message))
        {
            const uint32_t messageType = (message.Words[0] >> 28) & 0xF;
            const bool channelVoice =
                messageType == MessageTypeMidi1ChannelVoice || messageType == MessageTypeMidi2ChannelVoice;

            if (channelVoice && !discardNotes)
            {
                return WithoutAudioResult::NeedsAudio;
            }

            (void)queue.TryPop(message);

            if (channelVoice && ((message.Words[0] >> 20) & 0xF) == StatusNoteOn)
            {
                continue;
            }

            (void)dispatcher.ProcessWords(message.Words, message.WordCount);
        }

        return WithoutAudioResult::Drained;
    }
}
