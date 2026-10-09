// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Branch, Switch, Set tag, Set memory and Put value, and the file format that carries them.
class LogicStepTests : public WEX::TestClass<LogicStepTests>
{
public:

    BEGIN_TEST_CLASS(LogicStepTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ABranchSendsYesAndNoTheWayItTests);
    TEST_METHOD(ANewBranchSendsEverythingYes);
    TEST_METHOD(ANoteOffGoesWhereItsNoteOnWent);
    TEST_METHOD(APedalLetGoReachesEveryWayItWentDown);
    TEST_METHOD(TagsStayWithTheirOwnPath);
    TEST_METHOD(ATagLastsTheWholeTrip);
    TEST_METHOD(ATagKeepsWhatTheMessageWasBeforeAChange);
    TEST_METHOD(AnEmptyTagGoesTheUnreadableWay);
    TEST_METHOD(AMemoryLastsBetweenMessages);
    TEST_METHOD(AMessageSeesItsOwnChangeToAMemory);
    TEST_METHOD(ToggleAndStepWalkTheirRange);
    TEST_METHOD(PutValuePutsAMemoryIntoTheChannel);
    TEST_METHOD(PutValueCopiesMidi2ValuesWhole);
    TEST_METHOD(PutValueNeverTurnsANoteOnIntoANoteOff);
    TEST_METHOD(ValuesCompareTheSameInBothProtocols);
    TEST_METHOD(ALongMessageStaysInOnePiece);
    TEST_METHOD(ABypassedSwitchCanSendOnlyItsFirstWay);
    TEST_METHOD(ASetMemoryStepRunsWithNothingAfterIt);
    TEST_METHOD(NamesIgnoreCaseAndStopAtTheCap);
    TEST_METHOD(LogicSettingsSurviveTheFile);
    TEST_METHOD(TheFileKeepsTheWayEachLinkLeavesBy);
    TEST_METHOD(AFileFromANewerVersionIsMarked);
    TEST_METHOD(AFileWrittenTheWayTheGuideSaysReads);

    // ---- the trace ----
    TEST_METHOD(ATraceShowsTheWayEachMessageGoes);
    TEST_METHOD(ATraceCarriesMemoriesFromOneMessageToTheNext);
    TEST_METHOD(ATraceSaysWhichStepKeptAMessageOut);
    TEST_METHOD(ATraceMakesTheMessagesItSays);
};
