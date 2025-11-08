// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



//using Microsoft.Windows.Devices.Midi2.Initialization;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointUpdateCommand : Command<EndpointUpdateCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterEndpointUpdateName")]
            [CommandOption("-n|--name")]
            public string? NewName { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}


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
