// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.ViewModels;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi;

namespace Microsoft.Midi.Settings.Contracts.Services;

public class MidiWinRTMidi1PortEnumerationService : IMidiWinRTMidi1PortEnumerationService
{
    private const string BluetoothIdIndicator = ".ble10";

    public void Start()
    {
        throw new NotImplementedException("MIDI 1 WinRT device watcher not yet implemented.");
    }

    public IList<DeviceInformation> GetAllSourcePorts()
    {
        var devices = DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector()).GetAwaiter().GetResult();   

        if (devices == null || devices.Count == 0)
        {
            return new List<DeviceInformation>();
        }

        var result = devices.ToList();

        result.Sort((x,y) => x.Name.CompareTo(y.Name, StringComparison.InvariantCultureIgnoreCase));

        return result;
    }


    public IList<DeviceInformation> GetAllDestinationPorts()
    {
        var devices = DeviceInformation.FindAllAsync(MidiOutPort.GetDeviceSelector()).GetAwaiter().GetResult();

        if (devices == null || devices.Count == 0)
        {
            return new List<DeviceInformation>();
        }

        var result = devices.ToList();

        result.Sort((x, y) => x.Name.CompareTo(y.Name, StringComparison.InvariantCultureIgnoreCase));

        return result;
    }


    public IList<DeviceInformation> GetBleSourcePorts()
    {
        var ports = GetAllSourcePorts();

        // filter to just BLE

        var result = new List<DeviceInformation>();

        foreach (var port in ports)
        {
            if (port.Id.Contains(BluetoothIdIndicator, StringComparison.InvariantCultureIgnoreCase))
            {
                result.Add(port);
            }
        }

        return result;
    }


    public IList<DeviceInformation> GetBleDestinationPorts()
    {
        var ports = GetAllDestinationPorts();

        // filter to just BLE

        var result = new List<DeviceInformation>();

        foreach (var port in ports)
        {
            if (port.Id.Contains(BluetoothIdIndicator, StringComparison.InvariantCultureIgnoreCase))
            {
                result.Add(port);
            }
        }

        return result;
    }



    public DeviceInformation GetById(string winrt1PortDeviceId)
    {
        return DeviceInformation.CreateFromIdAsync(winrt1PortDeviceId).GetAwaiter().GetResult();
    }


    //event EventHandler<DeviceInformation> SourcePortAdded;
    //event EventHandler<DeviceInformationUpdate> SourcePortUpdated;
    //event EventHandler<DeviceInformationUpdate> SourcePortRemoved;

    //event EventHandler<DeviceInformation> DestinationPortAdded;
    //event EventHandler<DeviceInformationUpdate> DestinationPortUpdated;
    //event EventHandler<DeviceInformationUpdate> DestinationPortRemoved;
}