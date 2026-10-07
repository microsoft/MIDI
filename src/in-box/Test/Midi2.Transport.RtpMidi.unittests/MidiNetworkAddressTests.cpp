// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAddressTests.h"

#include <chrono>
#include <stop_token>

#include "midi_network_addresses.h"

using namespace WindowsMidiServicesInternal;
using namespace WEX::Common;
using namespace WEX::Logging;

#define CHECK_TEXT(actual, expected) \
    do { std::wstring const a_ = (actual); std::wstring const e_ = (expected); VERIFY_IS_TRUE(a_ == e_, WEX::Common::String().Format(L"%hs: got \"%s\", expected \"%s\"", #actual, a_.c_str(), e_.c_str())); } while (0)

namespace
{
    std::wstring Join(_In_ std::vector<std::wstring> const& addresses)
    {
        std::wstring text;

        for (auto const& address : addresses)
        {
            if (!text.empty()) text += L", ";
            text += address;
        }

        return text.empty() ? std::wstring{ L"(none)" } : text;
    }

    unsigned int CountOf(_In_ std::vector<std::wstring> const& addresses, _In_ std::wstring const& address)
    {
        return static_cast<unsigned int>(std::count(addresses.begin(), addresses.end(), address));
    }
}


void MidiNetworkAddressTests::TestSortDoesNotDependOnTheOrderGiven()
{
    auto const first = SortMidiNetworkAddresses({ L"127.0.0.1", L"::1" });
    auto const second = SortMidiNetworkAddresses({ L"::1", L"127.0.0.1" });

    Log::Comment(String().Format(L"Windows' order: %s", Join(first).c_str()));

    VERIFY_IS_TRUE(first == second, L"the same order whichever was given first");
    VERIFY_ARE_EQUAL(CountOf(first, L"127.0.0.1"), 1u, L"the IPv4 loopback address is kept");

    for (auto const& address : first)
    {
        VERIFY_IS_TRUE(address == L"127.0.0.1" || address == L"::1", L"nothing comes back that was not given");
    }
}

void MidiNetworkAddressTests::TestSortReturnsTheTextAsGiven()
{
    // ::1 written out in full, which Windows itself would print as ::1
    auto const sorted = SortMidiNetworkAddresses({ L"0:0:0:0:0:0:0:1", L"127.0.0.1" });

    Log::Comment(String().Format(L"Sorted: %s", Join(sorted).c_str()));

    VERIFY_ARE_EQUAL(CountOf(sorted, L"127.0.0.1"), 1u);

    if (sorted.size() == 2)
    {
        VERIFY_ARE_EQUAL(CountOf(sorted, L"0:0:0:0:0:0:0:1"), 1u, L"an address comes back in the form it was given");
    }
    else
    {
        Log::Comment(L"This PC can't reach the IPv6 loopback address, so only the IPv4 one is left to compare.");
    }
}

void MidiNetworkAddressTests::TestSortLeavesOutWhatCannotBeUsed()
{
    auto const sorted = SortMidiNetworkAddresses(
        {
            L"not an address",
            L"fe80::1",             // link-local, with nothing to say which adapter reaches it
            L"fe80::1%abc",         // not an adapter index
            L"127.0.0.1",
            L"127.0.0.1",           // listed twice
            L"",
        });

    Log::Comment(String().Format(L"Sorted: %s", Join(sorted).c_str()));

    VERIFY_ARE_EQUAL(sorted.size(), static_cast<size_t>(1), L"only the one usable address is left");
    VERIFY_ARE_EQUAL(CountOf(sorted, L"127.0.0.1"), 1u, L"and it is there once");

    VERIFY_IS_TRUE(SortMidiNetworkAddresses({}).empty(), L"nothing from nothing");
}

void MidiNetworkAddressTests::TestSortKeepsAnAddressItCannotPlace()
{
    // the documentation range, which nothing routes. Alone, it is still the only one to try.
    auto const alone = SortMidiNetworkAddresses({ L"2001:db8::1" });

    VERIFY_ARE_EQUAL(alone.size(), static_cast<size_t>(1));
    CHECK_TEXT(alone.empty() ? std::wstring{} : alone[0], L"2001:db8::1");

    // when Windows can place none of several, they all come back rather than none
    auto const several = SortMidiNetworkAddresses({ L"2001:db8::2", L"2001:db8::1" });

    Log::Comment(String().Format(L"Sorted: %s", Join(several).c_str()));

    VERIFY_ARE_EQUAL(several.size(), static_cast<size_t>(2));
    VERIFY_ARE_EQUAL(CountOf(several, L"2001:db8::1"), 1u);
    VERIFY_ARE_EQUAL(CountOf(several, L"2001:db8::2"), 1u);
}

void MidiNetworkAddressTests::TestResolveFindsLocalhost()
{
    std::stop_source stop;
    auto const addresses = ResolveMidiNetworkHostName(L"localhost", 5, stop.get_token());

    Log::Comment(String().Format(L"localhost: %s", Join(addresses).c_str()));

    VERIFY_ARE_EQUAL(CountOf(addresses, L"127.0.0.1"), 1u, L"localhost includes the IPv4 loopback address, once");
}

void MidiNetworkAddressTests::TestResolveReturnsAnAddressAsItIs()
{
    std::stop_source stop;

    auto const v4 = ResolveMidiNetworkHostName(L"192.0.2.10", 5, stop.get_token());
    VERIFY_ARE_EQUAL(v4.size(), static_cast<size_t>(1));
    CHECK_TEXT(v4.empty() ? std::wstring{} : v4[0], L"192.0.2.10");

    auto const v6 = ResolveMidiNetworkHostName(L"2001:db8::5", 5, stop.get_token());
    VERIFY_ARE_EQUAL(v6.size(), static_cast<size_t>(1));
    CHECK_TEXT(v6.empty() ? std::wstring{} : v6[0], L"2001:db8::5");
}

void MidiNetworkAddressTests::TestResolveGivesUpOnceStopped()
{
    std::stop_source stop;
    stop.request_stop();

    auto const started = std::chrono::steady_clock::now();
    auto const addresses = ResolveMidiNetworkHostName(L"localhost", 5, stop.get_token());
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();

    VERIFY_IS_TRUE(addresses.empty(), L"nothing is looked up once the service is stopping");
    VERIFY_IS_LESS_THAN(elapsed, 1000ll, L"and it returns at once");
}

void MidiNetworkAddressTests::TestChooseNothingFromNoAddresses()
{
    VERIFY_IS_TRUE(ChooseMidiNetworkAddress({}, L"", 0).empty());
    VERIFY_IS_TRUE(ChooseMidiNetworkAddress({}, L"192.168.1.20", 3).empty());
}

void MidiNetworkAddressTests::TestChooseMovesOnForEachUnansweredAttempt()
{
    std::vector<std::wstring> const addresses{ L"fe80::1%14", L"192.168.1.20", L"2001:db8::20" };

    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"", 0), L"fe80::1%14");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"", 1), L"192.168.1.20");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"", 2), L"2001:db8::20");

    // every address had its turn, so it starts over
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"", 3), L"fe80::1%14");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"", 4), L"192.168.1.20");
}

