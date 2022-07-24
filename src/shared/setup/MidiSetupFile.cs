using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiSetup
{
    public class MidiSetupFile
    {
        string FileName { get; set; }

        Schema.Header Header { get; set; }
    }
}
