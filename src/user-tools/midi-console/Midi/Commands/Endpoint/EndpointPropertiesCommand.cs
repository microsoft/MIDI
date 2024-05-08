// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
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
            if (!MidiService.IsAvailable())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }


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

                TableColumn column1 = new TableColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertiesTableColumnHeaderProperty));
                column1.Width(45);

                table.AddColumn(column1);

                TableColumn column2 = new TableColumn(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertiesTableColumnHeaderValue));

                //Console.WriteLine("Window width " + Console.WindowWidth);

                if (Console.WindowWidth > 140)
                {
                    column2.NoWrap = true;
                }

                table.AddColumn(column2);


                MidiEndpointDeviceInformation di = MidiEndpointDeviceInformation.CreateFromId(endpointId);

                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertiesTableSectionHeaderIdentification), "");
                table.AddRow(Strings.PropertiesTablePropertyLabelName, AnsiMarkupFormatter.FormatEndpointName(di.Name));
                table.AddRow(Strings.PropertiesTablePropertyLabelId, AnsiMarkupFormatter.FormatFullEndpointInterfaceId(di.Id));
                table.AddRow(Strings.PropertiesTablePropertyLabelPurpose, di.EndpointPurpose.ToString());

                table.AddRow(Strings.PropertiesTablePropertyLabelSerialNumber, AnsiMarkupFormatter.EscapeString(di.TransportSuppliedSerialNumber));
                table.AddRow(Strings.PropertiesTablePropertyLabelManufacturer, AnsiMarkupFormatter.EscapeString(di.ManufacturerName));

                if (di.TransportSuppliedVendorId > 0 || di.TransportSuppliedProductId > 0)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelVidPid, 
                        AnsiMarkupFormatter.EscapeString(di.TransportSuppliedVendorId.ToString("X4")
                        + " / " + 
                        AnsiMarkupFormatter.EscapeString(di.TransportSuppliedProductId.ToString("X4"))));
                }

                if (di.TransportSuppliedDescription != string.Empty)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelTransportSuppliedDescription, AnsiMarkupFormatter.EscapeString(di.TransportSuppliedDescription));
                }

                table.AddRow(Strings.PropertiesTablePropertyLabelMultiClient, di.SupportsMultiClient.ToString());


                // data format

                if (di.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacket)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelNativeDataFormat, Strings.PropertyValueNativeDataFormatUmp);
                }
                else if (di.NativeDataFormat == MidiEndpointNativeDataFormat.ByteStream)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelNativeDataFormat, Strings.PropertyValueNativeDataFormatByteStream);
                }
                else
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelNativeDataFormat, Strings.PropertyValueNativeDataFormatUnknown);
                }





                if (settings.Verbose)
                {
                    //table.AddRow("Kind", AnsiMarkupFormatter.FormatDeviceKind(di.DeviceInformation.Kind));
                    table.AddRow(Strings.PropertiesTablePropertyLabelContainerId, AnsiMarkupFormatter.FormatContainerId(di.ContainerId.ToString()));
                    table.AddRow(Strings.PropertiesTablePropertyLabelDeviceInstanceId, AnsiMarkupFormatter.FormatDeviceInstanceId(di.DeviceInstanceId));
                }

                // we only show protocol negotiation stuff if we're native UMP

                if (di.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacket)
                {
                    if (settings.Verbose)
                    {
                        table.AddEmptyRow();
                        table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableSectionHeaderEndpointMetadata), "");
                        table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionEndpointMetadata), "");
                        table.AddRow(Strings.PropertyTablePropertyLabelEndpointInfoLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(di.EndpointInformationLastUpdateTime));

                        table.AddRow(Strings.PropertiesTablePropertyLabelProductInstanceId, AnsiMarkupFormatter.EscapeString(di.ProductInstanceId));
                        table.AddRow(Strings.PropertyTablePropertyLabelProductInstanceIdLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(di.ProductInstanceIdLastUpdateTime));

                        table.AddRow(Strings.PropertiesTablePropertyLabelEndpointSuppliedName, AnsiMarkupFormatter.FormatEndpointName(di.EndpointSuppliedName));
                        table.AddRow(Strings.PropertyTablePropertyLabelEndpointSuppliedNameLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(di.EndpointSuppliedNameLastUpdateTime));


                        table.AddRow(Strings.PropertiesTablePropertyLabelSystemExclusiveId,
                            di.DeviceIdentitySystemExclusiveId[0].ToString("X2") + " " +
                            di.DeviceIdentitySystemExclusiveId[1].ToString("X2") + " " +
                            di.DeviceIdentitySystemExclusiveId[2].ToString("X2"));

                        table.AddRow(Strings.PropertiesTablePropertyLabelDeviceFamily,
                            di.DeviceIdentityDeviceFamilyMsb.ToString("X2") + " " +
                            di.DeviceIdentityDeviceFamilyLsb.ToString("X2"));

                        table.AddRow(Strings.PropertiesTablePropertyLabelDeviceFamilyModelNumber,
                            di.DeviceIdentityDeviceFamilyModelNumberMsb.ToString("X2") + " " +
                            di.DeviceIdentityDeviceFamilyModelNumberLsb.ToString("X2"));

                        table.AddRow(Strings.PropertiesTablePropertyLabelSoftwareRevisionLevel,
                            di.DeviceIdentitySoftwareRevisionLevel[0].ToString("X2") + " " +
                            di.DeviceIdentitySoftwareRevisionLevel[1].ToString("X2") + " " +
                            di.DeviceIdentitySoftwareRevisionLevel[2].ToString("X2") + " " +
                            di.DeviceIdentitySoftwareRevisionLevel[3].ToString("X2"));

                        table.AddRow(Strings.PropertyTablePropertyLabelDeviceIdentityLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(di.DeviceIdentityLastUpdateTime));

                    }
                }

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertiesTableSectionHeaderUserData), "");
                table.AddRow(Strings.PropertiesTablePropertyLabelUserSuppliedName, AnsiMarkupFormatter.FormatEndpointName(di.UserSuppliedName));
                table.AddRow(Strings.PropertiesTablePropertyLabelDescription, AnsiMarkupFormatter.EscapeString(di.UserSuppliedDescription));
                table.AddRow(Strings.PropertiesTablePropertyLabelSmallImagePath, AnsiMarkupFormatter.EscapeString(di.UserSuppliedSmallImagePath));
                table.AddRow(Strings.PropertiesTablePropertyLabelLargeImagePath, AnsiMarkupFormatter.EscapeString(di.UserSuppliedLargeImagePath));



                if (di.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacket)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderActiveConfiguration), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionActiveConfiguration), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelProtocol, di.ConfiguredProtocol.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredSendJR, di.ConfiguredToSendJRTimestamps.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredReceiveJR, di.ConfiguredToReceiveJRTimestamps.ToString());

                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderDeclaredCapabilities), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionDeclaredCapabilities), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredMIDI1Protocol, di.SupportsMidi10Protocol.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredMIDI2Protocol, di.SupportsMidi20Protocol.ToString());

                    if (settings.Verbose)
                    {
                        table.AddRow(Strings.PropertyTablePropertyLabelUMPVersion, di.SpecificationVersionMajor + "." + di.SpecificationVersionMinor);
                        table.AddRow(Strings.PropertyTablePropertyLabelSupportsSendingJR, di.SupportsSendingJRTimestamps.ToString());
                        table.AddRow(Strings.PropertyTablePropertyLabelSupportsReceivingJR, di.SupportsReceivingJRTimestamps.ToString());
                    }
                }

                if (settings.Verbose)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderTransportInformation), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionTransportInformation), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelTransportSuppliedName, AnsiMarkupFormatter.FormatEndpointName(di.TransportSuppliedName));
                    table.AddRow(Strings.PropertyTablePropertyLabelTransportId, AnsiMarkupFormatter.EscapeString(di.TransportId.ToString()));
                    table.AddRow(Strings.PropertyTablePropertyLabelTransportMnemonic, AnsiMarkupFormatter.EscapeString(di.TransportMnemonic));
                }

                table.AddEmptyRow();

                if (di.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacket)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderFunctionBlocks), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionFunctionBlocks), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlocksStatic, di.HasStaticFunctionBlocks.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlocksDeclaredCount, di.DeclaredFunctionBlockCount.ToString());
                    table.AddEmptyRow();

                    if (di.FunctionBlocks.Count > 0)
                    {
                        foreach (var functionBlock in di.FunctionBlocks.Values)
                        {
                            if (!settings.Verbose)
                            {
                                string functionInformation = string.Empty;
                                

                                if (functionBlock.GroupCount == 1)
                                {
                                    functionInformation +=
                                        $"[grey]{Strings.CommonStringGroupSingular}[/] {functionBlock.FirstGroupIndex + 1} ({Strings.CommonStringIndexSingular} {functionBlock.FirstGroupIndex})";
                                }
                                else
                                {
                                    int stopGroupIndex = functionBlock.FirstGroupIndex + functionBlock.GroupCount - 1;
                                    functionInformation +=
                                        $"[grey]{Strings.CommonStringGroupPlural}[/] {functionBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {functionBlock.FirstGroupIndex}-{stopGroupIndex})";

                                }

                                functionInformation += $", [grey]{Strings.CommonStringMIDI}[/] " + functionBlock.Midi10Connection.ToString();
                                functionInformation += $", [grey]{Strings.CommonStringDirectionSingular}[/] " + functionBlock.Direction.ToString();
                                functionInformation += $", [grey]{Strings.PropertyTablePropertyLabelFunctionBlockUIHint}[/] " + functionBlock.UIHint.ToString();

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
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlock, AnsiMarkupFormatter.FormatBlockNumber(functionBlock.Number) + " " + AnsiMarkupFormatter.FormatBlockName(functionBlock.Name));
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockActive, functionBlock.IsActive.ToString());
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockFirstGroupIndex, functionBlock.FirstGroupIndex.ToString());
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockGroupCount, functionBlock.GroupCount.ToString());

                                if (settings.Verbose)
                                {
                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockDirection, functionBlock.Direction.ToString());
                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockUIHint, functionBlock.UIHint.ToString());

                                    string sysexStreams = string.Empty;
                                    if (functionBlock.MaxSystemExclusive8Streams == 0)
                                    {
                                        sysexStreams = Strings.PropertyTablePropertyValueFunctionBlockSysEx8NotSupported;
                                    }
                                    else if (functionBlock.MaxSystemExclusive8Streams == 1)
                                    {
                                        sysexStreams = Strings.PropertyTablePropertyValueFunctionBlockSysEx8SingleStreamSupported;
                                    }
                                    else
                                    {
                                        sysexStreams = functionBlock.MaxSystemExclusive8Streams + " " + Strings.PropertyTablePropertyValueSuffixFunctionBlockMultipleSysexStreams;
                                    }

                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockMaxSysEx8Streams, sysexStreams);
                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockMIDI10Connection, functionBlock.Midi10Connection.ToString());
                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockMIDICIVersionFormat, functionBlock.MidiCIMessageVersionFormat.ToString());
                                }


                                if (functionBlock.GroupCount == 1)
                                {
                                    table.AddRow(Strings.CommonStringGroupSingular, $"{functionBlock.FirstGroupIndex + 1} ({Strings.CommonStringIndexSingular} {functionBlock.FirstGroupIndex})");
                                }
                                else
                                {
                                    int stopGroupIndex = functionBlock.FirstGroupIndex + functionBlock.GroupCount - 1;
                                    table.AddRow(Strings.CommonStringGroupPlural, $"{functionBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {functionBlock.FirstGroupIndex}-{stopGroupIndex})");
                                }

                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockGroupCount, functionBlock.GroupCount.ToString());
                            }
                        }
                    }
                }

                // Show group terminal blocks only if verbose or if there are no function blocks

                if ((settings.Verbose || di.DeclaredFunctionBlockCount == 0) && di.GroupTerminalBlocks.Count > 0)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderGroupTerminalBlocks), "");
                    
                    if (di.FunctionBlocks.Count > 0)
                    {
                        table.AddRow(Strings.PropertyTableSectionNoteGTBFunctionBlocksAlsoPresent, "");
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
                                    $"[grey]{Strings.CommonStringGroupSingular}[/] {groupTerminalBlock.FirstGroupIndex + 1} ({Strings.CommonStringIndexSingular} {groupTerminalBlock.FirstGroupIndex})";
                            }
                            else
                            {
                                int stopGroupIndex = groupTerminalBlock.FirstGroupIndex + groupTerminalBlock.GroupCount - 1;
                                groupInformation +=
                                    $"[grey]{Strings.CommonStringGroupPlural}[/] {groupTerminalBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {groupTerminalBlock.FirstGroupIndex}-{stopGroupIndex})";

                            }

                            groupInformation += $", [grey]{Strings.CommonStringProtocolSingular}[/] " + groupTerminalBlock.Protocol.ToString();
                            groupInformation += $", [grey]{Strings.CommonStringDirectionSingular}[/] " + groupTerminalBlock.Direction.ToString();

                            table.AddRow(
                                AnsiMarkupFormatter.FormatBlockNumber(groupTerminalBlock.Number) + " " +
                                AnsiMarkupFormatter.FormatBlockName(groupTerminalBlock.Name),
                                groupInformation);
                        }
                        else
                        {
                            table.AddEmptyRow();

                            table.AddRow(Strings.CommonStringGroupTerminalBlockSingular, AnsiMarkupFormatter.FormatBlockNumber(groupTerminalBlock.Number) + " " + AnsiMarkupFormatter.FormatBlockName(groupTerminalBlock.Name));

                            if (groupTerminalBlock.GroupCount == 1)
                            {
                                table.AddRow(Strings.CommonStringGroupSingular, $"{groupTerminalBlock.FirstGroupIndex + 1} ({Strings.CommonStringIndexSingular} {groupTerminalBlock.FirstGroupIndex})");
                            }
                            else
                            {
                                int stopGroupIndex = groupTerminalBlock.FirstGroupIndex + groupTerminalBlock.GroupCount - 1;
                                table.AddRow(Strings.CommonStringGroupPlural, $"{groupTerminalBlock.FirstGroupIndex + 1}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {groupTerminalBlock.FirstGroupIndex}-{stopGroupIndex})");
                            }

                            table.AddRow(Strings.PropertyTablePropertyLabelGTBGroupCount, groupTerminalBlock.GroupCount.ToString());
                            table.AddRow(Strings.CommonStringDirectionSingular, groupTerminalBlock.Direction.ToString());
                            table.AddRow(Strings.CommonStringProtocolSingular, groupTerminalBlock.Protocol.ToString());

                            string inputBandwidth = string.Empty;
                            string outputBandwidth = string.Empty;

                            if (groupTerminalBlock.MaxDeviceInputBandwidthIn4KBitsPerSecondUnits == 0)
                            {
                                inputBandwidth = Strings.PropertyTablePropertyValueGTBInputBandwidthUnknownOrNotFixed;
                            }
                            else
                            {
                                inputBandwidth = groupTerminalBlock.MaxDeviceInputBandwidthIn4KBitsPerSecondUnits.ToString() +
                                    $" ({((double)groupTerminalBlock.CalculatedMaxDeviceInputBandwidthBitsPerSecond / 1000.0).ToString("N2")} {Strings.CommonStringKBPS})";
                            }


                            if (groupTerminalBlock.MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits == 0)
                            {
                                outputBandwidth = Strings.PropertyTablePropertyValueGTBInputBandwidthUnknownOrNotFixed;
                            }
                            else
                            {
                                outputBandwidth = groupTerminalBlock.MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits.ToString() +
                                    $" ({((double)groupTerminalBlock.CalculatedMaxDeviceOutputBandwidthBitsPerSecond / 1000.0).ToString("N2")} {Strings.CommonStringKBPS})";
                            }

                            table.AddRow(Strings.PropertyTablePropertyLabelGTBMaxInputBandwidth, inputBandwidth);
                            table.AddRow(Strings.PropertyTablePropertyLabelGTBMaxOutputBandwidth, outputBandwidth);
                        }
                    }
                }


                if (settings.Verbose)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderAdditionalProperties), "");
                    //table.AddRow("Requires Note Off Translation", di.NativeDataFormat.ToString());
                }



                // container ---------------------------------------------------------------------------------

                var containerInfo = di.GetContainerInformation();

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderContainer), "");
                table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionContainer), "");

                if (containerInfo != null)
                {

                    table.AddRow(Strings.PropertyTablePropertyLabelContainerName, AnsiMarkupFormatter.FormatEndpointName(containerInfo.Name));
                    table.AddRow(Strings.PropertyTablePropertyLabelContainerId, AnsiMarkupFormatter.FormatContainerId(containerInfo.Id));
                    table.AddRow(Strings.PropertyTablePropertyLabelContainerKind, AnsiMarkupFormatter.FormatDeviceKind(containerInfo.Kind));

                    if (settings.Verbose)
                    {
                        DisplayProperties(table, containerInfo.Properties);
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatError(Strings.PropertyTableErrorNoMatchingContainer), "");
                }

                // parent -------------------------------------------------------------------------------------

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderParentDevice), "");
                table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionParentDevice), "");

                if (containerInfo != null)
                {
                    var parentInfo = di.GetParentDeviceInformation();

                    if (parentInfo != null)
                    {

                        table.AddRow(Strings.PropertyTablePropertyLabelParentName, AnsiMarkupFormatter.FormatEndpointName(parentInfo.Name));
                        table.AddRow(Strings.PropertyTablePropertyLabelParentId, AnsiMarkupFormatter.FormatDeviceInstanceId(parentInfo.Id));
                        table.AddRow(Strings.PropertyTablePropertyLabelParentKind, AnsiMarkupFormatter.FormatDeviceKind(parentInfo.Kind));

                        if (settings.Verbose)
                        {
                            DisplayProperties(table, parentInfo.Properties);
                        }
                    }
                    else
                    {
                        table.AddRow(AnsiMarkupFormatter.FormatError(Strings.PropertyTableErrorNoMatchingParent), "");
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatError(Strings.PropertyTableErrorNoMatchingContainer), "");
                }

                // write out the properties table

                AnsiConsole.Write(table);


                if (settings.IncludeRaw)
                {
                    AnsiConsole.WriteLine();
                    AnsiConsole.WriteLine(Strings.PropertyTableHeaderRawProperties);
                    AnsiConsole.WriteLine();

                    // spit out all properties by key / value in a new table
                    var rawPropertyTable = new Table();

                    AnsiMarkupFormatter.SetTableBorderStyle(rawPropertyTable);

                    TableColumn rawColumn1 = new TableColumn(
                        AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableColumnHeaderProperty));

                    rawPropertyTable.AddColumn(rawColumn1);

                    TableColumn rawColumn2 = new TableColumn(
                        AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableColumnHeaderValue));

                    rawPropertyTable.AddColumn(rawColumn2);

                    //rawPropertyTable.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading("All Raw Properties"), "");

                    DisplayProperties(rawPropertyTable, di.Properties);

                    AnsiConsole.Write(rawPropertyTable);
                }


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
                        else if (value is DateTimeOffset)
                        {
                            table.AddRow(key, AnsiMarkupFormatter.FormatLongDateTime((DateTimeOffset)value!));
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
