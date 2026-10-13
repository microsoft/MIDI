// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <atomic>
#include <mutex>

#include "KsaPinMapUpdateTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    HRESULT const InvalidData{ HRESULT_FROM_WIN32(ERROR_INVALID_DATA) };

    constexpr size_t FilterIdOffset{ offsetof(KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY, FilterId) };

    KsaPinMap::Entry MakeEntry(
        _In_ std::wstring const& filterId,
        _In_ uint32_t const pinId,
        _In_ MidiFlow const flow,
        _In_ uint8_t const groupIndex)
    {
        KsaPinMap::Entry entry{ };
        entry.FilterId = filterId;
        entry.PinId = pinId;
        entry.PinDataFlow = flow;
        entry.GroupIndex = groupIndex;

        return entry;
    }

    // A teVirtualMIDI style port: one filter with a source pin and a destination pin on one group
    std::vector<KsaPinMap::Entry> MakePort(_In_ std::wstring const& filterId, _In_ uint8_t const groupIndex)
    {
        return {
            MakeEntry(filterId, 0, MidiFlowOut, groupIndex),
            MakeEntry(filterId, 1, MidiFlowIn, groupIndex)
        };
    }

    std::vector<KsaPinMap::Entry> Combine(
        _In_ std::vector<KsaPinMap::Entry> first,
        _In_ std::vector<KsaPinMap::Entry> const& second)
    {
        first.insert(first.end(), second.begin(), second.end());

        return first;
    }

    // The layout BuildPinsAndGroupTerminalBlocksPropertyData in both endpoint managers writes: a byte
    // count, then packed entries, each one's filter id terminated and followed by two zero bytes.
    std::vector<uint8_t> WritePinMap(_In_ std::vector<KsaPinMap::Entry> const& entries)
    {
        size_t total{ SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER };

        for (auto const& entry : entries)
        {
            total += SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING + (entry.FilterId.length() + 1) * sizeof(wchar_t);
        }

        std::vector<uint8_t> data(total, 0);

        auto const totalByteCount = static_cast<UINT32>(total);
        memcpy(data.data(), &totalByteCount, sizeof(totalByteCount));

        size_t offset{ SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER };

        for (auto const& entry : entries)
        {
            KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY fixedFields{ };
            fixedFields.ByteCount = static_cast<UINT>(SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING + (entry.FilterId.length() + 1) * sizeof(wchar_t));
            fixedFields.GroupIndex = entry.GroupIndex;
            fixedFields.PinId = entry.PinId;
            fixedFields.PinDataFlow = entry.PinDataFlow;

            memcpy(data.data() + offset, &fixedFields, FilterIdOffset);
            memcpy(data.data() + offset + FilterIdOffset, entry.FilterId.c_str(), (entry.FilterId.length() + 1) * sizeof(wchar_t));

            offset += fixedFields.ByteCount;
        }

        return data;
    }

    HRESULT ParseMap(_In_ std::vector<uint8_t> const& data, _Out_ std::vector<KsaPinMap::Entry>& entries)
    {
        return KsaPinMap::Parse(data.data(), data.size(), entries);
    }

    void PatchUInt32(_Inout_ std::vector<uint8_t>& data, _In_ size_t const offset, _In_ UINT32 const value)
    {
        memcpy(data.data() + offset, &value, sizeof(value));
    }

    void VerifyRejected(_In_ std::vector<uint8_t> const& data, _In_z_ wchar_t const* what)
    {
        std::vector<KsaPinMap::Entry> entries{ MakeEntry(L"left over", 9, MidiFlowIn, 9) };

        VERIFY_ARE_EQUAL(InvalidData, ParseMap(data, entries), what);
        VERIFY_IS_TRUE(entries.empty(), what);
    }

    bool SameEntries(_In_ std::vector<KsaPinMap::Entry> const& a, _In_ std::vector<KsaPinMap::Entry> const& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }

        for (size_t i = 0; i < a.size(); i++)
        {
            if (!KsaPinMap::SameBinding(a[i], b[i]) || a[i].FilterId != b[i].FilterId)
            {
                return false;
            }
        }

        return true;
    }

    // Records what the code under test did, from any thread
    class EventLog
    {
    public:
        void Add(_In_ std::wstring const& event)
        {
            std::lock_guard<std::mutex> guard(m_lock);
            m_events.push_back(event);
        }

        std::vector<std::wstring> Events()
        {
            std::lock_guard<std::mutex> guard(m_lock);
            return m_events;
        }

    private:
        std::mutex m_lock{ };
        std::vector<std::wstring> m_events{ };
    };

    // where the event is in the log, or the log's size when it is not there
    size_t PositionOf(_In_ std::vector<std::wstring> const& events, _In_ std::wstring const& event)
    {
        return static_cast<size_t>(std::find(events.begin(), events.end(), event) - events.begin());
    }

    std::wstring const TestEndpoint{ L"\\\\?\\swd#midisrv#midiu_ksa_test#{e7cce071-3c03-423f-88d3-f1045d02552b}" };
    std::wstring const OtherEndpoint{ L"\\\\?\\swd#midisrv#midiu_ksa_other#{e7cce071-3c03-423f-88d3-f1045d02552b}" };

    std::wstring const FilterA{ L"\\\\?\\root#media#0000#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\\wms1275 a" };
    std::wstring const FilterB{ L"\\\\?\\root#media#0000#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\\wms1275 b" };
    std::wstring const FilterC{ L"\\\\?\\root#media#0000#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\\wms1275 c" };
}


