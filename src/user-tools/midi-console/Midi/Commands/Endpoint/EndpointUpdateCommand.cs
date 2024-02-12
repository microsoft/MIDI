using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class EndpointUpdateCommand : Command<EndpointUpdateCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterEndpointUpdateName")]
            [CommandOption("-n|--name")]
            public string? NewName { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            if (string.IsNullOrEmpty(endpointId))
            {
                return (int)MidiConsoleReturnCode.ErrorNoEndpointsFound;
            }


            // now we can update properties








            return (int)MidiConsoleReturnCode.Success;
        }



    }
}
