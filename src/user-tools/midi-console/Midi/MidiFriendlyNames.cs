using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp
{
    public class MidiFriendlyNames
    {
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
    }
}
