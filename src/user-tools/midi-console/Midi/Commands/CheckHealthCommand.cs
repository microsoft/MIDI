using Spectre.Console;
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
            [DefaultValue(true)]
            public bool Loopback { get; init; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            // check to see if the service is running. 
            // TODO: move this code into the SDK

            string serviceName = "MidiSrv";

            var controller = new System.ServiceProcess.ServiceController(serviceName);

            if (controller != null)
            {
                if (controller.Status == System.ServiceProcess.ServiceControllerStatus.Running)
                {
                    AnsiConsole.WriteLine($"Service {serviceName} is running.");

                    if (settings.Loopback)
                    {
                        // TODO: write the loopback test

                        AnsiConsole.WriteLine($"Loopback test is not currently implemented.");


                    }


                }
                else
                {
                    AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"Service '{serviceName}' status is {controller.Status.ToString()}\n"));
                }
            }
            else
            {
                AnsiConsole.Markup(AnsiMarkupFormatter.FormatError($"Unable to connect to service '{serviceName}'\n"));
            }



            // if loopback, do a loopback test



            return 0;
        }
    }
}
