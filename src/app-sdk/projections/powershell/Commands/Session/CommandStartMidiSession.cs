using Microsoft.Windows.Devices.Midi2.Initialization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{


    [Cmdlet(VerbsLifecycle.Start, "MidiSession")]
    public class CommandStartMidiSession : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public string Name { get; set; }

        protected override void ProcessRecord()
        {
            // todo: check to see if initialized. If not, throw

            var backingSession = Microsoft.Windows.Devices.Midi2.MidiSession.Create(Name);

            if (backingSession != null)
            {
                var session = new MidiSession(backingSession);

                WriteVerbose("MIDI Session started.");
                WriteObject(session);
            }
            else
            {
                throw new ArgumentException("Unable to create session. Has MIDI been initialized with Start-Midi?");
            }
        }
    }


}
