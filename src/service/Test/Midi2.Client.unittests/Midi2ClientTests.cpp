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
    VERIFY_SUCCEEDED(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED));
    
    return true;
}

bool Midi2ClientTests::ClassCleanup()
{
    Windows::Foundation::Uninitialize();
    return true;
}

