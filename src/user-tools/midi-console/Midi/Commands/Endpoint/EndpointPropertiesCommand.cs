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
            [LocalizedDescription("ParameterEndpointPropertiesVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }

            [LocalizedDescription("ParameterEndpointPropertiesIncludeRaw")]
            [CommandOption("-r|--include-raw-properties|--include-raw")]
            [DefaultValue(false)]
            public bool IncludeRaw { get; set; }
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

            if (!string.IsNullOrEmpty(endpointId))
            {
                var table = new Table();

                AnsiMarkupFormatter.SetTableBorderStyle(table);


                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableColumnHeaderProperty));
                table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableColumnHeaderValue));


                MidiEndpointDeviceInformation di = MidiEndpointDeviceInformation.CreateFromId(endpointId);

                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableSectionHeaderIdentification), "");
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelName, AnsiMarkupFormatter.FormatEndpointName(di.Name));
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelId, AnsiMarkupFormatter.FormatFullEndpointInterfaceId(di.Id));
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelPurpose, di.EndpointPurpose.ToString());
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelSerialNumber, AnsiMarkupFormatter.EscapeString(di.TransportSuppliedSerialNumber));
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelManufacturer, AnsiMarkupFormatter.EscapeString(di.ManufacturerName));

                if (settings.Verbose)
                {
                    //table.AddRow("Kind", AnsiMarkupFormatter.FormatDeviceKind(di.DeviceInformation.Kind));
                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelContainerId, AnsiMarkupFormatter.FormatContainerId(di.ContainerId.ToString()));
                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelDeviceInstanceId, AnsiMarkupFormatter.FormatDeviceInstanceId(di.DeviceInstanceId));
                }

                if (settings.Verbose)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableSectionHeaderEndpointMetadata), "");
                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelProductInstanceId, AnsiMarkupFormatter.EscapeString(di.ProductInstanceId));
                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelEndpointSuppliedName, AnsiMarkupFormatter.FormatEndpointName(di.EndpointSuppliedName));
                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelSystemExclusiveId, 
                        di.DeviceIdentitySystemExclusiveId[0].ToString("X2") + " " +
                        di.DeviceIdentitySystemExclusiveId[1].ToString("X2") + " " +
                        di.DeviceIdentitySystemExclusiveId[2].ToString("X2"));

                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelDeviceFamily,
                        di.DeviceIdentityDeviceFamilyMsb.ToString("X2") + " " +
                        di.DeviceIdentityDeviceFamilyLsb.ToString("X2"));

                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelDeviceFamilyModelNumber,
                        di.DeviceIdentityDeviceFamilyModelNumberMsb.ToString("X2") + " " +
                        di.DeviceIdentityDeviceFamilyModelNumberLsb.ToString("X2"));

                    table.AddRow(Resources.Strings.PropertiesTablePropertyLabelSoftwareRevisionLevel,
                        di.DeviceIdentitySoftwareRevisionLevel[0].ToString("X2") + " " +
                        di.DeviceIdentitySoftwareRevisionLevel[1].ToString("X2") + " " +
                        di.DeviceIdentitySoftwareRevisionLevel[2].ToString("X2") + " " +
                        di.DeviceIdentitySoftwareRevisionLevel[3].ToString("X2"));
                }

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableSectionHeaderUserData), "");
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelUserSuppliedName, AnsiMarkupFormatter.FormatEndpointName(di.UserSuppliedName));
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelDescription, AnsiMarkupFormatter.EscapeString(di.Description));
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelSmallImagePath, AnsiMarkupFormatter.EscapeString(di.SmallImagePath));
                table.AddRow(Resources.Strings.PropertiesTablePropertyLabelLargeImagePath, AnsiMarkupFormatter.EscapeString(di.LargeImagePath));



                // TODO: Continue with localization


                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Active Configuration"), "");
                table.AddRow("Protocol", di.ConfiguredProtocol.ToString());
                table.AddRow("Sends JR Timestamps", di.ConfiguredToSendJRTimestamps.ToString());
                table.AddRow("Receives JR Timestamps", di.ConfiguredToReceiveJRTimestamps.ToString());

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Declared Capabilities"), "");
                table.AddRow("Multi-client", di.SupportsMultiClient.ToString());
                table.AddRow("MIDI 1.0 Protocol", di.SupportsMidi10Protocol.ToString());
                table.AddRow("MIDI 2.0 Protocol", di.SupportsMidi20Protocol.ToString());

                if (settings.Verbose)
                {
                    table.AddRow("UMP Version", di.SpecificationVersionMajor + "." + di.SpecificationVersionMinor);
                    table.AddRow("Sending JR Time", di.SupportsSendingJRTimestamps.ToString());
                    table.AddRow("Receiving JR Time", di.SupportsReceivingJRTimestamps.ToString());
                }

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
                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Function Blocks"), "");
                table.AddRow("Static Function Blocks?", di.HasStaticFunctionBlocks.ToString());
                table.AddRow("Declared Function Block Count", di.FunctionBlockCount.ToString());
                table.AddEmptyRow();

                if (di.FunctionBlocks.Count > 0)
                {
                    foreach (var functionBlock in di.FunctionBlocks.Values)
                    {
                        if (!settings.Verbose)
                        {
                            string functionInformation = string.Empty;
                            ;

                            if (functionBlock.GroupCount == 1)
                            {
                                functionInformation +=
                                    $"[grey]Group[/] {functionBlock.FirstGroupIndex + 1} (Index {functionBlock.FirstGroupIndex})";
                            }
                            else
                            {
                                int stopGroupIndex = functionBlock.FirstGroupIndex + functionBlock.GroupCount - 1;
                                functionInformation +=
                                    $"[grey]Groups[/] {functionBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} (Indexes {functionBlock.FirstGroupIndex}-{stopGroupIndex})";

                            }

                            functionInformation += ", [grey]MIDI[/] " + functionBlock.Midi10Connection.ToString();
                            functionInformation += ", [grey]Direction[/] " + functionBlock.Direction.ToString();
                            functionInformation += ", [grey]UI Hint[/] " + functionBlock.UIHint.ToString();

                            string active = string.Empty;

                            if (!functionBlock.IsActive)
                            {
                                active = " [grey](inactive)[/] ";
                            }
                            else
                            {
                                // if it's active, we don't show anything special
                                active = string.Empty;
                            }

                            table.AddRow(
                                AnsiMarkupFormatter.FormatBlockNumber(functionBlock.Number) + " " +
                                AnsiMarkupFormatter.FormatBlockName(functionBlock.Name) +
                                active, 
                                functionInformation);
                        }
                        else
                        {
                            table.AddEmptyRow();
                            table.AddRow("Function Block", AnsiMarkupFormatter.FormatBlockNumber(functionBlock.Number) + " " + AnsiMarkupFormatter.FormatBlockName(functionBlock.Name));
                            table.AddRow("Active", functionBlock.IsActive.ToString());
                            table.AddRow("First Group Index", functionBlock.FirstGroupIndex.ToString());
                            table.AddRow("Group Count", functionBlock.GroupCount.ToString());

                            if (settings.Verbose)
                            {
                                table.AddRow("Direction", functionBlock.Direction.ToString());
                                table.AddRow("UI Hint", functionBlock.UIHint.ToString());

                                string sysexStreams = string.Empty;
                                if (functionBlock.MaxSystemExclusive8Streams == 0)
                                {
                                    sysexStreams = "SysEx 8 Not Supported";
                                }
                                else if (functionBlock.MaxSystemExclusive8Streams == 1)
                                {
                                    sysexStreams = "Single SysEx Stream";
                                }
                                else
                                {
                                    sysexStreams = functionBlock.MaxSystemExclusive8Streams + " SysEx Streams";
                                }

                                table.AddRow("Max SysEx 8 Streams", sysexStreams);
                                table.AddRow("MIDI 1.0", functionBlock.Midi10Connection.ToString());
                                table.AddRow("MIDI CI Version Format", functionBlock.MidiCIMessageVersionFormat.ToString());
                            }


                            if (functionBlock.GroupCount == 1)
                            {
                                table.AddRow("Group", $"{functionBlock.FirstGroupIndex + 1} (Index {functionBlock.FirstGroupIndex})");
                            }
                            else
                            {
                                int stopGroupIndex = functionBlock.FirstGroupIndex + functionBlock.GroupCount - 1;
                                table.AddRow("Groups", $"{functionBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} (Indexes {functionBlock.FirstGroupIndex}-{stopGroupIndex})");
                            }

                            table.AddRow("Group Count", functionBlock.GroupCount.ToString());
                        }
                    }
                }

                if (di.GroupTerminalBlocks.Count > 0)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Group Terminal Blocks"), "");
                    
                    if (di.FunctionBlocks.Count > 0)
                    {
                        table.AddRow("Function blocks are present. Applications should use the Function Blocks and their group declarations instead of the Group Terminal Blocks.", "");
                    }

                    foreach (var groupTerminalBlock in di.GroupTerminalBlocks)
                    {
                        if (!settings.Verbose)
                        {
                            string groupInformation = string.Empty;
                            ;

                            if (groupTerminalBlock.GroupCount == 1)
                            {
                                groupInformation +=
                                    $"[grey]Group[/] {groupTerminalBlock.FirstGroupIndex + 1} (Index {groupTerminalBlock.FirstGroupIndex})";
                            }
                            else
                            {
                                int stopGroupIndex = groupTerminalBlock.FirstGroupIndex + groupTerminalBlock.GroupCount - 1;
                                groupInformation +=
                                    $"[grey]Groups[/] {groupTerminalBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} (Indexes {groupTerminalBlock.FirstGroupIndex}-{stopGroupIndex})";

                            }

                            groupInformation += ", [grey]Protocol[/] " + groupTerminalBlock.Protocol.ToString();
                            groupInformation += ", [grey]Direction[/] " + groupTerminalBlock.Direction.ToString();

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
                                    $" ({((double)groupTerminalBlock.CalculatedMaxDeviceInputBandwidthBitsPerSecond / 1000.0).ToString("N2")} kbps)";
                            }


                            if (groupTerminalBlock.MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits == 0)
                            {
                                outputBandwidth = "0 (Unknown or Not Fixed)";
                            }
                            else
                            {
                                outputBandwidth = groupTerminalBlock.MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits.ToString() +
                                    $" ({((double)groupTerminalBlock.CalculatedMaxDeviceOutputBandwidthBitsPerSecond / 1000.0).ToString("N2")} kbps)";
                            }

                            table.AddRow("Max Input Bandwidth", inputBandwidth);
                            table.AddRow("Max Output Bandwidth", outputBandwidth);
                        }
                    }
                }


                if (settings.Verbose)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("Additional Capabilities/Properties"), "");
                    //table.AddRow("Requires Note Off Translation", di.NativeDataFormat.ToString());
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

                if (settings.IncludeRaw)
                {
                    // spit out all properties by key / value in a new table

                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("All Raw Properties"), "");

                    DisplayProperties(table, di.Properties);
                }

                AnsiConsole.Write(table);

            }



            return (int)(MidiConsoleReturnCode.Success);
        }




        private void DisplayProperties(Table table, IReadOnlyDictionary<string, object> properties)
        {
            foreach (string key in properties.Keys)
            {
                object? value;
                bool found = properties.TryGetValue(key, out value);

                if (found)
                {
                    if (value != null)
                    {
                        if (key == "System.ItemNameDisplay")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatEndpointName(value.ToString()!));
                        }
                        else if (key == "System.Devices.DeviceInstanceId")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatDeviceInstanceId(value.ToString()!));
                        }
                        else if (key == "System.Devices.Parent")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatDeviceParentId(value.ToString()!));
                        }
                        else if (key == "System.Devices.ContainerId")
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatContainerId(value.ToString()!));
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
                                table.AddRow(key, AnsiMarkupFormatter.EscapeString(value.ToString()!));
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
