using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal sealed class EnumDevicesCommand : Command<EnumDevicesCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            throw new NotImplementedException();

            return 0;
        }

    }
}
