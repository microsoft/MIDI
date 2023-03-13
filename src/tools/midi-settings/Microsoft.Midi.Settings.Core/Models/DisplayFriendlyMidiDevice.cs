using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;


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


public class DisplayFriendlyFunctionBlock
{
    public byte Number
    {
    get; set; 
    }

    public string DeviceSuppliedName
    {
        get; set;
    }

    public string UserSuppliedName
    {
        get; set;
    }

    public byte StartingGroupNumber
    {
        get; set;
    }

    public byte EndingGroupNumber
    {
        get; set; 
    }

    public IDictionary<string, string> AllProperties
    {
        get; set;
    }

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

    public MidiNativeDeviceDataFormat NativeDataFormat
    {
        get; set; 
    }

    public MidiDeviceTransportType TransportType
    {
        get; set; 
    }

    public IList<DisplayFriendlyFunctionBlock> FunctionBlocks
    {
        get; set; 
    }

    public IDictionary<string, string> AllProperties
    {
        get; set;
    }


}
