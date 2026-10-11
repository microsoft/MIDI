// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The remote control map: what incoming messages do, and what goes back to light a surface.
class RemoteControlTests : public WEX::TestClass<RemoteControlTests>
{
public:

    BEGIN_TEST_CLASS(RemoteControlTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(NotesLaunchClipsAndScenes);
    TEST_METHOD(ControllersSetMuteSoloAndArm);
    TEST_METHOD(TheTransportIsOnChannel16);
    TEST_METHOD(Midi2MessagesWorkToo);
    TEST_METHOD(NoteOffsAndOtherMessagesDoNothing);
    TEST_METHOD(TracksAreCountedWithoutFolders);
    TEST_METHOD(FeedbackSendsOnlyWhatChanged);
    TEST_METHOD(TheGlassLayoutFollowsTheLayoutRules);
    TEST_METHOD(ABigSequenceSpreadsOverPages);
};
