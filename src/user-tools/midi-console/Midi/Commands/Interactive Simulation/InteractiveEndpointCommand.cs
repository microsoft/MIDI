// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
{

    internal class InteractiveEndpointCommand : Command<InteractiveEndpointCommand.Settings>
    {
        private bool _continue = true;

        private delegate void MenuActionCallback();

        private class MenuItem
        {
            public string Text { get; set; } = string.Empty;
            public MenuActionCallback? Action { get; set; }
        }


        //MidiVirtualDeviceResponder _responder;
//        MidiEndpointConnection? _endpoint = null;


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


        private void ActionRequestFunctionBlocks()
        {
            // TODO: Prompt for single or all
        }
        private void ActionRequestEndpointMetadata()
        {
            // TODO: Prompt for single or all
        }

        private void ActionQuit()
        {
            _continue = false;
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


        public void ShowMainMenu()
        {
            while (_continue)
            {
                var choices = new List<MenuItem>();

                choices.Add(new MenuItem() { Text = "Request Function Blocks from Endpoint", Action = ActionRequestFunctionBlocks });
                choices.Add(new MenuItem() { Text = "Request Endpoint Metadata from Endpoint", Action = ActionRequestEndpointMetadata });

                // TODO: Virtual device sub-menu



                var selectionStyle = new Style(Color.White, Color.DeepSkyBlue3, null, null);

                var result = AnsiConsole.Prompt(
                    new SelectionPrompt<MenuItem>()
                        .Title(Strings.InteractiveEndpointMenuPleaseSelectOption)
                        .HighlightStyle(selectionStyle)
                        .AddChoices(choices)
                    );

                if (result != null && result.Action != null)
                {
                    result.Action();
                }
            }

            return;
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

