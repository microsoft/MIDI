// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


void MidiPingTests::TestPing()
{
//    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    uint8_t pingCount = 100;
    
    std::cout << "Pinging service..." << std::endl;

    auto summary = MidiDiagnostics::PingService(pingCount);

    VERIFY_IS_TRUE(summary.Success());
    VERIFY_ARE_EQUAL(summary.Responses().Size(), pingCount);

}