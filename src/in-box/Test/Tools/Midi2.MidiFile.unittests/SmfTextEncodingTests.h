// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Text in a Standard MIDI File has no declared encoding. The reader picks one per file: UTF-8,
// then Shift-JIS, then Latin-1.
class SmfTextEncodingTests : public WEX::TestClass<SmfTextEncodingTests>
{
public:

    BEGIN_TEST_CLASS(SmfTextEncodingTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(DecodesShiftJisNamesAndCopyright);
    TEST_METHOD(DecodesShiftJisKaraokeLyrics);
    TEST_METHOD(DecodesShiftJisNextToAsciiText);
    TEST_METHOD(DecodesHalfWidthKatakanaWhenTheRestOfTheFileIsShiftJis);
    TEST_METHOD(KeepsUtf8JapaneseAsUtf8);
    TEST_METHOD(KeepsLatin1Text);
    TEST_METHOD(KeepsLatin1TextThatIsAlsoValidShiftJis);
    TEST_METHOD(KeepsShortCapitalAccentedLatin1AsLatin1);
    TEST_METHOD(KeepsLatin1WhenAnyTextIsNotValidShiftJis);
    TEST_METHOD(KeepsDosAccentedLettersOutOfShiftJis);
    TEST_METHOD(KeepsCurlyQuotesOutOfShiftJis);
    TEST_METHOD(DropsAShiftJisCharacterCutByTheTextLimit);
    TEST_METHOD(DropsAUtf8CharacterCutByTheTextLimit);

    // ---- RP-026 code set tags and byte order marks ----
    TEST_METHOD(ReadsLyricsAfterAJapaneseCodeSetTag);
    TEST_METHOD(ReadsATagAndTextInTheSameLyric);
    TEST_METHOD(AcceptsTheCodeSetNameInAnyCase);
    TEST_METHOD(SwitchesCodeSetsPartWayThrough);
    TEST_METHOD(ReadsLatinTaggedLyricsAsWindows1252);
    TEST_METHOD(UsesAJapaneseTagForNamesWithoutOtherEvidence);
    TEST_METHOD(UsesALatinTagForNamesThatLookLikeShiftJis);
    TEST_METHOD(KeepsUtf8NamesInATaggedFile);
    TEST_METHOD(DropsNonAsciiLyricsInAnUndefinedCodeSet);
    TEST_METHOD(LeavesSomethingThatOnlyLooksLikeATagAlone);
    TEST_METHOD(ReadsTextThatStartsWithAByteOrderMark);
};
