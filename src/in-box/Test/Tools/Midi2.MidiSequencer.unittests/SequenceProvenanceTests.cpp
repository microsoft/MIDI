// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceProvenanceTests.h"

#include "SequenceProvenance.h"
#include "SequenceSerializer.h"

#include <initializer_list>
#include <string>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    namespace types = midiapp::DigitalSourceTypes;

    // A sequence with one track that places a clip of each origin given, and one more clip of
    // unusedOrigin that nothing uses.
    Sequence WithClips(std::initializer_list<ClipOrigin> origins, ClipOrigin unusedOrigin = ClipOrigin::Unknown)
    {
        Sequence sequence{};
        Track track{};
        track.Id = L"t-1";
        track.Name = L"Track";

        int64_t tick{ 0 };

        for (auto const origin : origins)
        {
            Clip clip{};
            clip.Id = L"c-" + std::to_wstring(sequence.Clips.size());
            clip.Origin = origin;
            sequence.Clips.push_back(clip);
            track.Timeline.push_back(Placement{ clip.Id, tick, 0 });
            tick += TicksPerQuarterNote * 4;
        }

        if (unusedOrigin != ClipOrigin::Unknown)
        {
            Clip unused{};
            unused.Id = L"c-unused";
            unused.Origin = unusedOrigin;
            sequence.Clips.push_back(unused);
        }

        sequence.Tracks.push_back(track);
        return sequence;
    }
}

void SequenceProvenanceTests::OneKindOfClipSuggestsThatKind()
{
    VERIFY_ARE_EQUAL(std::wstring{ types::DigitalCapture }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Recorded, ClipOrigin::Recorded })));
    VERIFY_ARE_EQUAL(std::wstring{ types::DigitalCreation }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Drawn })));
    VERIFY_ARE_EQUAL(std::wstring{ types::AlgorithmicMedia }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Generated })));

    // A file alone says nothing about how it was made, and neither does an empty sequence.
    VERIFY_ARE_EQUAL(std::wstring{ types::DigitalCreation }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Imported })));
    VERIFY_ARE_EQUAL(std::wstring{ types::DigitalCreation }, SuggestDigitalSourceType(WithClips({})));
}

void SequenceProvenanceTests::AMixSuggestsComposite()
{
    VERIFY_ARE_EQUAL(std::wstring{ types::Composite }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Recorded, ClipOrigin::Drawn })));
    VERIFY_ARE_EQUAL(std::wstring{ types::Composite }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Imported, ClipOrigin::Generated })));
}

void SequenceProvenanceTests::AnAssistantsClipIsAlwaysDisclosed()
{
    VERIFY_ARE_EQUAL(std::wstring{ types::TrainedAlgorithmicMedia }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Assistant })));
    VERIFY_ARE_EQUAL(std::wstring{ types::CompositeWithTrainedAlgorithmicMedia }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Drawn, ClipOrigin::Assistant })));
    VERIFY_IS_TRUE(midiapp::InvolvesGenerativeAi(SuggestDigitalSourceType(WithClips({ ClipOrigin::Recorded, ClipOrigin::Assistant }))));
}

void SequenceProvenanceTests::OnlyClipsInUseCount()
{
    // A clip nothing plays doesn't change the answer, even one an assistant wrote.
    VERIFY_ARE_EQUAL(std::wstring{ types::DigitalCapture }, SuggestDigitalSourceType(WithClips({ ClipOrigin::Recorded }, ClipOrigin::Assistant)));

    // A clip in a launcher slot counts.
    auto sequence = WithClips({ ClipOrigin::Recorded }, ClipOrigin::Generated);
    sequence.Tracks[0].Slots.push_back(L"c-unused");
    VERIFY_ARE_EQUAL(size_t{ 2 }, ClipsInUse(sequence).size());
    VERIFY_ARE_EQUAL(std::wstring{ types::Composite }, SuggestDigitalSourceType(sequence));
}

void SequenceProvenanceTests::TheSequenceFileKeepsTheCopyrightNotice()
{
    auto original = WithClips({ ClipOrigin::Drawn });
    original.Name = L"Song";
    original.Copyright = L"Copyright 2026 \"Example\" Music";
    NormalizeSequence(original);

    auto const text = WriteSequenceJson(original);

    Sequence read{};
    VERIFY_IS_TRUE(ReadSequenceJson(text, read).Succeeded());
    VERIFY_ARE_EQUAL(original.Copyright, read.Copyright);
    VERIFY_ARE_EQUAL(text, WriteSequenceJson(read));
}
