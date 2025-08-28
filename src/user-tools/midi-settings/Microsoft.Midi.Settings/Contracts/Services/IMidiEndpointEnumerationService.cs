// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.ViewModels;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiEndpointEnumerationService
{
    void Start();

    IList<MidiEndpointWrapper> GetEndpoints();
    IList<MidiEndpointWrapper> GetEndpointsForTransportCode(string transportCode);
    IList<MidiEndpointWrapper> GetEndpointsForTransportId(Guid transportId);
    IList<MidiEndpointWrapper> GetEndpointsForPurpose(MidiEndpointDevicePurpose purpose);

    MidiEndpointWrapper GetEndpointById(string endpointDeviceId);


    event EventHandler<MidiEndpointDeviceInformation> EndpointUpdated;
    event EventHandler<MidiEndpointDeviceInformation> EndpointAdded;
    event EventHandler<MidiEndpointDeviceInformation> EndpointRemoved;

}
