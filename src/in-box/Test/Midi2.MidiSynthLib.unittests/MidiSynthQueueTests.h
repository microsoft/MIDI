// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The ring buffer that carries UMP between the service, the render thread and the worker.
class MidiSynthQueueTests : public WEX::TestClass<MidiSynthQueueTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthQueueTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestPushAllIsAllOrNothing);
    TEST_METHOD(TestPushAllNeverShowsPartOfAMessage);
};
