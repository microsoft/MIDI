using Spectre.Console.Cli;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{

    internal class InteractiveEndpointCommand : Command<InteractiveEndpointCommand.Settings>
    {
        //MidiVirtualDeviceResponder _responder;
        MidiBidirectionalEndpointConnection _endpoint;


        internal class Settings : CommandSettings
        {
            [LocalizedDescription("TODO")]
            [CommandArgument(0, "<Endpoint Definition File>")]
            public string? EndpointDefinitionFile { get; set; }
        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.EndpointDefinitionFile == null)
            {
                return ValidationResult.Error("Missing endpoint definition json file");
            }

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Interactive Endpoint not yet implemented."));

            return (int)MidiConsoleReturnCode.ErrorNotImplemented;
        }

        // will also want ways to send arbitrary messages, so that code may need to be refactored so it can
        // be used here (send individual words, or maybe send more)

        private void MoveFunctionBlock(byte functionBlockId, byte newStartGroupIndex, byte newGroupSpan)
        {
            // if not static fbs in the endpoint info
        }

        private void UpdateFunctionBlockName(byte functionBlockId, string newName)
        {
            // if not static fbs in the endpoint info
        }

        private void DeactivateFunctionBlock(byte functionBlockId)
        {
            // if not static fbs in the endpoint info
        }

        private void ActivateFunctionBlock(byte functionBlockId)
        {
            // if not static fbs in the endpoint info
        }

        private void UpdateEndpointName(string newName)
        {

        }

        private void UpdateEndpointProductInstanceId(string newProductInstanceId)
        {

        }

    }
}

