// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The synth list file. The app won't start on a list it can't use as it is, because starting with
// no synths would save an empty list over the customer's own, so every problem has to be told
// apart from a list that is fine.

#include "pch.h"

#include <format>

#include "SynthList.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisoundmodule;

namespace
{
    constexpr char const* FirstId = "{11111111-1111-1111-1111-111111111111}";
    constexpr char const* SecondId = "{22222222-2222-2222-2222-222222222222}";

    // A backslash in JSON is written twice.
    constexpr char const* Bank = R"(C:\\Banks\\Piano.sf2)";

    // One synth, the way the app writes it.
    std::string Synth(_In_ std::string_view id, _In_ std::string_view soundFont, _In_ std::string_view productInstanceId = "SF2SYNTH-0000000A")
    {
        return std::format(
            R"({{"id":"{}","name":"Piano","soundFont":"{}","productInstanceId":"{}","volumeDb":-3,"enabled":false}})",
            id, soundFont, productInstanceId);
    }

    std::string List(_In_ std::vector<std::string> const& synths, _In_ std::string_view version = "1")
    {
        std::string text = std::format(R"({{"_comment":"Test","version":{},"synths":[)", version);

        for (size_t i = 0; i < synths.size(); i++)
        {
            text += (i == 0 ? "" : ",") + synths[i];
        }

        return text + "]}";
    }

    std::string NumberedId(_In_ size_t number)
    {
        return std::format("{{{:08X}-0000-0000-0000-000000000000}}", number);
    }
}

class SynthListTests
{
public:
    BEGIN_TEST_CLASS(SynthListTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ReadsWhatTheAppWrites)
    {
        auto const list = ParseSynthList(List({ Synth(FirstId, Bank, "SF2SYNTH-0000000A"), Synth(SecondId, Bank, "SF2SYNTH-0000000B") }));

        VERIFY_IS_TRUE(list.Problem == SynthListProblem::None);
        VERIFY_ARE_EQUAL(size_t{ 2 }, list.Synths.size());

        auto const& first = list.Synths[0];

        VERIFY_IS_TRUE(first.Id == L"{11111111-1111-1111-1111-111111111111}");
        VERIFY_IS_TRUE(first.Name == L"Piano");
        VERIFY_IS_TRUE(first.SoundFontPath == LR"(C:\Banks\Piano.sf2)");
        VERIFY_IS_TRUE(first.ProductInstanceId == "SF2SYNTH-0000000A");
        VERIFY_ARE_EQUAL(-3.0, first.VolumeDb);
        VERIFY_IS_FALSE(first.Enabled);

        VERIFY_IS_TRUE(list.Synths[1].Id == L"{22222222-2222-2222-2222-222222222222}");

        // A file without a version is from before the app wrote one, and an empty list is a list.
        VERIFY_IS_TRUE(ParseSynthList(R"({"synths":[]})").Problem == SynthListProblem::None);
    }

