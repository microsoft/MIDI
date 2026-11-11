// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <winrt/Windows.Foundation.Collections.h>
using namespace winrt::Windows::Foundation::Collections;

const uint64_t MAX_SINGLE_PING_ROUND_TRIP_TICKS = 3000; // typically much less than this.

void MidiPingTests::TestPing()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    uint8_t pingCount = 100;
    
    std::cout << "Pinging service..." << std::endl;

    auto summary = MidiDiagnostics::PingService(pingCount);

    VERIFY_IS_TRUE(summary.Success());
    VERIFY_ARE_EQUAL(summary.Responses().Size(), pingCount);

    for (auto response : summary.Responses())
    {
        std::cout 
            << response.SourceId() 
            << " " << response.Index() 
            << " " << response.ClientSendMidiTimestamp() 
            << " " << response.ClientReceiveMidiTimestamp()
            << " " << response.ClientDeltaTimestamp()
            << std::endl;

        VERIFY_IS_TRUE(response.ClientReceiveMidiTimestamp() > response.ClientSendMidiTimestamp());
        VERIFY_IS_TRUE(response.ClientDeltaTimestamp() > 0);
        VERIFY_IS_TRUE(response.SourceId() > 0);
    }

    std::cout << "Total Ping Round Trip Ticks   : " << summary.TotalPingRoundTripMidiClock() << std::endl;
    std::cout << "Average Ping Round Trip Ticks : " << summary.AveragePingRoundTripMidiClock() << std::endl;

    VERIFY_IS_TRUE(summary.TotalPingRoundTripMidiClock() < MAX_SINGLE_PING_ROUND_TRIP_TICKS * pingCount);
    VERIFY_IS_TRUE(summary.AveragePingRoundTripMidiClock() < MAX_SINGLE_PING_ROUND_TRIP_TICKS);
}