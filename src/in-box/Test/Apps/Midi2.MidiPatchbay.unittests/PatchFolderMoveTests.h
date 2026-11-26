// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class PatchFolderMoveTests : public WEX::TestClass<PatchFolderMoveTests>
{
public:

    BEGIN_TEST_CLASS(PatchFolderMoveTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AnEarlierFolderMovesWhole);
    TEST_METHOD(BothFoldersMergeAndNothingIsReplaced);
    TEST_METHOD(AnEmptiedEarlierFolderIsRemoved);
    TEST_METHOD(NothingHappensWithoutAnEarlierFolder);
    TEST_METHOD(AFolderInUseStaysUntilItCanMove);
    TEST_METHOD(AFileWithTheNewNameKeepsTheEarlierFolder);
    TEST_METHOD(ALinkedFolderIsNeverEmptied);
};
