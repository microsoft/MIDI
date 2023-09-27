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

        ErrorWinRTTypeActivationFailure = 10,
        ErrorMidiServicesNotInstalled = 12,

        ErrorCreatingSession = 100,

        ErrorNoEndpointsFound = 200,
        ErrorCreatingEndpointConnection = 210,
        ErrorOpeningEndpointConnection = 212,

        ErrorMalformedUmp = 300,


        ErrorNotImplemented = 998,
        ErrorGeneralFailure = 999
    }
}
