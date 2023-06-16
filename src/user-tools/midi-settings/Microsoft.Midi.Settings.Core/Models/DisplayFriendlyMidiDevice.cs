using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;

// TODO: Move these to the ViewModels/Data folder and base on VM base class
// The model/data itself needs to come from the SDK. 

public enum MidiDeviceTransportType
{
    Unknown=0,
    Usb,
    Network,
    Bluetooth,
    Serial
}

public enum MidiNativeDeviceDataFormat
{
    Unknown=0,
    ByteStream,
    Ump
}

public enum MidiDeviceProtocolSupport
{
    Midi1 = 1,
    Midi2 = 2,
    All = 3
}

public enum MidiDeviceConfiguredProtocol
{
    Midi1 = 1,
    Midi2 = 2
}


// Device and UMP Endpoint are synonymous
public class DisplayFriendlyMidiDevice
{
    public string Id
    {
        get; set; 
    }

    public string Name
    {
        get; set;
    }

    public string Manufacturer
    {
        get; set;
    }


    public byte[] IdentityManufacturerSysExId
    {
        get; set; 
    }

    public byte[] IdentityDeviceFamily
    {
        get; set;
    }

    public byte[] IdentityDeviceFamilyModelNumber
    {
        get; set;
    }

    public byte[] SoftwareRevisionLevel
    {
        get; set;
    }

    public string ProductInstanceId
    {
        get; set;
    }


    public bool FunctionBlocksAreStatic
    {
        get; set;
    }

    public bool CanReceiveJRTimestamps
    {
        get; set;
    }

    public bool CanTransmitJRTimestamps
    {
        get; set;
    }


    public MidiDeviceProtocolSupport ProtocolSupport
    {
        get; set;
    }

    public MidiNativeDeviceDataFormat NativeDataFormat
    {
        get; set; 
    }

    public MidiDeviceTransportType TransportType
    {
        get; set; 
    }

    public IList<DisplayFriendlyMidiFunctionBlock> FunctionBlocks
    {
        get; set; 
    }

    public IList<DisplayFriendlyMidiGroup> ActiveGroups
    {
        get; set;
    }

    public IDictionary<string, string> AllProperties
    {
        get; set;
    }

    // TODO: Sysex IDs
    //


}
