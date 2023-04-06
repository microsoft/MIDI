using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;

// TODO: Move these to the ViewModels/Data folder and base on VM base class
// The model/data itself needs to come from the SDK. 

public enum MidiFunctionBlockProtocol
{
    Midi1,
    Midi2
}

public enum MidiFunctionBlockDirection
{
    Bidirectional,
    InputReceiver,
    OutputSource
}

public enum MidiFunctionUIHint
{
    Bidirectional,
    PrimarilyReceiver,
    PrimarilyOutputSource
}


public enum MidiFunctionBlockMidi1BandwidthFlag
{
    NotMidi1 = 0,
    Midi1DoNotRestrictBandwidth,
    Midi1RestrictTo31250,
    Reserved3
}

public class DisplayFriendlyMidiFunctionBlock
{
    public byte Number
    {
        get; set;
    }

    public bool IsActive
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

    public byte MidiCiMessageFormat
    {
        get; set; 
    }

    public byte MaximumSimultaneousSysEx8Streams
    {
        get; set;
    }

    public IList<DisplayFriendlyMidiGroup> SpannedGroups
    {
        get; set; 
    }


    public IDictionary<string, string> AllProperties
    {
        get; set;
    }


    public MidiFunctionUIHint UIHint
    {
        get; set;
    }

    public MidiFunctionBlockDirection Direction
    {
        get; set;
    }

    public MidiFunctionBlockProtocol Protocol
    {
        get; set;
    }

    public MidiFunctionBlockMidi1BandwidthFlag Midi1BandwidthFlag
    {
        get; set;
    }

}

