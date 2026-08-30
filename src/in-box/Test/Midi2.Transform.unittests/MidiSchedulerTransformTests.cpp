// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "Midi2SchedulerTransform.h"
#include "Midi2MidiSrvTransport.h"

#include "MidiSchedulerTransformTests.h"

#include <initguid.h>
#include "MidiDefs.h"

bool MidiSchedulerTransformTests::ClassSetup()
{
    PrintStagingStates();

    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    return true;
}