void KsaPinMapUpdateTests::TestParseReadsTheLayoutTheEndpointManagerWrites()
{
    // the one character filter id leaves the entries after it unaligned
    std::vector<KsaPinMap::Entry> const written
    {
        MakeEntry(FilterA, 0, MidiFlowOut, 0),
        MakeEntry(L"x", 3, MidiFlowIn, 15),
        MakeEntry(FilterB, 1, MidiFlowIn, 1),
        MakeEntry(L"\\\\?\\usb#vid_0582&pid_0168&mi_00#7&2c0c3b1&0&0000#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\\global", 7, MidiFlowOut, 9)
    };

    std::vector<KsaPinMap::Entry> parsed{ };

    VERIFY_ARE_EQUAL(S_OK, ParseMap(WritePinMap(written), parsed));
    VERIFY_IS_TRUE(SameEntries(written, parsed));
}

void KsaPinMapUpdateTests::TestParseReadsAnEmptyMap()
{
    std::vector<KsaPinMap::Entry> parsed{ MakeEntry(L"left over", 9, MidiFlowIn, 9) };

    VERIFY_ARE_EQUAL(S_OK, ParseMap(WritePinMap({ }), parsed));
    VERIFY_IS_TRUE(parsed.empty());
}

void KsaPinMapUpdateTests::TestParseRejectsMalformedMaps()
{
    auto const valid = WritePinMap({ MakeEntry(L"abc", 0, MidiFlowOut, 0) });
    size_t const entryOffset{ SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER };

    {
        std::vector<KsaPinMap::Entry> entries{ };
        VERIFY_ARE_EQUAL(InvalidData, KsaPinMap::Parse(nullptr, 0, entries));
    }

    VerifyRejected({ 0x04, 0x00 }, L"shorter than the header");

    {
        auto data = valid;
        PatchUInt32(data, 0, static_cast<UINT32>(data.size() + 4));
        VerifyRejected(data, L"total byte count past the end of the data");
    }

    {
        auto data = valid;
        PatchUInt32(data, 0, 2);
        VerifyRejected(data, L"total byte count smaller than the header");
    }

    {
        // a reader that steps by this would never move
        auto data = valid;
        PatchUInt32(data, entryOffset, 0);
        VerifyRejected(data, L"entry byte count of zero");
    }

    {
        auto data = valid;
        PatchUInt32(data, entryOffset, static_cast<UINT32>(data.size() - entryOffset + 2));
        VerifyRejected(data, L"entry byte count past the end of the map");
    }

    {
        auto data = valid;
        PatchUInt32(data, 0, static_cast<UINT32>(entryOffset + FilterIdOffset));
        data.resize(entryOffset + FilterIdOffset);
        VerifyRejected(data, L"entry cut off before its filter id");
    }

    {
        auto data = valid;

        for (size_t position = entryOffset + FilterIdOffset; position + 1 < data.size(); position += sizeof(wchar_t))
        {
            data[position] = 'z';
            data[position + 1] = 0;
        }

        VerifyRejected(data, L"filter id with no terminator");
    }

    VerifyRejected(WritePinMap({ MakeEntry(L"", 0, MidiFlowOut, 0) }), L"empty filter id");

    VerifyRejected(WritePinMap({ MakeEntry(L"abc", 0, MidiFlowOut, 16) }), L"group index past the last group");

    VerifyRejected(WritePinMap({ MakeEntry(L"abc", 0, MidiFlowBidirectional, 0) }), L"pin that is neither a source nor a destination");

    {
        // the first entry is fine, the second is not
        auto data = WritePinMap({ MakeEntry(L"abc", 0, MidiFlowOut, 0), MakeEntry(L"def", 1, MidiFlowIn, 0) });
        PatchUInt32(data, valid.size(), 0);
        VerifyRejected(data, L"second entry byte count of zero");
    }
}


