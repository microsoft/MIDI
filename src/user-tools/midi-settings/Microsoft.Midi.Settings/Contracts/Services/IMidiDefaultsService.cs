// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiDefaultsService
{
    string GetDefaultMidiConfigName();
    string GetDefaultMidiConfigFileName();

    MidiLoopbackEndpointCreationConfig GetDefaultLoopbackCreationConfig();
    MidiBasicLoopbackEndpointCreationConfig GetDefaultBasicLoopbackCreationConfig();

    bool DoesDefaultLoopbackAlreadyExist();

    bool DoesDefaultBasicLoopbackAlreadyExist();

}
