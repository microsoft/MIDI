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
        MidiEndpointConnection _endpoint;


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

        private void UpdateEndpointInfo()
        {
            // send out endpoint info update
        }

        private void UpdateEndpointName(string newName)
        {
            // send out endpoint name update
        }

        private void UpdateEndpointProductInstanceId(string newProductInstanceId)
        {
            // send out product instance id update
        }


        // TODO: Need to localize these strings
        private void ShowMainMenu()
        {
            var result = AnsiConsole.Prompt(
                new SelectionPrompt<string>()
                    .Title("Please select an option")
                    .AddChoices(new[]
                    {
                        "View Configuration",
                        "Update Endpoint Info",
                        "Update Endpoint Product Instance Id",
                        "Function Block Sub-menu",
                        "Shut down Device and Exit"
                    }
                    ));

            
        }

        // TODO: Need to localize these strings
        private void ShowFunctionBlockMenu(MidiFunctionBlock block)
        {
            var result = AnsiConsole.Prompt(
                new SelectionPrompt<string>()
                    .Title("Please select an option")
                    .AddChoices(new[]
                    {
                        "Toggle Function Block Active State",
                        "Update Function Block Name",
                        "Move Function Block",
                        "Function Block Sub-menu",
                        "Back to main menu"
                    }
                    ));
        }


        private void ShowUpdateEndpointInfoPrompts()
        {
            // prompt for MIDI 1.0 compat

            // prompt for MIDI 2.0 compat

            // prompt for UMP version major

            // prompt for ump version minor

            // prompt for RX Jitter Reduction Timestamps

            // prompt for TX Jitter Reduction Timestamps

            // number of function blocks needs to remain fixed. Same with static or dynamic function blocks

        }

        private void ShowUpdateDeviceIdentityPrompts()
        {
            // prompt for the sysex Ids, device family, device family model, software revision bytes
        }




    }
}

