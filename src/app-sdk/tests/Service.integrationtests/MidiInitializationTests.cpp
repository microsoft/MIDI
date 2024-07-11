// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

void MidiInitializationTests::TestEnsureServiceAvailable()
{
    LOG_OUTPUT(L"TestEnsureServiceAvailable **********************************************************************");

    VERIFY_IS_TRUE(MidiServicesInitializer::EnsureServiceAvailable());
}