void KsaPinMapUpdateTests::TestPlanOpensOnlyAddedPins()
{
    auto const current = MakePort(FilterA, 0);
    auto const desired = Combine(MakePort(FilterA, 0), MakePort(FilterB, 1));

    auto const changes = KsaPinMap::PlanChanges(current, desired);

    VERIFY_IS_TRUE(changes.ToClose.empty());
    VERIFY_IS_TRUE(SameEntries(MakePort(FilterB, 1), changes.ToOpen));
}

void KsaPinMapUpdateTests::TestPlanClosesOnlyRemovedPins()
{
    auto const current = Combine(MakePort(FilterA, 0), MakePort(FilterB, 1));
    auto const desired = MakePort(FilterA, 0);

    auto const changes = KsaPinMap::PlanChanges(current, desired);

    VERIFY_IS_TRUE(SameEntries(MakePort(FilterB, 1), changes.ToClose));
    VERIFY_IS_TRUE(changes.ToOpen.empty());
}

void KsaPinMapUpdateTests::TestPlanReplacesAGroupGivenToAnotherPin()
{
    // B left, and C took the group B had
    auto const current = Combine(MakePort(FilterA, 0), MakePort(FilterB, 1));
    auto const desired = Combine(MakePort(FilterA, 0), MakePort(FilterC, 1));

    auto const changes = KsaPinMap::PlanChanges(current, desired);

    VERIFY_IS_TRUE(SameEntries(MakePort(FilterB, 1), changes.ToClose));
    VERIFY_IS_TRUE(SameEntries(MakePort(FilterC, 1), changes.ToOpen));
}

void KsaPinMapUpdateTests::TestPlanIgnoresFilterIdCase()
{
    std::wstring upperCaseFilterA{ FilterA };
    std::transform(upperCaseFilterA.begin(), upperCaseFilterA.end(), upperCaseFilterA.begin(), towupper);

    auto const changes = KsaPinMap::PlanChanges(MakePort(upperCaseFilterA, 0), MakePort(FilterA, 0));

    VERIFY_IS_TRUE(changes.ToClose.empty());
    VERIFY_IS_TRUE(changes.ToOpen.empty());
}

void KsaPinMapUpdateTests::TestPlanLastEntryForAGroupWins()
{
    std::vector<KsaPinMap::Entry> const desired
    {
        MakeEntry(FilterA, 0, MidiFlowOut, 0),
        MakeEntry(FilterB, 0, MidiFlowOut, 0)
    };

    auto const changes = KsaPinMap::PlanChanges({ }, desired);

    VERIFY_IS_TRUE(changes.ToClose.empty());
    VERIFY_IS_TRUE(SameEntries({ MakeEntry(FilterB, 0, MidiFlowOut, 0) }, changes.ToOpen));
}


