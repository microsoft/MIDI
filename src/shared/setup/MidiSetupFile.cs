using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Setup
{
    public class MidiSetupFile
    {
        string FileName { get; set; }

        Schema.SetupHeader Header { get; set; }
    }
}
