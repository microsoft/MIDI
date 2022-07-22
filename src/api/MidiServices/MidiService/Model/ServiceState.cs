using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Collections;
using System.Reflection;

namespace MidiService.Model
{
    public class ServiceState
    {
        // TODO: Store PID of settings app so we can verify that only
        // the settings app or the console can destroy someone else's 
        // session? Not really secure, though, and easy to spoof.


        //private DeviceGraph _deviceGraph = new DeviceGraph();


        public Version? GetServiceVersion()
        {
            return Assembly.GetExecutingAssembly().GetName().Version;
        }

        public Statistics Statistics { get; } = new Statistics();


    }
}
