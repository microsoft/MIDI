using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi2;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class UmpEndpointPickerEntry
    {
        public string Name { get; init; }
        public string EndpointDeviceId { get; init; }
        public EndpointDirection Direction { get; init; }

        public UmpEndpointPickerEntry(string name, string endpointDeviceId, EndpointDirection direction)
        {
            Name = name;
            EndpointDeviceId = endpointDeviceId;
            Direction = direction;
        }

        public override string ToString()
        {
            return Name;

        }
    }

    internal class UmpEndpointPicker
    {
        private static void LoadChoices(List<UmpEndpointPickerEntry> choices, string selector, EndpointDirection direction)
        {
            var endpoints = DeviceInformation.FindAllAsync(selector)
                .GetAwaiter()
                .GetResult();

            if (endpoints != null)
            {
                foreach (var endpoint in endpoints)
                {
                    choices.Add(new UmpEndpointPickerEntry(endpoint.Name, endpoint.Id, direction));
                }
            }
        }


        public static string PickInput()
        {
            var choices = new List<UmpEndpointPickerEntry>();

            LoadChoices(choices, MidiBidirectionalEndpointConnection.GetDeviceSelector(), EndpointDirection.Bidirectional);
            LoadChoices(choices, MidiInputEndpointConnection.GetDeviceSelector(), EndpointDirection.In);

            if (choices.Count > 0)
            {
                var result = AnsiConsole.Prompt(
                    new SelectionPrompt<UmpEndpointPickerEntry>()
                        .Title("Please select an output endpoint")
                        .AddChoices(choices)
                    );


                if (result != null)
                {
                    return result.EndpointDeviceId;
                }
            }

            return string.Empty;
        }

        public static string PickOutput()
        {
            var choices = new List<UmpEndpointPickerEntry>();

            LoadChoices(choices, MidiBidirectionalEndpointConnection.GetDeviceSelector(), EndpointDirection.Bidirectional);
            LoadChoices(choices, MidiOutputEndpointConnection.GetDeviceSelector(), EndpointDirection.Out);

            if (choices.Count > 0)
            {
                var result = AnsiConsole.Prompt(
                    new SelectionPrompt<UmpEndpointPickerEntry>()
                        .Title("Please select an output endpoint")
                        .AddChoices(choices)
                    );


                if (result != null)
                {
                    return result.EndpointDeviceId;
                }
            }

            return string.Empty;

        }
    }
}
