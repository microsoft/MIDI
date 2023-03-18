using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;


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
