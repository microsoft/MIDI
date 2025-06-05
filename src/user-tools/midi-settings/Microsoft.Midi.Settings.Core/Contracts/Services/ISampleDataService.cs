// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Core.Models;

namespace Microsoft.Midi.Settings.Core.Contracts.Services;

// Remove this class once your pages/features are using your data.
public interface ISampleDataService
{
    Task<IEnumerable<MidiMessageViewModel>> GetUmpMonitorDataAsync();
    Task<IEnumerable<string>> GetUmpEndpointNamesAsync();
    Task<IEnumerable<DisplayFriendlyMidiDevice>> GetDevicesAsync();


}
