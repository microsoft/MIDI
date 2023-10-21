using Spectre.Console.Cli;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;
using Windows.Devices.Enumeration;

namespace Microsoft.Devices.Midi2.ConsoleApp
{

    internal class EndpointPropertiesCommand : Command<EndpointPropertiesCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterMonitorEndpointVerbose")]
            [CommandOption("-v|--verbose|--details")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate endpoint 

            return ValidationResult.Success();
        }


        public override int Execute(CommandContext context, Settings settings)
        {
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickInput();
            }

            var table = new Table();
            table.Border(TableBorder.Rounded);
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Property"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Value"));


            MidiEndpointDeviceInformation di = MidiEndpointDeviceInformation.CreateFromId(endpointId);

            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Identification"), "");
            table.AddRow("Name", AnsiMarkupFormatter.FormatEndpointName(di.Name));
            table.AddRow("Id", AnsiMarkupFormatter.FormatDeviceInstanceId(di.Id));

            if (settings.Verbose)
            {
                table.AddRow("Kind", di.DeviceInformation.Kind.ToString());
                table.AddRow("Purpose", di.EndpointPurpose.ToString());
                table.AddRow("Container Id", di.ContainerId.ToString());
                table.AddRow("Device Instance Id", AnsiMarkupFormatter.FormatDeviceInstanceId(di.DeviceInstanceId));
            }

            if (settings.Verbose)
            {
                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Endpoint Metadata"), "");
                table.AddRow("Product Instance Id", di.ProductInstanceId);
                table.AddRow("Endpoint-Supplied Name", AnsiMarkupFormatter.FormatEndpointName(di.EndpointSuppliedName));
            }

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("User Data"), "");
            table.AddRow("User-Supplied Name", AnsiMarkupFormatter.FormatEndpointName(di.UserSuppliedName));
            table.AddRow("Description", di.Description);
            table.AddRow("Small Image Path", di.SmallImagePath);
            table.AddRow("Large Image Path", di.LargeImagePath);

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Capabilities"), "");
            table.AddRow("UMP Version", di.SpecificationVersionMajor + "." + di.SpecificationVersionMinor);
            table.AddRow("MIDI 1.0 Protocol", di.SupportsMidi10Protocol.ToString());
            table.AddRow("MIDI 2.0 Protocol", di.SupportsMidi20Protocol.ToString());
            table.AddRow("Sending JR Time", di.SupportsSendingJRTimestamps.ToString());
            table.AddRow("Receiving JR Time", di.SupportsReceivingJRTimestamps.ToString());
            table.AddRow("Multi-client", di.SupportsMultiClient.ToString());

            if (settings.Verbose)
            {
                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Transport Information"), "");
                table.AddRow("Transport-supplied Name", AnsiMarkupFormatter.FormatEndpointName(di.TransportSuppliedName));
                table.AddRow("Transport Id", di.TransportId);
                table.AddRow("Transport Mnemonic", di.TransportMnemonic);
                table.AddRow("Native Data Format", di.NativeDataFormat.ToString());
            }

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Blocks"), "");
            table.AddRow("Static Function Blocks?", di.HasStaticFunctionBlocks.ToString());
            table.AddRow("Function Block Count", di.FunctionBlocks.Count.ToString());

            foreach (var functionBlock in di.FunctionBlocks)
            {
                table.AddEmptyRow();
                table.AddRow("Block", functionBlock.Number.ToString());
                table.AddRow("Active", functionBlock.IsActive.ToString());
                table.AddRow("Name", functionBlock.Name);
                table.AddRow("First Group Index", functionBlock.FirstGroupIndex.ToString());
                table.AddRow("Group Span", functionBlock.NumberOfGroupsSpanned.ToString());

                if (settings.Verbose)
                {
                    table.AddRow("Direction", functionBlock.Direction.ToString());
                    table.AddRow("UI Hint", functionBlock.UIHint.ToString());
                    table.AddRow("Max SysEx 8 Streams ", functionBlock.MaxSystemExclusive8Streams.ToString());
                    table.AddRow("MIDI 1.0 ", functionBlock.Midi10Connection.ToString());
                    table.AddRow("MIDI CI Version Format", functionBlock.MidiCIMessageVersionFormat.ToString());
                }
            }

            table.AddRow("Group Terminal Block Count", di.GroupTerminalBlocks.Count.ToString());

            foreach (var groupTerminalBlock in di.GroupTerminalBlocks)
            {
                table.AddEmptyRow();
                table.AddRow("Block", groupTerminalBlock.Number.ToString());
                table.AddRow("Name", groupTerminalBlock.Name);

                //if (settings.Verbose)
                //{
                //    table.AddRow("Direction", functionBlock.Direction.ToString());
                //    table.AddRow("UI Hint", functionBlock.UIHint.ToString());
                //    table.AddRow("Max SysEx 8 Streams ", functionBlock.MaxSystemExclusive8Streams.ToString());
                //    table.AddRow("MIDI 1.0 ", functionBlock.Midi10Connection.ToString());
                //    table.AddRow("MIDI CI Version Format", functionBlock.MidiCIMessageVersionFormat.ToString());
                //}
            }



            if (settings.Verbose)
            {
                // spit out all properties by key / value in a new table

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("All Properties"), "");

                foreach (string key in di.DeviceInformation.Properties.Keys)
                {
                    object value;
                    bool worked = di.DeviceInformation.Properties.TryGetValue(key, out value);

                    if (worked && value != null)
                    {
                        table.AddRow(key, value.ToString());
                    }
                    else
                    {
                        table.AddRow(key, "<Unknown>");
                    }
                }
            }

            // container

            var containerInfo = di.GetContainerInformation();

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Container"), "");

            if (containerInfo != null)
            {

                table.AddRow("Name", AnsiMarkupFormatter.FormatEndpointName(containerInfo.Name));
                table.AddRow("Id", AnsiMarkupFormatter.FormatDeviceInstanceId(containerInfo.Id));
                table.AddRow("Kind", containerInfo.Kind.ToString());

                if (settings.Verbose)
                {
                    foreach (string key in containerInfo.Properties.Keys)
                    {
                        object value;
                        bool worked = containerInfo.Properties.TryGetValue(key, out value);

                        if (worked && value != null)
                        {
                            table.AddRow(key, ""); // value.ToString());
                        }
                        else
                        {
                            table.AddRow(key, "<Unknown>");
                        }
                    }
                }
            }
            else
            {
                table.AddRow(AnsiMarkupFormatter.FormatError("No matching container found"), "");
            }

            // parent

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Parent Device"), "");

            if (containerInfo != null)
            {
                var myDevice = DeviceInformation.FindAllAsync(
                    $"System.Devices.ContainerId:=\"{containerInfo.Id}\" AND System.Devices.DeviceInstanceId:=\"{di.DeviceInstanceId}\"",
                    new[]
                    {
                        "System.Devices.Parent"
                    },
                    DeviceInformationKind.Device
                    ).GetAwaiter().GetResult();

                var parentInfo = di.GetParentDeviceInformation();

                if (parentInfo != null)
                {

                    table.AddRow("Name", AnsiMarkupFormatter.FormatEndpointName(parentInfo.Name));
                    table.AddRow("Id", AnsiMarkupFormatter.FormatDeviceInstanceId(parentInfo.Id));
                    table.AddRow("Kind", parentInfo.Kind.ToString());

                    if (settings.Verbose)
                    {
                        foreach (string key in parentInfo.Properties.Keys)
                        {
                            object value;
                            bool worked = parentInfo.Properties.TryGetValue(key, out value);

                            if (worked && value != null)
                            {
                                table.AddRow(key, value.ToString());
                            }
                            else
                            {
                                table.AddRow(key, "<Unknown>");
                            }
                        }
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatError("No device parent found"), "");
                }
            }
            else
            {
                table.AddRow(AnsiMarkupFormatter.FormatError("No matching container found"), "");
            }

            AnsiConsole.Write(table);


            return (int)(MidiConsoleReturnCode.Success);
        }



    }
}
