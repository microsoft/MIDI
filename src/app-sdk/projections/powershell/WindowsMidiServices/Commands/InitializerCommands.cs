using Microsoft.Windows.Devices.Midi2.Initialization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    // there's an initialize verb, but no corresponding
    // terminate or shutdown, so we'll just use start/stop

    [Cmdlet(VerbsLifecycle.Start, "Midi")]
    public class InitializeCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            if (Initializer.CreateInitializer())
            {
                if (Initializer.Initialize())
                {
                    WriteVerbose("MIDI Initialized.");
                }
                else
                {
                    // unable to initialize
                    //WriteError(new ErrorRecord())''
                }
            }
            else
            {
                // unable to create initializer
            }

        }

    }

    [Cmdlet(VerbsLifecycle.Stop, "Midi")]
    public class ShutdownCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            if (Initializer.Shutdown())
            {
                WriteVerbose("MIDI Shut down.");
            }
        }
    }



}
