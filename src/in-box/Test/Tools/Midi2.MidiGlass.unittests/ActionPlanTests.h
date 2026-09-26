// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ActionPlanTests : public WEX::TestClass<ActionPlanTests>
{
public:

    BEGIN_TEST_CLASS(ActionPlanTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- system exclusive into UMP ----

    TEST_METHOD(AShortDumpIsOneCompletePacket);
    TEST_METHOD(ALongDumpIsStartContinueEnd);
    TEST_METHOD(TheWrapperBytesAreOptional);
    TEST_METHOD(ADumpWithAStatusByteInItIsRefusedWhole);
    TEST_METHOD(AnEmptyDumpSendsNothing);
    TEST_METHOD(TheGroupReachesEveryPacket);
    TEST_METHOD(TheLastPacketCarriesOnlyTheBytesItHas);

    // ---- what a control does beyond its immediate messages ----

    TEST_METHOD(AChannelVoiceMessageMakesNoPlan);
    TEST_METHOD(ASystemExclusiveMessageMakesAPlan);
    TEST_METHOD(APlanIsFoundByItsOwnTrigger);
    TEST_METHOD(ARawMessageTravelsThroughUntouched);
    TEST_METHOD(AMessageNamingAMissingDeviceKeepsItsPlace);

    // ---- sequences ----

    TEST_METHOD(ASequenceBecomesItsSteps);
    TEST_METHOD(AWaitBecomesAWaitAction);
    TEST_METHOD(ARepeatBlockIsFlattened);
    TEST_METHOD(ASequenceNamingItselfTerminates);
    TEST_METHOD(ASequenceThatIsNotThereIsSkipped);
    TEST_METHOD(APlanCannotGrowWithoutLimit);
    TEST_METHOD(AStepCanMoveAnotherControl);
    TEST_METHOD(ANoteInASequenceIsBuiltIntoWords);
    TEST_METHOD(ANoteInASequenceAlwaysHasItsNoteOff);
    TEST_METHOD(AControlChangeInASequenceIsBuiltIntoWords);
    TEST_METHOD(ARepeatedNoteIsPlayedEveryTime);
    TEST_METHOD(AStepKindThisBuildDoesNotKnowSendsNothing);
};
