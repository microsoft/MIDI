// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



using Spectre.Console;

namespace Microsoft.Midi.ConsoleApp
{
    internal class UmpEndpointPickerEntry : IComparable<UmpEndpointPickerEntry>
    {
        public string Name { get; private set; }
        public string EndpointDeviceId { get; private set; }

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
           // Console.WriteLine("DEBUG: LoadChoices");

            var endpoints = MidiEndpointDeviceInformation.FindAll(
                MidiEndpointDeviceInformationSortOrder.Name, 
                MidiEndpointDeviceInformationFilters.StandardNativeMidi1ByteFormat | 
                    MidiEndpointDeviceInformationFilters.StandardNativeUniversalMidiPacketFormat | 
                    MidiEndpointDeviceInformationFilters.DiagnosticLoopback);

            if (endpoints != null)
            {
             //   Console.WriteLine("DEBUG: endpoint list returned");

                foreach (var endpoint in endpoints)
                {
                    var parent = endpoint.GetParentDeviceInformation();

                    var transportInfo = endpoint.GetTransportSuppliedInfo();
                    string fullName;

                    if (transportInfo.ManufacturerName != "Microsoft" && transportInfo.ManufacturerName != string.Empty)
                    {
                        fullName = AnsiMarkupFormatter.FormatManufacturerName(transportInfo.ManufacturerName) + " " + AnsiMarkupFormatter.FormatEndpointName(endpoint.Name);
                    }
                    else
                    {
                        fullName = AnsiMarkupFormatter.FormatEndpointName(endpoint.Name);
                    }

                    if (parent != null)
                    {
                        choices.Add(new UmpEndpointPickerEntry(
                            AnsiMarkupFormatter.GetEndpointIcon(endpoint.EndpointPurpose) + " " +
                            (fullName + " [grey35](" + AnsiMarkupFormatter.EscapeString(endpoint.GetParentDeviceInformation().Name) + ")[/]").PadRight(80),
                            endpoint.EndpointDeviceId));
                    }
                    else
                    {
                        choices.Add(new UmpEndpointPickerEntry(
                            AnsiMarkupFormatter.GetEndpointIcon(endpoint.EndpointPurpose) + " " +
                            fullName.PadRight(80),
                            endpoint.EndpointDeviceId));
                    }
                }

              //  Console.WriteLine("DEBUG: sorting");

                choices.Sort();

                // this feels so dirty               
                choices.Add(new UmpEndpointPickerEntry("🔙 " + "(Cancel)".PadRight(80), "")); // TODO: Localize

            }
        }

        public static string PickEndpoint(string prompt)
        {

            var choices = new List<UmpEndpointPickerEntry>();

            LoadChoices(choices);

            if (choices.Count > 0)
            {
                //Console.WriteLine("DEBUG: prompt");

                //var selectionStyle = new Style(null, null, Decoration.Invert, null);
                var selectionStyle = new Style(Color.White, Color.DeepSkyBlue3, null, null);

                var result = AnsiConsole.Prompt(
                    new SelectionPrompt<UmpEndpointPickerEntry>()
                        .PageSize(20)
                        .Title(prompt)
                        .HighlightStyle(selectionStyle)
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

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(Strings.CommonStringCanceled));
            return string.Empty;
        }

        public static string PickEndpoint()
        {
            return PickEndpoint(Strings.EndpointPickerPleaseSelectEndpoint);
        }

    }
}
