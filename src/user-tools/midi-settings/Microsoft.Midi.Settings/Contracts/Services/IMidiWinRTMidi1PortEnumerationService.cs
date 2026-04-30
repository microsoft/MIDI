// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.ViewModels;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiWinRTMidi1PortEnumerationService
{
    void Start();

    IList<DeviceInformation> GetAllSourcePorts();
    IList<DeviceInformation> GetAllDestinationPorts();

    IList<DeviceInformation> GetBleSourcePorts();
    IList<DeviceInformation> GetBleDestinationPorts();

    DeviceInformation GetById(string winrt1PortDeviceId);


    //event EventHandler<DeviceInformation> SourcePortAdded;
    //event EventHandler<DeviceInformationUpdate> SourcePortUpdated;
    //event EventHandler<DeviceInformationUpdate> SourcePortRemoved;

    //event EventHandler<DeviceInformation> DestinationPortAdded;
    //event EventHandler<DeviceInformationUpdate> DestinationPortUpdated;
    //event EventHandler<DeviceInformationUpdate> DestinationPortRemoved;
}