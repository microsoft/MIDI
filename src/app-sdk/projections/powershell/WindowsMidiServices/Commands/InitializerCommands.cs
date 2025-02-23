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
            Initializer.CreateInitializer();
            Initializer.Initialize();
        }

    }

    [Cmdlet(VerbsLifecycle.Stop, "Midi")]
    public class ShutdownCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            Initializer.Shutdown();
        }
    }



}
