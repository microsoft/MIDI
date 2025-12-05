// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



using Windows.Devices.Enumeration;
using Windows.Devices.Midi;

namespace Microsoft.Midi.ConsoleApp
{
    internal sealed class EnumLegacyEndpointsCommand : Command<EnumLegacyEndpointsCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumLegacyEndpointsDirection")]
            [CommandOption("-d|--direction")]
            [DefaultValue(LegacyEndpointDirection.All)]
            public LegacyEndpointDirection EndpointDirection { get; init; }

            [LocalizedDescription("ParameterEnumLegacyEndpointsIncludeEndpointId")]
            [CommandOption("-i|--include-endpoint-id")]
            [DefaultValue(true)]
            public bool IncludeId { get; init; }
        }


        struct MidiPort
        {
            public string Number;
            public string Name;
            public string Id;
            public LegacyEndpointDirection Direction;

            public MidiPort(string number, string name, string id, LegacyEndpointDirection direction)
            {
                Number = number;
                Name = name;
                Id = id;
                Direction = direction;
            }
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}

            bool atLeastOneEndpointFound = false;

            var table = new Table();

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            if (settings.IncludeId)
            {
                table.ShowRowSeparators();
            }



            table.AddColumn("MIDI 1.0 Byte Format Endpoints for older MIDI APIs");

            // Input endpoints

            // using properties directly is not supported for third-party apps. The numbers may change in the future
            // #define STRING_PKEY_MIDI_ServiceAssignedPortNumber MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"17"
            // DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_ServiceAssignedPortNumber, 17);     // DEVPROP_TYPE_UINT32
            string STRING_PKEY_MIDI_ServiceAssignedPortNumber = "{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},17";
            string[] additionalProperties = { STRING_PKEY_MIDI_ServiceAssignedPortNumber };

            var midiPorts = new List<MidiPort>();
            uint longestNameLength = 0;

            if (settings.EndpointDirection == LegacyEndpointDirection.All || settings.EndpointDirection == LegacyEndpointDirection.In)
            {
                var endpoints = DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector(), additionalProperties)
                    .GetAwaiter().GetResult();

                if (endpoints.Count > 0)
                {
                    atLeastOneEndpointFound = true;

                    foreach (var endpointInfo in endpoints)
                    {
                        // we set to blank because not all WinRT MIDI 1.0 ports are WinMM ports. BLE for example
                        string serviceAssignedPortNumber = "";


                        if (endpointInfo.Properties.ContainsKey(STRING_PKEY_MIDI_ServiceAssignedPortNumber))
                        {
                            var prop = endpointInfo.Properties[STRING_PKEY_MIDI_ServiceAssignedPortNumber];

                            if (prop != null)
                            {
                                // internal info. Again, this is why property use is not directly supported for apps yet
                                // for midi inputs, we -1 from the number
                                serviceAssignedPortNumber = ((uint)prop - 1).ToString();
                            }
                        }

                        longestNameLength = Math.Max(longestNameLength, (uint)endpointInfo.Name.Length);

                        midiPorts.Add(new MidiPort(serviceAssignedPortNumber, endpointInfo.Name, endpointInfo.Id.ToLower(), LegacyEndpointDirection.In));

                    }
                }
                else
                {
                    table.AddRow("No input endpoints.");
                }
            }

            // Output endpoints

            if (settings.EndpointDirection == LegacyEndpointDirection.All || settings.EndpointDirection == LegacyEndpointDirection.Out)
            {
                var endpoints = DeviceInformation.FindAllAsync(MidiOutPort.GetDeviceSelector(), additionalProperties)
                    .GetAwaiter().GetResult();

                if (endpoints.Count > 0)
                {
                    atLeastOneEndpointFound = true;

                    foreach (var endpointInfo in endpoints)
                    {
                        string serviceAssignedPortNumber = "";

                        if (endpointInfo.Name == "Microsoft GS Wavetable Synth")
                        {
                            // GS synth is a hard-coded device 0
                            serviceAssignedPortNumber = "0";
                        }
                        else if (endpointInfo.Properties.ContainsKey(STRING_PKEY_MIDI_ServiceAssignedPortNumber))
                        {
                            var prop = endpointInfo.Properties[STRING_PKEY_MIDI_ServiceAssignedPortNumber];

                            if (prop != null)
                            {
                                serviceAssignedPortNumber = ((uint)prop).ToString();
                            }
                        }

                        longestNameLength = Math.Max(longestNameLength, (uint)endpointInfo.Name.Length);

                        midiPorts.Add(new MidiPort(serviceAssignedPortNumber, endpointInfo.Name, endpointInfo.Id.ToLower(), LegacyEndpointDirection.Out));
                    }
                }
                else
                {
                    table.AddRow("No output endpoints.");
                }
            }

            foreach (var port in midiPorts.OrderBy(p => p.Direction).ThenBy(p => (p.Number == string.Empty? -1 : int.Parse(p.Number))))
            {
                DisplayEndpointInformationFormatted(table, settings, port, (int)longestNameLength);
            }

            AnsiConsole.Write(table);

            if (atLeastOneEndpointFound)
            {
                return 0;
            }
            else
            {
                // TODO: Formalize error codes.
                return 1;
            }
        }


        private void DisplayEndpointInformationFormatted(Table table, Settings settings, MidiPort port, int nameColumnWidth)
        {
            string endpointType = string.Empty;

            if (port.Direction == LegacyEndpointDirection.In)
            {
                endpointType = Strings.DirectionMessageSource;
            }
            else
            {
                endpointType = Strings.DirectionMessageDestination;
            }

            string data = string.Empty;

            // we optimize for the normal case of < 100 inputs or outputs. If > 100, the formatting just gets pushed over
            data = $"[grey35]Port[/] [orange3]{port.Number.PadLeft(2)}[/] [grey35]:[/] " + 
                AnsiMarkupFormatter.FormatEndpointName(port.Name.PadRight(nameColumnWidth)) + $" [silver]{endpointType}[/]";

            if (settings.IncludeId)
            {
                data += "\n";
                data += AnsiMarkupFormatter.FormatFullEndpointInterfaceId(port.Id);
            }


            table.AddRow(data);
        }

    }
}
