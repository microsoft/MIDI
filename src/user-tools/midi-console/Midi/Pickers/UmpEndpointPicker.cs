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
                const string supportsMidi2Indicator = "Midi 2.0  "; // optional field so include the trailing spaces

                int maxEndpointNameWidth = 0;
                int maxManufacturerNameWidth = 0;
                int maxMidi2Width = 0;
                int maxTransportCodeWidth = 0;

                // calculate sizes
                foreach (var endpoint in endpoints)
                {
                    maxEndpointNameWidth = Math.Max(endpoint.Name.Length + 2, maxEndpointNameWidth);

                    var transportInfo = endpoint.GetTransportSuppliedInfo();

                    if (transportInfo.ManufacturerName != "Microsoft" && transportInfo.ManufacturerName != string.Empty)
                    {
                        maxManufacturerNameWidth = Math.Max(transportInfo.ManufacturerName.Length, maxManufacturerNameWidth);
                    }

                    maxTransportCodeWidth = Math.Max(transportInfo.TransportCode.Length + 2, maxTransportCodeWidth);

                    if (endpoint.GetDeclaredEndpointInfo().SupportsMidi20Protocol)
                    {
                        maxMidi2Width = supportsMidi2Indicator.Length;  
                    }
                }

                int totalLineWidth = 2 + maxEndpointNameWidth + maxTransportCodeWidth + maxMidi2Width + maxManufacturerNameWidth;


                foreach (var endpoint in endpoints)
                {
                    //var parent = endpoint.GetParentDeviceInformation();

                    var transportInfo = endpoint.GetTransportSuppliedInfo();

                    string endpointNamePadded = endpoint.Name.PadRight(maxEndpointNameWidth, ' ');
                    string manufacturerNamePadded;
                    string midi2Padded = string.Empty;
                    string transportCodePadded = transportInfo.TransportCode.PadRight(maxTransportCodeWidth, ' ');

                    if (endpoint.GetDeclaredEndpointInfo().SupportsMidi20Protocol)
                    {
                        midi2Padded = "[grey]" + supportsMidi2Indicator.PadRight(maxMidi2Width) + "[/]";  // optional field so include the trailing space
                    }
                    else
                    {
                        midi2Padded = new string(' ', maxMidi2Width);
                    }

                    if (transportInfo.ManufacturerName != "Microsoft" && transportInfo.ManufacturerName != string.Empty)
                    {
                        manufacturerNamePadded = transportInfo.ManufacturerName.PadRight(maxManufacturerNameWidth, ' ');
                    }
                    else
                    {
                        manufacturerNamePadded = new string(' ', maxManufacturerNameWidth);
                    }

                    choices.Add(new UmpEndpointPickerEntry(
                        AnsiMarkupFormatter.GetEndpointIcon(endpoint) + " " +
                        AnsiMarkupFormatter.FormatEndpointName(endpointNamePadded) +
                        AnsiMarkupFormatter.FormatTransportCode(transportCodePadded) +
                        midi2Padded +
                        AnsiMarkupFormatter.FormatManufacturerName(manufacturerNamePadded),
                        endpoint.EndpointDeviceId));
                }

                //  Console.WriteLine("DEBUG: sorting");

             //   choices.Sort();

                // this feels so dirty               
                choices.Add(new UmpEndpointPickerEntry("🔙 " + "(Cancel)".PadRight(totalLineWidth, ' '), "")); // TODO: Localize

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
