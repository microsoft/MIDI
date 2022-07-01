using System;
using System.Collections.Generic;
using System.Text;

namespace MidiSettings.Core.Models;

public class SampleMidiDevice
{
    public string Id
    {
        get; set;
    }

    public string Name
    {
        get;set;
    }

    public string IconPath
    {
        get; set;
    }

    public string DeviceSuppliedDisplayName
    {
        get; set;
    }

    public string DriverDescription
    {
        get; set;
    }

    public string DriverProvider
    {
        get; set;
    }

    public string DriverVersion
    {
        get; set;
    }

    public DateTime DriverDate
    {
        get; set;
    }

    public string DeviceLocationInformation
    {
        get; set;
    }

    public string DeviceManufacturer
    {
        get; set;
    }

    public string DeviceInstancePath
    {
        get; set;
    }

    public string ParentDevice
    {
        get; set;
    }


    public string TransportType
    {
        get; set;
    }

    public string TransportGlyph
    {
        get; set;
    }

    public ICollection<SampleMidiEndpoint> MidiEndpoints
    {
        get; set;
    }

    public override string ToString() => $"{Name}";
}
