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

    // ---- GBK, and telling it apart from Shift-JIS ----
    TEST_METHOD(DecodesAGbkTitleOfKatakana);
    TEST_METHOD(DecodesAGbkTitleThatIsAlsoValidShiftJis);
    TEST_METHOD(DecodesAGbkTitleThatShiftJisReadsAsHalfWidthKatakana);
    TEST_METHOD(KeepsAShiftJisFileThatIsAlsoValidGbk);
    TEST_METHOD(KeepsAShortLatin1PairThatIsAlsoValidGbk);
    TEST_METHOD(LetsAJapaneseTagWinOverGbk);
    TEST_METHOD(KeepsHalfWidthKatakanaWithVoicingMarksAsShiftJis);
    TEST_METHOD(ReadsAKoreanCopyrightSign);

    // ---- Big5 and Korean ----
    TEST_METHOD(DecodesKoreanNames);
    TEST_METHOD(DecodesBig5ThatGbkReadsAsKana);
    TEST_METHOD(DecodesBig5WithAsciiTrailBytes);
    TEST_METHOD(DecodesBig5ThatIsNotValidGbk);
    TEST_METHOD(KeepsJapaneseInGbkOutOfBig5);
    TEST_METHOD(KeepsAShortHalfWidthKatakanaTitleAsShiftJis);
    TEST_METHOD(KeepsLatin1SymbolsBeforeLettersOutOfBig5);

    // ---- RP-026 song information and the encoding record ----
    TEST_METHOD(ReadsSongInformationTags);
    TEST_METHOD(EndsASongInformationItemAtTheNextItem);
    TEST_METHOD(ReadsSongInformationWithABraceInsideACharacter);
    TEST_METHOD(RecordsTheEncodingAndHowItWasChosen);
    TEST_METHOD(WritesSongInformationBack);

    // ---- real files from a customer ----
    TEST_METHOD(ReadsAMinimalShiftJisFile);
    TEST_METHOD(ReadsShiftJisNamesFromTheLastTrack);

    // ---- generated samples with a title and a plain text event ----
    TEST_METHOD(ReadsKoreanWithHanja);
    TEST_METHOD(ReadsBig5WhenOnlyTheTextEventRulesOutKorean);
    TEST_METHOD(ReadsGbkThatIsAlsoValidKoreanAndBig5);
};