void MidiNetworkAddressTests::TestChooseStartsWhereTheLastSessionOpened()
{
    std::vector<std::wstring> const addresses{ L"fe80::1%14", L"192.168.1.20", L"2001:db8::20" };

    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"192.168.1.20", 0), L"192.168.1.20");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"192.168.1.20", 1), L"fe80::1%14");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"192.168.1.20", 2), L"2001:db8::20");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"192.168.1.20", 3), L"192.168.1.20");

    // hex digits in another case are the same address
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"FE80::1%14", 0), L"fe80::1%14");
}

void MidiNetworkAddressTests::TestChooseIgnoresAConnectedAddressNoLongerListed()
{
    std::vector<std::wstring> const addresses{ L"fe80::1%14", L"192.168.1.20" };

    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"192.168.1.99", 0), L"fe80::1%14");
    CHECK_TEXT(ChooseMidiNetworkAddress(addresses, L"192.168.1.99", 1), L"192.168.1.20");
}

void MidiNetworkAddressTests::TestNextAddressIsUntriedUntilEachHadATurn()
{
    VERIFY_IS_FALSE(IsNextMidiNetworkAddressUntried(1, 0), L"no addresses");
    VERIFY_IS_FALSE(IsNextMidiNetworkAddressUntried(1, 1), L"one address, already tried");

    VERIFY_IS_TRUE(IsNextMidiNetworkAddressUntried(1, 2), L"two addresses, the second not tried yet");
    VERIFY_IS_FALSE(IsNextMidiNetworkAddressUntried(2, 2), L"two addresses, both tried");
    VERIFY_IS_TRUE(IsNextMidiNetworkAddressUntried(3, 2), L"the second turn through them");

    VERIFY_IS_TRUE(IsNextMidiNetworkAddressUntried(1, 3));
    VERIFY_IS_TRUE(IsNextMidiNetworkAddressUntried(2, 3));
    VERIFY_IS_FALSE(IsNextMidiNetworkAddressUntried(3, 3));
}
