// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiPanicService
{
    bool SendMidiPanic(string endpointDeviceId);

    bool SendMidiPanic(string endpointDeviceId, IList<MidiGroup> groups);

    bool SendMidiPanic(string endpointDeviceId, IList<MidiGroup> groups, IList<MidiChannel> channels);



    bool SendMidiPanic(MidiEndpointConnection connection);

    bool SendMidiPanic(MidiEndpointConnection connection, IList<MidiGroup> groups);

    bool SendMidiPanic(MidiEndpointConnection connection, IList<MidiGroup> groups, IList<MidiChannel> channels);


}