    // Anything that would drop a synth, or the whole list, is refused rather than read around.
    TEST_METHOD(RefusesWhatItCannotUse)
    {
        struct Case
        {
            std::string Text;
            SynthListProblem Problem;
            size_t Entry;
            wchar_t const* Name;
        };

        Case const cases[]
        {
            { "not json", SynthListProblem::NotJson, 0, L"text that isn't JSON" },
            { "[1,2]", SynthListProblem::NotJson, 0, L"JSON that isn't an object" },
            { "{}", SynthListProblem::NoSynthList, 0, L"an object with no list" },
            { R"({"synths":5})", SynthListProblem::NoSynthList, 0, L"a list that isn't a list" },
            { R"({"version":"one","synths":[]})", SynthListProblem::NoSynthList, 0, L"a version that isn't a number" },
            { List({}, "2"), SynthListProblem::NewerVersion, 0, L"a newer app's file" },
            { List({ Synth(FirstId, Bank), "7" }), SynthListProblem::EntryNotASynth, 2, L"a synth that isn't an object" },
            { List({ Synth("not an id", Bank) }), SynthListProblem::EntryId, 1, L"a damaged id" },
            { List({ Synth("", Bank) }), SynthListProblem::EntryId, 1, L"no id" },
            { List({ Synth(FirstId, "Piano.sf2") }), SynthListProblem::EntrySoundBank, 1, L"a relative sound bank path" },
            { List({ Synth(FirstId, "") }), SynthListProblem::EntrySoundBank, 1, L"no sound bank" },
            { List({ Synth(FirstId, Bank), Synth(FirstId, Bank, "SF2SYNTH-0000000B") }), SynthListProblem::EntryDuplicateId, 2, L"the same id twice" },
        };

        for (auto const& test : cases)
        {
            auto const list = ParseSynthList(test.Text);

            Log::Comment(String().Format(L"%s: problem %d, synth %zu", test.Name, static_cast<int>(list.Problem), list.Entry));

            VERIFY_IS_TRUE(list.Problem == test.Problem, test.Name);
            VERIFY_ARE_EQUAL(test.Entry, list.Entry, test.Name);
            VERIFY_IS_TRUE(list.Synths.empty(), L"and nothing comes back to start with");
        }
    }

    TEST_METHOD(RefusesMoreSynthsThanItCanRun)
    {
        std::vector<std::string> synths;

        for (size_t i = 1; i <= MaximumSynths; i++)
        {
            synths.push_back(Synth(NumberedId(i), Bank, std::format("SF2SYNTH-{:08X}", i)));
        }

        VERIFY_IS_TRUE(ParseSynthList(List(synths)).Problem == SynthListProblem::None, L"as many as it can run");

        synths.push_back(Synth(NumberedId(MaximumSynths + 1), Bank, "SF2SYNTH-FFFFFFFF"));

        VERIFY_IS_TRUE(ParseSynthList(List(synths)).Problem == SynthListProblem::TooManySynths, L"one more");
    }

    // A name, an identity or a volume can be put right without losing the synth, so these are read.
    TEST_METHOD(TidiesWhatItCanWithoutLosingASynth)
    {
        auto const text = std::format(R"({{"synths":[)"
            R"({{"id":"{}","name":"  Grand\u0007 Piano  ","soundFont":"{}","productInstanceId":"SF2SYNTH-0000000A","volumeDb":99}},)"
            R"({{"id":"{}","soundFont":"{}","productInstanceId":"SF2SYNTH-0000000A","volumeDb":"loud"}},)"
            R"({{"id":"{}","name":"A Very Long Name For A Single Synth Endpoint","soundFont":"{}","productInstanceId":"caf\u00e9"}}]}})",
            FirstId, Bank, SecondId, Bank, NumberedId(3), Bank);

        auto const list = ParseSynthList(text);

        VERIFY_IS_TRUE(list.Problem == SynthListProblem::None);
        VERIFY_ARE_EQUAL(size_t{ 3 }, list.Synths.size());

        VERIFY_IS_TRUE(list.Synths[0].Name == L"Grand Piano", L"control characters and outer spaces go");
        VERIFY_ARE_EQUAL(12.0, list.Synths[0].VolumeDb, L"a volume past the top comes back at the top");
        VERIFY_IS_TRUE(list.Synths[0].Enabled, L"a synth is on unless the file says otherwise");

        VERIFY_IS_TRUE(list.Synths[1].Name == L"Piano.sf2", L"no name is the sound bank's file name");
        VERIFY_IS_TRUE(list.Synths[1].ProductInstanceId.empty(), L"a copied identity is left for the caller to replace");
        VERIFY_ARE_EQUAL(0.0, list.Synths[1].VolumeDb);

        VERIFY_IS_TRUE(list.Synths[2].Name == L"A Very Long Name For A Single S", L"a long name is cut to what an endpoint takes");
        VERIFY_ARE_EQUAL(MaximumSynthNameLength, list.Synths[2].Name.size());
        VERIFY_IS_TRUE(list.Synths[2].ProductInstanceId.empty(), L"an identity that isn't printable ASCII is replaced");
    }
};
