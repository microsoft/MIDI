// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include "Midi2TransportTestsBase.h"
#include "Midi2KSAggregateTransportTests.h"

void MidiKSAggregateTransportTests::TestMidiKSAggregateTransportBidi_UMP()
{
    TestMidiTransportBidi(__uuidof(Midi2KSAggregateTransport), MidiDataFormats_UMP);
}

void MidiKSAggregateTransportTests::TestMidiKSAggregateIO_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2KSAggregateTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_None);
}

void MidiKSAggregateTransportTests::TestMidiKSAggregateIOSlowMessages_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2KSAggregateTransport), MidiDataFormats_UMP, TRUE, MessageOptionFlags_None);
}

void MidiKSAggregateTransportTests::TestMidiKSAggregateIO_Latency_UMP_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2KSAggregateTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_WaitForSendComplete);
}

bool MidiKSAggregateTransportTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool MidiKSAggregateTransportTests::TestCleanup()
{
    return true;
}

bool MidiKSAggregateTransportTests::ClassSetup()
{
    PrintStagingStates();

    // Midi discovery conflicts with usage of the KS transport outside of the service, but we
    // require midisrv to be running for endpoint enumeration.
    // Disable discovery and restart the service so we can ensure the tests will all
    // run.
    StopMIDIService();
    SetMidiDiscovery(false);
    StartMIDIService();

    return true;
}

bool MidiKSAggregateTransportTests::ClassCleanup()
{
    // Stop the service and restore midi discovery.
    StopMIDIService();
    SetMidiDiscovery(true);

    return true;
}

