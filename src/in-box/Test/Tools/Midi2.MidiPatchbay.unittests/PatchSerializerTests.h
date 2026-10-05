// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class PatchSerializerTests : public WEX::TestClass<PatchSerializerTests>
{
public:

    BEGIN_TEST_CLASS(PatchSerializerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(APatchSurvivesTheFile);
    TEST_METHOD(OnlyEndpointEndsHaveAGroup);
    TEST_METHOD(AnEarlierFileBecomesBlocks);
    TEST_METHOD(APlainEarlierConnectionStaysPlain);
    TEST_METHOD(ConvertedConnectionsDoExactlyWhatTheyDid);
    TEST_METHOD(ANoteMapNextToATransposeKeepsItsTargets);
    TEST_METHOD(ANoteMapEntryThatCannotBeKeptIsReported);
    TEST_METHOD(AMutedConnectionStaysMuted);
    TEST_METHOD(LinksToMissingThingsAreLeftOut);
    TEST_METHOD(AnUnknownBlockIsLeftOutWithItsLinks);
    TEST_METHOD(TextThatIsNotAPatchIsRejected);
    TEST_METHOD(RemovingABlockRemovesItsLinks);
};
