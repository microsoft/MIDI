using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp
{
    public class MidiFriendlyNames
    {
        public static string DevicePurpose(MidiEndpointDevicePurpose purpose)
        {
            switch (purpose)
            {
                case MidiEndpointDevicePurpose.DiagnosticLoopback:
                    return Strings.DevicePurposeDiagnosticLoopback;
                case MidiEndpointDevicePurpose.DiagnosticPing:
                    return Strings.DevicePurposeDiagnosticPing;
                case MidiEndpointDevicePurpose.VirtualDeviceResponder:
                    return Strings.DevicePurposeVirtualDeviceResponder;
                case MidiEndpointDevicePurpose.InBoxGeneralMidiSynth:
                    return Strings.DevicePurposeInBoxGeneralMidiSynth;
                case MidiEndpointDevicePurpose.NormalMessageEndpoint:
                    return Strings.DevicePurposeNormalMessageEndpoint;
                default:
                    return Strings.DevicePurposeUnknown;
            }
        }

        public static string GroupTerminalBlockDirection(MidiGroupTerminalBlockDirection direction)
        {
            switch (direction)
            {
                case MidiGroupTerminalBlockDirection.Bidirectional:
                    return Strings.DirectionMessageBiDi;
                case MidiGroupTerminalBlockDirection.BlockInput:
                    return Strings.DirectionMessageDestination;
                case MidiGroupTerminalBlockDirection.BlockOutput:
                    return Strings.DirectionMessageSource;
                default:
                    return Strings.Unknown;
            }
        }

        public static string Midi1PortDirection(Midi1PortFlow direction)
        {
            switch (direction)
            {
                case Midi1PortFlow.MidiMessageDestination:
                    return Strings.DirectionMessageDestination;
                case Midi1PortFlow.MidiMessageSource:
                    return Strings.DirectionMessageSource;
                default:
                    return Strings.Unknown;
            }
        }

        

    }
}
