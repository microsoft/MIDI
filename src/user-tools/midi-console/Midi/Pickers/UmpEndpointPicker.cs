// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



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

                    if (parent != null)
                    {
                        choices.Add(new UmpEndpointPickerEntry(
                            AnsiMarkupFormatter.GetEndpointIcon(endpoint.EndpointPurpose) + " " +
                            (endpoint.Name + " [grey35](" + endpoint.GetParentDeviceInformation().Name + ")[/]").PadRight(80),
                            endpoint.EndpointDeviceId));
                    }
                    else
                    {
                        choices.Add(new UmpEndpointPickerEntry(
                            AnsiMarkupFormatter.GetEndpointIcon(endpoint.EndpointPurpose) + " " +
                            (endpoint.Name).PadRight(80),
                            endpoint.EndpointDeviceId));
                    }
                }

              //  Console.WriteLine("DEBUG: sorting");

                choices.Sort();

                // this feels so dirty               
                choices.Add(new UmpEndpointPickerEntry("🔙 " + "(Cancel)".PadRight(80), "")); // TODO: Localize

            }
        }


        public static string PickEndpoint()
        {
            //Console.WriteLine("DEBUG: PickEndpoint");

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
                        .Title(Strings.EndpointPickerPleaseSelectEndpoint)
                        .HighlightStyle(selectionStyle)
                        .AddChoices(choices)
                    ) ;

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
