// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RecentSequencesTests.h"

#include "RecentSequences.h"

#include <string>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    std::wstring PathFor(size_t index)
    {
        return L"C:\\Music\\Song " + std::to_wstring(index) + L".midisequence";
    }

    // Song 0 is the oldest; the list ends newest first.
    std::vector<RecentSequence> Opened(size_t count)
    {
        std::vector<RecentSequence> list{};

        for (size_t i = 0; i < count; ++i)
        {
            RememberSequence(list, PathFor(i), static_cast<int64_t>(1000 + i));
        }

        return list;
    }

    bool Has(std::vector<RecentSequence> const& list, std::wstring const& path)
    {
        for (auto const& entry : list)
        {
            if (SameSequencePath(entry.Path, path))
            {
                return true;
            }
        }

        return false;
    }
}

void RecentSequencesTests::TheNewestIsFirstAndOpeningAgainMovesItUp()
{
    auto list = Opened(3);

    VERIFY_ARE_EQUAL(size_t{ 3 }, list.size());
    VERIFY_ARE_EQUAL(PathFor(2), list[0].Path);
    VERIFY_ARE_EQUAL(PathFor(0), list[2].Path);

    RememberSequence(list, PathFor(0), 2000);

    VERIFY_ARE_EQUAL(size_t{ 3 }, list.size());
    VERIFY_ARE_EQUAL(PathFor(0), list[0].Path);
    VERIFY_ARE_EQUAL(int64_t{ 2000 }, list[0].UsedAt);
    VERIFY_ARE_EQUAL(PathFor(2), list[1].Path);

    // Nothing to remember.
    RememberSequence(list, L"", 3000);
    RememberSequence(list, std::wstring(MaximumRecentPathLength + 1, L'a'), 3000);
    VERIFY_ARE_EQUAL(size_t{ 3 }, list.size());
}

void RecentSequencesTests::PathsMatchWithoutCaseOrSlashDirection()
{
    VERIFY_IS_TRUE(SameSequencePath(L"C:\\Music\\Night Drive.midisequence", L"c:/music/NIGHT DRIVE.MIDISEQUENCE"));
    VERIFY_IS_TRUE(SameSequencePath(L"D:\\Kl\u00C4nge\\a.midisequence", L"d:\\kl\u00E4nge\\A.midisequence"));
    VERIFY_IS_FALSE(SameSequencePath(L"C:\\Music\\a.midisequence", L"C:\\Music\\b.midisequence"));
    VERIFY_IS_FALSE(SameSequencePath(L"C:\\Music\\a.midisequence", L"C:\\Music\\a.midisequence.bak"));
    VERIFY_IS_FALSE(SameSequencePath(L"", L""));

    auto list = Opened(2);
    RememberSequence(list, L"c:/music/SONG 0.MIDISEQUENCE", 5000);

    VERIFY_ARE_EQUAL(size_t{ 2 }, list.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"c:/music/SONG 0.MIDISEQUENCE" }, list[0].Path);
}

void RecentSequencesTests::TheOldestAgeOutPastTheLimit()
{
    auto list = Opened(MaximumRecentSequences + 5);

    VERIFY_ARE_EQUAL(MaximumRecentSequences, list.size());
    VERIFY_IS_FALSE(Has(list, PathFor(4)));
    VERIFY_IS_TRUE(Has(list, PathFor(5)));
    VERIFY_ARE_EQUAL(PathFor(MaximumRecentSequences + 4), list[0].Path);
}

void RecentSequencesTests::PinnedSequencesNeverAgeOut()
{
    auto list = Opened(3);
    VERIFY_IS_TRUE(PinSequence(list, PathFor(0), true));

    for (size_t i = 100; i < 100 + MaximumRecentSequences + 10; ++i)
    {
        RememberSequence(list, PathFor(i), static_cast<int64_t>(5000 + i));
    }

    VERIFY_ARE_EQUAL(MaximumRecentSequences + 1, list.size());
    VERIFY_IS_TRUE(Has(list, PathFor(0)));
    VERIFY_IS_FALSE(Has(list, PathFor(1)));
    VERIFY_IS_FALSE(Has(list, PathFor(2)));

    // Unpinned, it stays until the next one opened pushes it out.
    VERIFY_IS_TRUE(PinSequence(list, PathFor(0), false));
    VERIFY_IS_TRUE(Has(list, PathFor(0)));

    RememberSequence(list, PathFor(999), 99999);
    VERIFY_IS_FALSE(Has(list, PathFor(0)));
    VERIFY_ARE_EQUAL(MaximumRecentSequences, list.size());
}

void RecentSequencesTests::OpeningAgainKeepsThePin()
{
    auto list = Opened(3);
    VERIFY_IS_TRUE(PinSequence(list, L"c:\\music\\song 1.midisequence", true));

    RememberSequence(list, PathFor(1), 9000);

    VERIFY_ARE_EQUAL(PathFor(1), list[0].Path);
    VERIFY_IS_TRUE(list[0].Pinned);
}

