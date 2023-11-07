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
    internal class UmpEndpointPickerEntry : IComparable<UmpEndpointPickerEntry>
    {
        public string Name { get; init; }
        public string EndpointDeviceId { get; init; }

        public UmpEndpointPickerEntry(string name, string endpointDeviceId)
        {
            Name = name;
            EndpointDeviceId = endpointDeviceId;
        }

        public override string ToString()
        {
            return Name;

        }

        // for sorting
        public int CompareTo(UmpEndpointPickerEntry? other)
        {
            if (other == null) return 1;

            return this.Name.CompareTo(other.Name);
        }
    }

    internal class UmpEndpointPicker
    {
        private static void LoadChoices(List<UmpEndpointPickerEntry> choices)
        {
            var endpoints = MidiEndpointDeviceInformation.FindAll(
                MidiEndpointDeviceInformationSortOrder.Name, 
                MidiEndpointDeviceInformationFilter.IncludeClientByteStreamNative | MidiEndpointDeviceInformationFilter.IncludeClientUmpNative | MidiEndpointDeviceInformationFilter.IncludeDiagnosticLoopback);

            if (endpoints != null)
            {
                foreach (var endpoint in endpoints)
                {
                    choices.Add(new UmpEndpointPickerEntry(endpoint.Name, endpoint.Id));
                }

                choices.Sort();
            }
        }


        public static string PickEndpoint()
        {
            var choices = new List<UmpEndpointPickerEntry>();

            LoadChoices(choices);

            if (choices.Count > 0)
            {
                var result = AnsiConsole.Prompt(
                    new SelectionPrompt<UmpEndpointPickerEntry>()
                        .Title(Strings.EndpointPickerPleaseSelectEndpoint)
                        .AddChoices(choices)
                    );


                if (result != null)
                {
                    return result.EndpointDeviceId;
                }
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorEnumEndpointsFailed));
            }

            return string.Empty;
        }

    }
}
