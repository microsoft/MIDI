using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;

// TODO: Move these to the ViewModels/Data folder and base on VM base class
// The model/data itself needs to come from the SDK. 


public class DisplayFriendlyMidiGroup
{
    public byte MidiGroupNumber
    {
        get; set;
    }

    public string FullGroupNameWithBlocks
    {
        get; set;
    }


}
