// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiDefs.h"
#include "Utf8StringTests.h"

using namespace WindowsMidiServicesInternal;

// The specification expresses its name limits in UTF-8 bytes, so these tests are all about the
// difference between a character count and a byte count, and about never cutting a character
// in half.

// U+00E9 LATIN SMALL LETTER E WITH ACUTE - 2 UTF-8 bytes
#define TEST_CHAR_2_BYTE L"\u00E9"

// U+20AC EURO SIGN - 3 UTF-8 bytes
#define TEST_CHAR_3_BYTE L"\u20AC"

// U+1F3B9 MUSICAL KEYBOARD - 4 UTF-8 bytes, and a surrogate pair in UTF-16
#define TEST_CHAR_4_BYTE L"\U0001F3B9"


void Utf8StringTests::TestByteCountAscii()
{
    VERIFY_ARE_EQUAL(Utf8ByteCount(L""), (size_t)0);
    VERIFY_ARE_EQUAL(Utf8ByteCount(L"A"), (size_t)1);
    VERIFY_ARE_EQUAL(Utf8ByteCount(L"Hello"), (size_t)5);
}

void Utf8StringTests::TestByteCountMultiByte()
{
    // one wchar_t, two bytes
    VERIFY_ARE_EQUAL(Utf8ByteCount(TEST_CHAR_2_BYTE), (size_t)2);

    // one wchar_t, three bytes
    VERIFY_ARE_EQUAL(Utf8ByteCount(TEST_CHAR_3_BYTE), (size_t)3);

    // two wchar_t (surrogate pair), four bytes
    VERIFY_ARE_EQUAL(std::wstring(TEST_CHAR_4_BYTE).length(), (size_t)2);
    VERIFY_ARE_EQUAL(Utf8ByteCount(TEST_CHAR_4_BYTE), (size_t)4);

    // the whole point: length() is not the byte count
    std::wstring mixed = L"A" TEST_CHAR_3_BYTE L"B";
    VERIFY_ARE_EQUAL(mixed.length(), (size_t)3);
    VERIFY_ARE_EQUAL(Utf8ByteCount(mixed), (size_t)5);

    VERIFY_IS_TRUE(ExceedsUtf8ByteCount(mixed, 4));
    VERIFY_IS_FALSE(ExceedsUtf8ByteCount(mixed, 5));
}

void Utf8StringTests::TestTruncateNotNeeded()
{
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"Hello", 5), std::wstring(L"Hello"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"Hello", 98), std::wstring(L"Hello"));
}

void Utf8StringTests::TestTruncateAscii()
{
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"Hello", 3), std::wstring(L"Hel"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"Hello", 1), std::wstring(L"H"));
}

void Utf8StringTests::TestTruncateOnExactCharacterBoundary()
{
    // This is the case the old network writer implementation got wrong: when the limit lands
    // exactly on the end of a multi-byte character, that character must be kept.
    std::wstring source = L"A" TEST_CHAR_3_BYTE TEST_CHAR_3_BYTE;

    // "A" plus one euro sign is exactly 4 bytes
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 4), std::wstring(L"A" TEST_CHAR_3_BYTE));

    // two-byte character ending exactly on the limit
    std::wstring source2 = TEST_CHAR_2_BYTE TEST_CHAR_2_BYTE;
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source2, 2), std::wstring(TEST_CHAR_2_BYTE));
}

void Utf8StringTests::TestTruncateSplitsTwoByteCharacter()
{
    std::wstring source = L"A" TEST_CHAR_2_BYTE L"B";

    // limit of 2 lands in the middle of the 2 byte character, so it must be dropped entirely
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 2), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 3), std::wstring(L"A" TEST_CHAR_2_BYTE));
}

void Utf8StringTests::TestTruncateSplitsThreeByteCharacter()
{
    std::wstring source = L"A" TEST_CHAR_3_BYTE L"B";

    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 2), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 3), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 4), std::wstring(L"A" TEST_CHAR_3_BYTE));
}

void Utf8StringTests::TestTruncateSplitsSurrogatePair()
{
    // A surrogate pair must never be split, or the result is not valid UTF-16 either
    std::wstring source = L"A" TEST_CHAR_4_BYTE;

    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 2), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 3), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 4), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(source, 5), std::wstring(L"A" TEST_CHAR_4_BYTE));
}

void Utf8StringTests::TestTruncateToZero()
{
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"Hello", 0), std::wstring(L""));
}

void Utf8StringTests::TestTruncateEmptyString()
{
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"", 98), std::wstring(L""));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(L"", 0), std::wstring(L""));
}

void Utf8StringTests::TestTruncateSingleCharacterTooLarge()
{
    // nothing at all fits, so the result is empty rather than a broken character
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(TEST_CHAR_4_BYTE, 3), std::wstring(L""));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(TEST_CHAR_3_BYTE, 2), std::wstring(L""));
    VERIFY_ARE_EQUAL(TruncateToUtf8ByteCount(TEST_CHAR_2_BYTE, 1), std::wstring(L""));
}

