// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "Midi2ClientTests.h"

void Midi2ClientTests::TestMidiClient()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
}

bool Midi2ClientTests::ClassSetup()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    
    return true;
}

bool Midi2ClientTests::ClassCleanup()
{
    return true;
}