void KsaPinMapUpdateTests::TestDeliveryBeforeStartIsDroppedAndInitialWorkRuns()
{
    KsaPinMap::Follower follower{ };
    auto const map = MakePort(FilterA, 0);

    int applied{ 0 };
    int initialWorkRuns{ 0 };

    VERIFY_ARE_EQUAL(S_FALSE, follower.Deliver(1, map));

    VERIFY_ARE_EQUAL(S_OK, follower.Start(
        [&applied](std::vector<KsaPinMap::Entry> const&) { applied++; return S_OK; },
        [&initialWorkRuns]() -> HRESULT { initialWorkRuns++; return S_OK; }));

    VERIFY_ARE_EQUAL(1, initialWorkRuns);
    VERIFY_ARE_EQUAL(0, applied);

    VERIFY_ARE_EQUAL(S_OK, follower.Deliver(2, map));
    VERIFY_ARE_EQUAL(1, applied);
}

void KsaPinMapUpdateTests::TestMapPublishedDuringInitialWorkIsAppliedAfterIt()
{
    KsaPinMap::Registry registry{ };
    KsaPinMap::Subscription subscription{ };
    auto const map = Combine(MakePort(FilterA, 0), MakePort(FilterB, 1));

    EventLog log{ };
    wil::unique_event publishing{ wil::EventOptions::ManualReset };
    std::thread publisher{ };

    auto joinPublisher = wil::scope_exit([&] { if (publisher.joinable()) { publisher.join(); } });

    auto const begun = subscription.Begin(
        registry,
        TestEndpoint,
        [&log](std::vector<KsaPinMap::Entry> const&) { log.Add(L"apply"); return S_OK; },
        [&]() -> HRESULT
        {
            log.Add(L"initial work started");

            publisher = std::thread([&]
                {
                    publishing.SetEvent();
                    LOG_IF_FAILED(registry.Publish(TestEndpoint, map));
                });

            // give the publisher time to reach the follower, where it has to wait
            publishing.wait(5000);
            Sleep(200);

            log.Add(L"initial work finished");

            return S_OK;
        });

    joinPublisher.reset();

    VERIFY_ARE_EQUAL(S_OK, begun);

    auto const events = log.Events();

    VERIFY_ARE_EQUAL(size_t{ 3 }, events.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"initial work started" }, events[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"initial work finished" }, events[1]);
    VERIFY_ARE_EQUAL(std::wstring{ L"apply" }, events[2]);
}

void KsaPinMapUpdateTests::TestPublishReachesOnlyItsEndpoint()
{
    KsaPinMap::Registry registry{ };
    KsaPinMap::Subscription first{ };
    KsaPinMap::Subscription second{ };
    KsaPinMap::Subscription other{ };

    std::atomic<int> testEndpointApplied{ 0 };
    std::atomic<int> otherEndpointApplied{ 0 };

    auto const noInitialWork = []() -> HRESULT { return S_OK; };

    VERIFY_ARE_EQUAL(S_OK, first.Begin(registry, TestEndpoint, [&](std::vector<KsaPinMap::Entry> const&) { testEndpointApplied++; return S_OK; }, noInitialWork));
    VERIFY_ARE_EQUAL(S_OK, second.Begin(registry, TestEndpoint, [&](std::vector<KsaPinMap::Entry> const&) { testEndpointApplied++; return S_OK; }, noInitialWork));
    VERIFY_ARE_EQUAL(S_OK, other.Begin(registry, OtherEndpoint, [&](std::vector<KsaPinMap::Entry> const&) { otherEndpointApplied++; return S_OK; }, noInitialWork));

    VERIFY_ARE_EQUAL(size_t{ 2 }, registry.FollowerCount(TestEndpoint));

    size_t appliedCount{ 0 };
    VERIFY_ARE_EQUAL(S_OK, registry.Publish(TestEndpoint, MakePort(FilterA, 0), &appliedCount));

    VERIFY_ARE_EQUAL(size_t{ 2 }, appliedCount);
    VERIFY_ARE_EQUAL(2, testEndpointApplied.load());
    VERIFY_ARE_EQUAL(0, otherEndpointApplied.load());

    // an endpoint nobody has open
    VERIFY_ARE_EQUAL(S_OK, registry.Publish(L"\\\\?\\swd#midisrv#midiu_ksa_closed", MakePort(FilterA, 0), &appliedCount));
    VERIFY_ARE_EQUAL(size_t{ 0 }, appliedCount);

    first.End();
    VERIFY_ARE_EQUAL(size_t{ 1 }, registry.FollowerCount(TestEndpoint));
}

