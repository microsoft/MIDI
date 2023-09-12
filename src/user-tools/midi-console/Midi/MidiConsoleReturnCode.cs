using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    public enum MidiConsoleReturnCode
    {
        Success = 0,

        WinRTTypeActivationFailure = 10,

        ErrorCreatingSession = 100,

        NoEndpointsFound = 200,
        ErrorCreatingEndpointConnection = 210,
        ErrorOpeningEndpointConnection = 212,

        MalformedUmp = 300,


        GeneralFailure = 999
    }
}
