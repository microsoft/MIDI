using System;
using System.Collections.Generic;
using System.Text;

namespace MidiSettings.Core.Models;

public class SampleMidiDevicePluginInformation : SamplePluginBase
{

    public string TransportType
    {
        get; set;
    }


    public override string ToString() => $"{Name}";
}
