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
    public class StartSessionCommand : Cmdlet
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


    [Cmdlet(VerbsLifecycle.Stop, "MidiSession")]
    public class StopSessionCommand : Cmdlet
    {
        [Parameter(Mandatory = true)]
        public MidiSession Session { get; set; }
        protected override void ProcessRecord()
        {
            if (Session.BackingSession != null)
            {
                WriteVerbose("MIDI Session stopped.");
                Session.BackingSession.Dispose();
            }
            else
            {
                WriteVerbose("MIDI Session was not previously started.");
            }

            Session = null;
        }


    }




}
