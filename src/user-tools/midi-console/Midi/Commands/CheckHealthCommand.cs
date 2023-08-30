using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    // commands to check the health of Windows MIDI Services on this PC
    // will check for MIDI services
    // will attempt a loopback test


    internal class CheckHealthCommand : Command<CheckHealthCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterCheckHealthLoopback")]
            [CommandOption("-l|--loopback")]
            public bool? Loopback { get; init; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            throw new NotImplementedException();

            return 0;
        }
    }
}
