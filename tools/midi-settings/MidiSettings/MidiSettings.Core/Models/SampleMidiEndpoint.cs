using System;
using System.Collections.Generic;
using System.Text;

namespace MidiSettings.Core.Models;

public class SampleMidiEndpoint
{
    public string Id
    {
        get; set;
    }

    public string Name
    {
        get; set;
    }

    public string DeviceSuppliedDisplayName
    {
        get; set;
    }



    public string ProtocolVersion
    {
        get; set;
    }




    // todo: groups and function blocks?

    
}
