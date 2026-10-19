// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once

#include <sal.h>

#include "UmpDispatcher.h"
#include "UmpRenderSource.h"

namespace MidiSynth
{
    enum class WithoutAudioResult
    {
        // Nothing is left in the queue.
        Drained,

        // A channel voice message is at the head of the queue, left there for the audio thread.
        NeedsAudio,
    };

    // Dispatches what is queued while nothing renders, so the endpoint still answers discovery,
    // MIDI-CI and identity. The caller must be the queue's only consumer, with no audio thread
    // running.
    //
    // A channel voice message is the only thing that has to be heard, so one is never taken here
    // unless notes are being discarded. It stays at the head of the queue for the audio thread the
    // caller is about to start, rather than being played to nothing. When no audio device can be
    // had, discardNotes keeps everything moving instead: note ons are dropped, because they would
    // start voices that nothing renders, and everything else is dispatched, so each channel is
    // still set up the way the sender left it.
    WithoutAudioResult DispatchWithoutAudio(
        _Inout_ UmpInboundQueue& queue,
        _Inout_ UmpDispatcher& dispatcher,
        _In_ bool discardNotes) noexcept;
}