void Utf8StringTests::TestTruncatedResultAlwaysWithinLimit()
{
    // Sweep every limit against a string of mixed character widths. Whatever comes back must
    // always encode to no more than the limit, and must always round-trip cleanly.
    std::wstring source =
        L"Contoso " TEST_CHAR_2_BYTE TEST_CHAR_3_BYTE TEST_CHAR_4_BYTE
        L" Synth " TEST_CHAR_4_BYTE TEST_CHAR_3_BYTE TEST_CHAR_2_BYTE L" 2000";

    for (size_t limit = 0; limit <= Utf8ByteCount(source) + 4; limit++)
    {
        auto truncated = TruncateToUtf8ByteCount(source, limit);

        VERIFY_IS_FALSE(ExceedsUtf8ByteCount(truncated, limit));

        // must be a prefix of the original, never mangled
        VERIFY_ARE_EQUAL(source.compare(0, truncated.length(), truncated), 0);

        // and the UTF-8 must still be well formed, meaning it converts back to the same thing
        VERIFY_ARE_EQUAL(WStringFromUtf8(Utf8FromWString(truncated)), truncated);
    }
}


// TruncateToCharacterCount bounds a wide string by wchar_t count, which is what the service's
// RPC entry points measure with wcsnlen_s. The UTF-8 helpers above are for the spec's byte
// limits and are deliberately not interchangeable with it.

void Utf8StringTests::TestTruncateToCharacterCountNotNeeded()
{
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"Hello", 5), std::wstring(L"Hello"));
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"Hello", 99), std::wstring(L"Hello"));
}

void Utf8StringTests::TestTruncateToCharacterCountAscii()
{
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"Hello", 4), std::wstring(L"Hell"));
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"Hello", 1), std::wstring(L"H"));
}

void Utf8StringTests::TestTruncateToCharacterCountIgnoresUtf8ByteCount()
{
    // Three wide characters, but six UTF-8 bytes. Counting bytes here would cut it short, which
    // is the mistake this helper exists to prevent.
    std::wstring source = TEST_CHAR_2_BYTE TEST_CHAR_3_BYTE L"A";

    VERIFY_ARE_EQUAL(source.length(), (size_t)3);
    VERIFY_ARE_EQUAL(Utf8ByteCount(source), (size_t)6);
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(source, 3), source);
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(source, 2), std::wstring(TEST_CHAR_2_BYTE TEST_CHAR_3_BYTE));
}

void Utf8StringTests::TestTruncateToCharacterCountSplitsSurrogatePair()
{
    // The pair occupies two wchar_t, so a limit of 2 lands between its halves. Keeping the lone
    // lead would leave an invalid string, so it goes too.
    std::wstring source = L"A" TEST_CHAR_4_BYTE;

    VERIFY_ARE_EQUAL(source.length(), (size_t)3);
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(source, 3), source);
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(source, 2), std::wstring(L"A"));
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(source, 1), std::wstring(L"A"));
}

void Utf8StringTests::TestTruncateToCharacterCountEdgeCases()
{
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"Hello", 0), std::wstring(L""));
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"", 5), std::wstring(L""));
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(L"", 0), std::wstring(L""));

    // a pair on its own, cut below its width, leaves nothing rather than half a character
    VERIFY_ARE_EQUAL(TruncateToCharacterCount(TEST_CHAR_4_BYTE, 1), std::wstring(L""));
}

void Utf8StringTests::TestTruncateToCharacterCountMatchesServiceLimit()
{
    // Whatever goes in, the result has to satisfy the same test the service applies before it
    // will accept a session name.
    std::wstring source =
        L"Contoso " TEST_CHAR_2_BYTE TEST_CHAR_3_BYTE TEST_CHAR_4_BYTE
        L" Session " TEST_CHAR_4_BYTE TEST_CHAR_3_BYTE TEST_CHAR_2_BYTE L" 2000";

    while (source.length() <= MAXIMUM_SESSION_NAME_CHARACTER_COUNT + 4)
    {
        source += TEST_CHAR_4_BYTE;
    }

    for (size_t limit = 0; limit <= source.length(); limit++)
    {
        auto truncated = TruncateToCharacterCount(source, limit);

        VERIFY_IS_LESS_THAN_OR_EQUAL(
            wcsnlen_s(truncated.c_str(), limit + 1), limit);

        // must be a prefix of the original, never mangled
        VERIFY_ARE_EQUAL(source.compare(0, truncated.length(), truncated), 0);

        // and never ends on an orphaned lead surrogate
        if (!truncated.empty())
        {
            VERIFY_IS_FALSE(IS_HIGH_SURROGATE(truncated.back()));
        }
    }
}