void RecentSequencesTests::PinningHasALimitAndNeedsAnEntry()
{
    std::vector<RecentSequence> list{};

    for (size_t i = 0; i < MaximumPinnedSequences; ++i)
    {
        RememberSequence(list, PathFor(i), static_cast<int64_t>(i + 1));
        VERIFY_IS_TRUE(PinSequence(list, PathFor(i), true));
    }

    RememberSequence(list, PathFor(MaximumPinnedSequences), 5000);

    VERIFY_IS_FALSE(PinSequence(list, PathFor(MaximumPinnedSequences), true));
    VERIFY_IS_FALSE(PinSequence(list, L"C:\\Music\\Not in the list.midisequence", true));

    // Pinning one that's pinned already is fine.
    VERIFY_IS_TRUE(PinSequence(list, PathFor(0), true));
}

void RecentSequencesTests::ForgettingRemovesEverySpelling()
{
    auto list = Opened(3);
    ForgetSequence(list, L"C:/MUSIC/song 1.midisequence");

    VERIFY_ARE_EQUAL(size_t{ 2 }, list.size());
    VERIFY_IS_FALSE(Has(list, PathFor(1)));

    ForgetSequence(list, L"C:\\Music\\Not in the list.midisequence");
    VERIFY_ARE_EQUAL(size_t{ 2 }, list.size());
}

void RecentSequencesTests::PinnedComeFirstWhenShown()
{
    auto list = Opened(5);
    VERIFY_IS_TRUE(PinSequence(list, PathFor(0), true));
    VERIFY_IS_TRUE(PinSequence(list, PathFor(3), true));

    auto const shown = RecentSequencesForDisplay(list);

    VERIFY_ARE_EQUAL(size_t{ 5 }, shown.size());
    VERIFY_ARE_EQUAL(PathFor(3), shown[0].Path);
    VERIFY_ARE_EQUAL(PathFor(0), shown[1].Path);
    VERIFY_ARE_EQUAL(PathFor(4), shown[2].Path);
    VERIFY_ARE_EQUAL(PathFor(2), shown[3].Path);
    VERIFY_ARE_EQUAL(PathFor(1), shown[4].Path);
}

void RecentSequencesTests::TheListRoundTrips()
{
    auto list = Opened(4);
    VERIFY_IS_TRUE(PinSequence(list, PathFor(1), true));
    list[0].UsedAt = 1792274562;

    auto const text = WriteRecentSequences(list);
    auto const read = ReadRecentSequences(text);

    VERIFY_ARE_EQUAL(list.size(), read.size());

    for (size_t i = 0; i < list.size(); ++i)
    {
        VERIFY_IS_TRUE(list[i] == read[i]);
    }

    VERIFY_IS_TRUE(ReadRecentSequences(WriteRecentSequences({})).empty());
}

void RecentSequencesTests::ReadingSkipsWhatIsntASequence()
{
    VERIFY_IS_TRUE(ReadRecentSequences(L"").empty());
    VERIFY_IS_TRUE(ReadRecentSequences(L"not json").empty());
    VERIFY_IS_TRUE(ReadRecentSequences(L"{\"path\":\"C:\\\\a.midisequence\"}").empty());

    auto const read = ReadRecentSequences(
        L"[1, \"text\", {\"usedAt\":5}, {\"path\":7}, {\"path\":\"\"}, "
        L"{\"path\":\"C:\\\\Music\\\\Kept.midisequence\",\"usedAt\":\"soon\",\"pinned\":\"yes\"}, "
        L"{\"path\":\"C:\\\\Music\\\\Future.midisequence\",\"usedAt\":1e300}]");

    VERIFY_ARE_EQUAL(size_t{ 2 }, read.size());

    for (auto const& entry : read)
    {
        VERIFY_ARE_EQUAL(int64_t{ 0 }, entry.UsedAt);
        VERIFY_IS_FALSE(entry.Pinned);
    }
}

void RecentSequencesTests::ReadingMergesTheSameSequence()
{
    auto const read = ReadRecentSequences(
        L"[{\"path\":\"C:\\\\Music\\\\A.midisequence\",\"usedAt\":10,\"pinned\":true}, "
        L"{\"path\":\"C:\\\\Music\\\\B.midisequence\",\"usedAt\":20}, "
        L"{\"path\":\"c:/music/a.midisequence\",\"usedAt\":30}]");

    VERIFY_ARE_EQUAL(size_t{ 2 }, read.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"c:/music/a.midisequence" }, read[0].Path);
    VERIFY_ARE_EQUAL(int64_t{ 30 }, read[0].UsedAt);
    VERIFY_IS_TRUE(read[0].Pinned);
    VERIFY_ARE_EQUAL(std::wstring{ L"C:\\Music\\B.midisequence" }, read[1].Path);
}