void KsaPinMapUpdateTests::TestOlderDeliveryIsIgnored()
{
    KsaPinMap::Follower follower{ };
    auto const map = MakePort(FilterA, 0);
    int applied{ 0 };

    VERIFY_ARE_EQUAL(S_OK, follower.Start(
        [&applied](std::vector<KsaPinMap::Entry> const&) { applied++; return S_OK; },
        []() -> HRESULT { return S_OK; }));

    VERIFY_ARE_EQUAL(S_OK, follower.Deliver(5, map));
    VERIFY_ARE_EQUAL(S_FALSE, follower.Deliver(4, map));
    VERIFY_ARE_EQUAL(S_FALSE, follower.Deliver(5, map));
    VERIFY_ARE_EQUAL(S_OK, follower.Deliver(6, map));

    VERIFY_ARE_EQUAL(2, applied);
}

void KsaPinMapUpdateTests::TestFailedInitialWorkLeavesNothingRegistered()
{
    KsaPinMap::Registry registry{ };
    KsaPinMap::Subscription subscription{ };
    int applied{ 0 };

    VERIFY_ARE_EQUAL(E_ACCESSDENIED, subscription.Begin(
        registry,
        TestEndpoint,
        [&applied](std::vector<KsaPinMap::Entry> const&) { applied++; return S_OK; },
        []() -> HRESULT { return E_ACCESSDENIED; }));

    VERIFY_IS_FALSE(subscription.IsActive());
    VERIFY_ARE_EQUAL(size_t{ 0 }, registry.FollowerCount(TestEndpoint));

    size_t appliedCount{ 0 };
    VERIFY_ARE_EQUAL(S_OK, registry.Publish(TestEndpoint, MakePort(FilterA, 0), &appliedCount));
    VERIFY_ARE_EQUAL(size_t{ 0 }, appliedCount);
    VERIFY_ARE_EQUAL(0, applied);
}

void KsaPinMapUpdateTests::TestEndWaitsForADeliveryInProgress()
{
    KsaPinMap::Registry registry{ };
    KsaPinMap::Subscription subscription{ };

    wil::unique_event applying{ wil::EventOptions::ManualReset };
    wil::unique_event release{ wil::EventOptions::ManualReset };
    std::atomic<int> applied{ 0 };
    std::atomic<bool> ended{ false };

    VERIFY_ARE_EQUAL(S_OK, subscription.Begin(
        registry,
        TestEndpoint,
        [&](std::vector<KsaPinMap::Entry> const&)
        {
            applied++;
            applying.SetEvent();
            release.wait(30000);
            return S_OK;
        },
        []() -> HRESULT { return S_OK; }));

    std::thread publisher([&] { LOG_IF_FAILED(registry.Publish(TestEndpoint, MakePort(FilterA, 0))); });
    std::thread ender{ };

    auto joinThreads = wil::scope_exit([&]
        {
            release.SetEvent();

            if (publisher.joinable())
            {
                publisher.join();
            }

            if (ender.joinable())
            {
                ender.join();
            }
        });

    VERIFY_IS_TRUE(applying.wait(5000));

    ender = std::thread([&] { subscription.End(); ended = true; });

    Sleep(300);
    VERIFY_IS_FALSE(ended.load(), L"End returned while a delivery was still running");

    joinThreads.reset();

    VERIFY_IS_TRUE(ended.load());
    VERIFY_ARE_EQUAL(size_t{ 0 }, registry.FollowerCount(TestEndpoint));

    size_t appliedCount{ 0 };
    VERIFY_ARE_EQUAL(S_OK, registry.Publish(TestEndpoint, MakePort(FilterA, 0), &appliedCount));
    VERIFY_ARE_EQUAL(size_t{ 0 }, appliedCount);
    VERIFY_ARE_EQUAL(1, applied.load());
}
