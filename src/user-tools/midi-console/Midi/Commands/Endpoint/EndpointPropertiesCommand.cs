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
using static System.Net.Mime.MediaTypeNames;

namespace Microsoft.Devices.Midi2.ConsoleApp
{

    internal class EndpointPropertiesCommand : Command<EndpointPropertiesCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterMonitorEndpointVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate endpoint 

            return ValidationResult.Success();
        }


        // TODO: Tons of strings in here that need to be localized

        public override int Execute(CommandContext context, Settings settings)
        {
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            var table = new Table();
            table.Border(TableBorder.Rounded);
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Property"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Value"));


            MidiEndpointDeviceInformation di = MidiEndpointDeviceInformation.CreateFromId(endpointId);

            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Identification"), "");
            table.AddRow("Name", AnsiMarkupFormatter.FormatEndpointName(di.Name));
            table.AddRow("Id", AnsiMarkupFormatter.FormatFullEndpointInterfaceId(di.Id));
            table.AddRow("Purpose", di.EndpointPurpose.ToString());

            if (settings.Verbose)
            {
                //table.AddRow("Kind", AnsiMarkupFormatter.FormatDeviceKind(di.DeviceInformation.Kind));
                table.AddRow("Container Id", AnsiMarkupFormatter.FormatContainerId(di.ContainerId.ToString()));
                table.AddRow("Device Instance Id", AnsiMarkupFormatter.FormatDeviceInstanceId(di.DeviceInstanceId));
            }

            if (settings.Verbose)
            {
                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Endpoint Metadata"), "");
                table.AddRow("Product Instance Id", AnsiMarkupFormatter.EscapeString(di.ProductInstanceId));
                table.AddRow("Endpoint-Supplied Name", AnsiMarkupFormatter.FormatEndpointName(di.EndpointSuppliedName));
            }

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("User Data"), "");
            table.AddRow("User-Supplied Name", AnsiMarkupFormatter.FormatEndpointName(di.UserSuppliedName));
            table.AddRow("Description", AnsiMarkupFormatter.EscapeString(di.Description));
            table.AddRow("Small Image Path", AnsiMarkupFormatter.EscapeString(di.SmallImagePath));
            table.AddRow("Large Image Path", AnsiMarkupFormatter.EscapeString(di.LargeImagePath));

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
                table.AddRow("Transport Id", AnsiMarkupFormatter.EscapeString(di.TransportId));
                table.AddRow("Transport Mnemonic", AnsiMarkupFormatter.EscapeString(di.TransportMnemonic));
                table.AddRow("Native Data Format", di.NativeDataFormat.ToString());
            }

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Blocks"), "");
            table.AddRow("Static Function Blocks?", di.HasStaticFunctionBlocks.ToString());
            table.AddRow("Function Block Count", di.FunctionBlocks.Count.ToString());

            foreach (var functionBlock in di.FunctionBlocks)
            {
                table.AddEmptyRow();
                table.AddRow("Function Block", AnsiMarkupFormatter.FormatBlockNumber(functionBlock.Number));
                table.AddRow("Name", AnsiMarkupFormatter.FormatBlockName(functionBlock.Name));
                table.AddRow("Active", functionBlock.IsActive.ToString());
                table.AddRow("First Group Index", functionBlock.FirstGroupIndex.ToString());
                table.AddRow("Group Count", functionBlock.GroupCount.ToString());

                if (settings.Verbose)
                {
                    table.AddRow("Direction", functionBlock.Direction.ToString());
                    table.AddRow("UI Hint", functionBlock.UIHint.ToString());
                    table.AddRow("Max SysEx 8 Streams", functionBlock.MaxSystemExclusive8Streams.ToString());
                    table.AddRow("MIDI 1.0", functionBlock.Midi10Connection.ToString());
                    table.AddRow("MIDI CI Version Format", functionBlock.MidiCIMessageVersionFormat.ToString());
                }
            }

            //table.AddRow("Group Terminal Block Count", di.GroupTerminalBlocks.Count.ToString());

            if (di.GroupTerminalBlocks.Count > 0)
            {
                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Group terminal Blocks"), "");

                foreach (var groupTerminalBlock in di.GroupTerminalBlocks)
                {
                    if (!settings.Verbose)
                    {
                        string groupInformation = string.Empty;
                        ;

                        if (groupTerminalBlock.GroupCount == 1)
                        {
                            groupInformation +=
                                $"Group {groupTerminalBlock.FirstGroupIndex + 1} (Index {groupTerminalBlock.FirstGroupIndex})";
                        }
                        else
                        {
                            int stopGroupIndex = groupTerminalBlock.FirstGroupIndex + groupTerminalBlock.GroupCount - 1;
                            groupInformation +=
                                $"Groups {groupTerminalBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} (Indexes {groupTerminalBlock.FirstGroupIndex}-{stopGroupIndex})";

                        }

                        groupInformation += ", Protocol " + groupTerminalBlock.Protocol.ToString();
                        groupInformation += ", Direction " + groupTerminalBlock.Direction.ToString();

                        table.AddRow(
                            AnsiMarkupFormatter.FormatBlockNumber(groupTerminalBlock.Number) + " " +
                            AnsiMarkupFormatter.FormatBlockName(groupTerminalBlock.Name),
                            groupInformation);
                    }
                    else
                    {
                        table.AddEmptyRow();

                        table.AddRow("Group Terminal Block", AnsiMarkupFormatter.FormatBlockNumber(groupTerminalBlock.Number) + " " + AnsiMarkupFormatter.FormatBlockName(groupTerminalBlock.Name));

                        if (groupTerminalBlock.GroupCount == 1)
                        {
                            table.AddRow("Group", $"{groupTerminalBlock.FirstGroupIndex + 1} (Index {groupTerminalBlock.FirstGroupIndex})");
                        }
                        else
                        {
                            int stopGroupIndex = groupTerminalBlock.FirstGroupIndex + groupTerminalBlock.GroupCount - 1;
                            table.AddRow("Groups", $"{groupTerminalBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} (Indexes {groupTerminalBlock.FirstGroupIndex}-{stopGroupIndex})");
                        }

                        table.AddRow("Group Count", groupTerminalBlock.GroupCount.ToString());
                        table.AddRow("Direction", groupTerminalBlock.Direction.ToString());
                        table.AddRow("Protocol", groupTerminalBlock.Protocol.ToString());

                        string inputBandwidth = string.Empty;
                        string outputBandwidth = string.Empty;

                        if (groupTerminalBlock.MaxDeviceInputBandwidthIn4KBitsPerSecondUnits == 0)
                        {
                            inputBandwidth = "0 (Unknown or Not Fixed)";
                        }
                        else
                        {
                            inputBandwidth = groupTerminalBlock.MaxDeviceInputBandwidthIn4KBitsPerSecondUnits.ToString() +
                                $" ({((double)groupTerminalBlock.CalculatedMaxDeviceInputBandwidthBitsPerSecond/1000.0).ToString("N2")} kbps)";
                        }


                        if (groupTerminalBlock.MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits == 0)
                        {
                            outputBandwidth = "0 (Unknown or Not Fixed)";
                        }
                        else
                        {
                            outputBandwidth = groupTerminalBlock.MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits.ToString() +
                                $" ({((double)groupTerminalBlock.CalculatedMaxDeviceOutputBandwidthBitsPerSecond/1000.0).ToString("N2")} kbps)";
                        }

                        table.AddRow("Max Input Bandwidth", inputBandwidth);
                        table.AddRow("Max Output Bandwidth", outputBandwidth);
                    }
                }
            }



            if (settings.Verbose)
            {
                // spit out all properties by key / value in a new table

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("All Properties"), "");

                DisplayProperties(table, di.Properties);
            }

            // container

            var containerInfo = di.GetContainerInformation();

            table.AddEmptyRow();
            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Container"), "");

            if (containerInfo != null)
            {

                table.AddRow("Name", AnsiMarkupFormatter.FormatEndpointName(containerInfo.Name));
                table.AddRow("Id", AnsiMarkupFormatter.FormatContainerId(containerInfo.Id));
                table.AddRow("Kind", AnsiMarkupFormatter.FormatDeviceKind(containerInfo.Kind));

                if (settings.Verbose)
                {
                    DisplayProperties(table, containerInfo.Properties);
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
                    table.AddRow("Kind", AnsiMarkupFormatter.FormatDeviceKind(parentInfo.Kind));

                    if (settings.Verbose)
                    {
                        DisplayProperties(table, parentInfo.Properties);
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




        private void DisplayProperties(Table table, IReadOnlyDictionary<string, object> properties)
        {
            foreach (string key in properties.Keys)
            {
                object value;
                bool found = properties.TryGetValue(key, out value);

                if (found)
                {
                    if (value != null)
                    {
                        if (key == "System.ItemNameDisplay")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatEndpointName(value.ToString()));
                        }
                        else if (key == "System.Devices.DeviceInstanceId")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatDeviceInstanceId(value.ToString()));
                        }
                        else if (key == "System.Devices.Parent")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatDeviceParentId(value.ToString()));
                        }
                        else if (key == "System.Devices.ContainerId")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatContainerId(value.ToString()));
                        }
                        else
                        {
                            if (value is byte[])
                            {
                                string s = string.Empty;

                                foreach (byte b in (byte[])value)
                                {
                                    s += string.Format("{0:X2} ", b);
                                }

                                table.AddRow(key, s);
                            }
                            else
                            {
                                table.AddRow(key, AnsiMarkupFormatter.EscapeString(value.ToString()));
                            }

                        }

                    }
                    else
                    {
                        table.AddRow(key, "[grey35]null[/]");
                    }
                }
            }
        }




    }
}
