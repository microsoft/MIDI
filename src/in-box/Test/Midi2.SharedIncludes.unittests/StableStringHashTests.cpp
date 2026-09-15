// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "StableStringHashTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // Measured from live endpoints on a development machine: each id is the parent device instance
    // id of an endpoint whose software device node carried the matching hash. Endpoint ids built
    // from these are already in customer configuration files, so a change here is a change to
    // persisted data and orphans every stored customization for the affected devices.
    struct FrozenHashCase
    {
        wchar_t const* DeviceInstanceId;
        uint64_t ExpectedHash;
    };

    constexpr FrozenHashCase FrozenHashCases[]
    {
        { L"USB\\VID_2573&PID_001A&MI_00\\9&A606804&0&0000",        2359487122041711333ULL },
        { L"USB\\VID_17CC&PID_1860&MI_00\\9&2BEE1BEB&0&0000",        488472409174937326ULL },
        { L"USB\\VID_2573&PID_008A&MI_00\\A&A27FB2A&0&0000",        5232876999726644066ULL },
        { L"USB\\VID_0499&PID_1063\\YMHFF3515D23837563143116244",  18314997113538265340ULL },
        { L"USB\\VID_2A08&PID_3090&MI_00\\9&1B27F134&0&0000",      15739441383951236956ULL },
        { L"ROOT\\MEDIA\\0000",                                     2237713434172167271ULL },
    };
}

void StableStringHashTests::TestFrozenValuesForRealDeviceInstanceIds()
{
    for (auto const& testCase : FrozenHashCases)
    {
        auto const actual = internal::StableWideStringHash(testCase.DeviceInstanceId);

        Log::Comment(String().Format(
            L"%s -> %I64u", testCase.DeviceInstanceId, actual));

        VERIFY_ARE_EQUAL(testCase.ExpectedHash, actual);
    }
}

void StableStringHashTests::TestMatchesStandardLibraryHash()
{
    // The frozen implementation started life as a copy of what the standard library produced. When
    // this fails, the standard library has changed and the freeze is the only reason customer
    // endpoint ids did not move with it. Report it, do not change the frozen values.
    wchar_t const* const cases[]
    {
        L"",
        L"A",
        L"MIDIU_KSA_",
        L"USB\\VID_0644&PID_805F&MI_02\\7&1C48DF24&0&0002",
        L"\\\\?\\usb#vid_0644&pid_805f&mi_02#7&1c48df24&0&0002#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\\global",
    };

    std::hash<std::wstring> hasher;

    for (auto const& value : cases)
    {
        std::wstring const text{ value };

        VERIFY_ARE_EQUAL(static_cast<uint64_t>(hasher(text)), internal::StableWideStringHash(text));
    }
}

void StableStringHashTests::TestEmptyString()
{
    VERIFY_ARE_EQUAL(14695981039346656037ULL, internal::StableWideStringHash(L""));
}

void StableStringHashTests::TestHashStringIsDecimal()
{
    VERIFY_ARE_EQUAL(
        std::wstring{ L"2237713434172167271" },
        internal::StableWideStringHashString(L"ROOT\\MEDIA\\0000"));
}
