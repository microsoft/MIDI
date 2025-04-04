// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



//using Microsoft.Windows.Devices.Midi2.Initialization;

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
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}


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


                MidiEndpointDeviceInformation di = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(endpointId);
                if (di == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Unable to create device. It may have been disconnected."));
                    AnsiConsole.WriteLine();
                    return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
                }


                var config = di.GetDeclaredStreamConfiguration();
                var epinfo = di.GetDeclaredEndpointInfo();
                var transportSuppliedInfo = di.GetTransportSuppliedInfo();


                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertiesTableSectionHeaderIdentification), "");
                table.AddRow(Strings.PropertiesTablePropertyLabelName, AnsiMarkupFormatter.FormatEndpointName(di.Name));
                table.AddRow(Strings.PropertiesTablePropertyLabelId, AnsiMarkupFormatter.FormatFullEndpointInterfaceId(di.EndpointDeviceId));
                table.AddRow(Strings.PropertiesTablePropertyLabelPurpose, di.EndpointPurpose.ToString());


                table.AddRow(Strings.PropertiesTablePropertyLabelSerialNumber, AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.SerialNumber));
                table.AddRow(Strings.PropertiesTablePropertyLabelManufacturer, AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.ManufacturerName));

                if (transportSuppliedInfo.VendorId > 0 || transportSuppliedInfo.ProductId > 0)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelVidPid, 
                        AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.VendorId.ToString("X4")
                        + " / " + 
                        AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.ProductId.ToString("X4"))));
                }

                if (transportSuppliedInfo.Description != string.Empty)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelTransportSuppliedDescription, AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.Description));
                }

                table.AddRow(Strings.PropertiesTablePropertyLabelMultiClient, transportSuppliedInfo.SupportsMultiClient.ToString());


                // data format

                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    table.AddRow(Strings.PropertiesTablePropertyLabelNativeDataFormat, Strings.PropertyValueNativeDataFormatUmp);
                }
                else if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.Midi1ByteFormat)
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

                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    if (settings.Verbose)
                    {
                        table.AddEmptyRow();
                        table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Resources.Strings.PropertiesTableSectionHeaderEndpointMetadata), "");
                        table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionEndpointMetadata), "");
                        //table.AddRow(Strings.PropertyTablePropertyLabelEndpointInfoLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(epinfo.EndpointInformationLastUpdateTime));

                        table.AddRow(Strings.PropertiesTablePropertyLabelProductInstanceId, AnsiMarkupFormatter.EscapeString(epinfo.ProductInstanceId));
                        //table.AddRow(Strings.PropertyTablePropertyLabelProductInstanceIdLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(di.ProductInstanceIdLastUpdateTime));

                        table.AddRow(Strings.PropertiesTablePropertyLabelEndpointSuppliedName, AnsiMarkupFormatter.FormatEndpointName(epinfo.Name));
                        //table.AddRow(Strings.PropertyTablePropertyLabelEndpointSuppliedNameLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(di.EndpointSuppliedNameLastUpdateTime));

                        var identity = di.GetDeclaredDeviceIdentity();
                        table.AddRow(Strings.PropertiesTablePropertyLabelSystemExclusiveId,
                            identity.SystemExclusiveIdByte1.ToString("X2") + " " +
                            identity.SystemExclusiveIdByte2.ToString("X2") + " " +
                            identity.SystemExclusiveIdByte3.ToString("X2"));

                        table.AddRow(Strings.PropertiesTablePropertyLabelDeviceFamily,
                            identity.DeviceFamilyMsb.ToString("X2") + " " +
                            identity.DeviceFamilyLsb.ToString("X2"));

                        table.AddRow(Strings.PropertiesTablePropertyLabelDeviceFamilyModelNumber,
                            identity.DeviceFamilyModelNumberMsb.ToString("X2") + " " +
                            identity.DeviceFamilyModelNumberLsb.ToString("X2"));

                        table.AddRow(Strings.PropertiesTablePropertyLabelSoftwareRevisionLevel,
                            identity.SoftwareRevisionLevelByte1.ToString("X2") + " " +
                            identity.SoftwareRevisionLevelByte2.ToString("X2") + " " +
                            identity.SoftwareRevisionLevelByte3.ToString("X2") + " " +
                            identity.SoftwareRevisionLevelByte4.ToString("X2"));

                        //table.AddRow(Strings.PropertyTablePropertyLabelDeviceIdentityLastUpdated, AnsiMarkupFormatter.FormatLongDateTime(identity.LastUpdateTime));

                    }
                }

                var userInfo = di.GetUserSuppliedInfo();

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertiesTableSectionHeaderUserData), "");
                table.AddRow(Strings.PropertiesTablePropertyLabelUserSuppliedName, AnsiMarkupFormatter.FormatEndpointName(userInfo.Name));
                table.AddRow(Strings.PropertiesTablePropertyLabelDescription, AnsiMarkupFormatter.EscapeString(userInfo.Description));
                table.AddRow(Strings.PropertiesTablePropertyLabelSmallImagePath, AnsiMarkupFormatter.EscapeString(userInfo.SmallImagePath));
                table.AddRow(Strings.PropertiesTablePropertyLabelLargeImagePath, AnsiMarkupFormatter.EscapeString(userInfo.LargeImagePath));



                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderActiveConfiguration), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionActiveConfiguration), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelProtocol, config.Protocol.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredSendJR, config.SendJitterReductionTimestamps.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredReceiveJR, config.ReceiveJitterReductionTimestamps.ToString());

                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderDeclaredCapabilities), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionDeclaredCapabilities), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredMIDI1Protocol, epinfo.SupportsMidi10Protocol.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelConfiguredMIDI2Protocol, epinfo.SupportsMidi20Protocol.ToString());

                    if (settings.Verbose)
                    {
                        table.AddRow(Strings.PropertyTablePropertyLabelUMPVersion, epinfo.SpecificationVersionMajor + "." + epinfo.SpecificationVersionMinor);
                        table.AddRow(Strings.PropertyTablePropertyLabelSupportsSendingJR, epinfo.SupportsSendingJitterReductionTimestamps.ToString());
                        table.AddRow(Strings.PropertyTablePropertyLabelSupportsReceivingJR, epinfo.SupportsReceivingJitterReductionTimestamps.ToString());
                    }
                }

                if (settings.Verbose)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderTransportInformation), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionTransportInformation), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelTransportSuppliedName, AnsiMarkupFormatter.FormatEndpointName(transportSuppliedInfo.Name));
                    table.AddRow(Strings.PropertyTablePropertyLabelTransportId, AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.TransportId.ToString()));
                    table.AddRow(Strings.PropertyTablePropertyLabelTransportCode, AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.TransportCode));
                }

                table.AddEmptyRow();

                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderFunctionBlocks), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionFunctionBlocks), "");
                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlocksStatic, epinfo.HasStaticFunctionBlocks.ToString());
                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlocksDeclaredCount, epinfo.DeclaredFunctionBlockCount.ToString());
                    table.AddEmptyRow();

                    // show function blocks for native UMP devices

                    if (di.GetDeclaredFunctionBlocks().Count > 0)
                    {
                        foreach (var functionBlock in di.GetDeclaredFunctionBlocks())
                        {
                            if (!settings.Verbose)
                            {
                                string functionInformation = string.Empty;
                                

                                if (functionBlock.GroupCount == 1)
                                {
                                    functionInformation +=
                                        $"[grey]{Strings.CommonStringGroupSingular}[/] {functionBlock.FirstGroup.DisplayValue} ({Strings.CommonStringIndexSingular} {functionBlock.FirstGroup.Index})";
                                }
                                else
                                {
                                    int stopGroupIndex = functionBlock.FirstGroup.Index + functionBlock.GroupCount - 1;
                                    functionInformation +=
                                        $"[grey]{Strings.CommonStringGroupPlural}[/] {functionBlock.FirstGroup.DisplayValue}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {functionBlock.FirstGroup.Index}-{stopGroupIndex})";

                                }

                                functionInformation += $", [grey]{Strings.CommonStringMIDI}[/] " + functionBlock.RepresentsMidi10Connection.ToString();
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
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockFirstGroupIndex, functionBlock.FirstGroup.Index.ToString());
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockGroupCount, functionBlock.GroupCount.ToString());

                                if (functionBlock.GroupCount == 1)
                                {
                                    table.AddRow(Strings.CommonStringGroupSingular, $"{functionBlock.FirstGroup.DisplayValue} ({Strings.CommonStringIndexSingular} {functionBlock.FirstGroup.Index})");
                                }
                                else
                                {
                                    int stopGroupIndex = functionBlock.FirstGroup.Index + functionBlock.GroupCount - 1;
                                    table.AddRow(Strings.CommonStringGroupPlural, $"{functionBlock.FirstGroup.DisplayValue}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {functionBlock.FirstGroup.Index}-{stopGroupIndex})");
                                }

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
                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockMIDI10Connection, functionBlock.RepresentsMidi10Connection.ToString());
                                    table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockMIDICIVersionFormat, functionBlock.MidiCIMessageVersionFormat.ToString());
                                }



                                //table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlockGroupCount, functionBlock.GroupCount.ToString());
                            }
                        }
                    }
                }


                // Show group terminal blocks only if verbose or if there are no function blocks

                if ((settings.Verbose || di.GetDeclaredEndpointInfo().DeclaredFunctionBlockCount == 0) && di.GetGroupTerminalBlocks().Count > 0)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderGroupTerminalBlocks), "");
                    
                    if (di.GetDeclaredFunctionBlocks().Count > 0)
                    {
                        table.AddRow(Strings.PropertyTableSectionNoteGTBFunctionBlocksAlsoPresent, "");
                    }

                    foreach (var groupTerminalBlock in di.GetGroupTerminalBlocks())
                    {
                        if (!settings.Verbose)
                        {
                            string groupInformation = string.Empty;
                            ;

                            if (groupTerminalBlock.GroupCount == 1)
                            {
                                groupInformation +=
                                    $"[grey]{MidiGroup.LongLabel}[/] {groupTerminalBlock.FirstGroup.DisplayValue} ({Strings.CommonStringIndexSingular} {groupTerminalBlock.FirstGroup.Index})";
                            }
                            else
                            {
                                int stopGroupIndex = groupTerminalBlock.FirstGroup.Index + groupTerminalBlock.GroupCount - 1;
                                groupInformation +=
                                    $"[grey]{MidiGroup.LongLabelPlural}[/] {groupTerminalBlock.FirstGroup.DisplayValue}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {groupTerminalBlock.FirstGroup.Index}-{stopGroupIndex})";

                            }

                            groupInformation += $", [grey]{Strings.CommonStringProtocolSingular}[/] " + groupTerminalBlock.Protocol.ToString();

                            groupInformation += $", [grey]{Strings.CommonStringDirectionSingular}[/] " + MidiFriendlyNames.GroupTerminalBlockDirection(groupTerminalBlock.Direction);

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
                                table.AddRow(Strings.CommonStringGroupSingular, $"{groupTerminalBlock.FirstGroup.DisplayValue} ({Strings.CommonStringIndexSingular} {groupTerminalBlock.FirstGroup.Index})");
                            }
                            else
                            {
                                int stopGroupIndex = groupTerminalBlock.FirstGroup.Index + groupTerminalBlock.GroupCount - 1;
                                table.AddRow(Strings.CommonStringGroupPlural, $"{groupTerminalBlock.FirstGroup.DisplayValue}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {groupTerminalBlock.FirstGroup.Index}-{stopGroupIndex})");
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


                // Related MIDI 1.0 ports (WinMM and WinRT MIDI 1.0) -------------------------------------------------------

                table.AddEmptyRow();
                var midi1SourcePorts = di.GetAssociatedMidi1Ports(Midi1PortFlow.MidiMessageSource);
                if (midi1SourcePorts != null && midi1SourcePorts.Count > 0)
                {
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderMidi1MessageSourcePorts), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionMidi1MessageSourcePorts), "");

                    foreach (var port in midi1SourcePorts.OrderBy(p => p.Group.Index))
                    {
                        string portInformation = string.Empty;

                        portInformation = $"[grey]{MidiGroup.LongLabel}[/] " + " " + port.Group.DisplayValue + 
                            $" ({ Strings.CommonStringIndexSingular} { port.Group.Index})";

                        if (settings.Verbose)
                        {
                            portInformation += "\n" + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(port.PortDeviceId);
                        }

                        table.AddRow(
                            AnsiMarkupFormatter.FormatPortIndex(port.PortIndex) + " " +
                            AnsiMarkupFormatter.FormatPortName(port.PortName),
                            portInformation);
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableNoAssociatedMidi1SourcePorts), "");
                }

                table.AddEmptyRow();
                var midi1DestinationPorts = di.GetAssociatedMidi1Ports(Midi1PortFlow.MidiMessageDestination);
                if (midi1DestinationPorts != null && midi1DestinationPorts.Count > 0)
                {
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderMidi1MessageDestinationPorts), "");
                    table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionMidi1MessageDestinationPorts), "");

                    foreach (var port in midi1DestinationPorts.OrderBy(p => p.Group.Index))
                    {
                        string portInformation = string.Empty;

                        portInformation = $"[grey]{MidiGroup.LongLabel}[/] " + " " + port.Group.DisplayValue +
                            $" ({Strings.CommonStringIndexSingular} {port.Group.Index})";

                        if (settings.Verbose)
                        {
                            portInformation += "\n" + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(port.PortDeviceId);
                        }

                        table.AddRow(
                            AnsiMarkupFormatter.FormatPortIndex(port.PortIndex) + " " +
                            AnsiMarkupFormatter.FormatPortName(port.PortName),
                            portInformation);
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableNoAssociatedMidi1DestinationPorts), "");
                }





                // container ---------------------------------------------------------------------------------

                var containerInfo = di.GetContainerDeviceInformation();

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
                    AnsiConsole.WriteLine($"{di.Properties.Count} properties");
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
                    string friendlyKey = key;

                    if (MidiEndpointDevicePropertyHelper.IsMidiPropertyKey(key))
                    {
                        friendlyKey = AnsiMarkupFormatter.FormatFriendlyPropertyKey(MidiEndpointDevicePropertyHelper.GetMidiPropertyNameFromPropertyKey(key));
                    }
                    else
                    {
                        // not a MIDI property key, so just output the key info
                        friendlyKey = key;
                    }

                    if (value != null)
                    {
                        if (key == "System.ItemNameDisplay")
                        {
                            table.AddRow(friendlyKey, AnsiMarkupFormatter.FormatEndpointName(value.ToString()!));
                        }
                        else if (key == "System.Devices.DeviceInstanceId")
                        {
                            table.AddRow(friendlyKey, AnsiMarkupFormatter.FormatDeviceInstanceId(value.ToString()!));
                        }
                        else if (key == "System.Devices.Parent")
                        {
                            table.AddRow(friendlyKey, AnsiMarkupFormatter.FormatDeviceParentId(value.ToString()!));
                        }
                        else if (key == "System.Devices.ContainerId")
                        {
                            table.AddRow(friendlyKey, AnsiMarkupFormatter.FormatContainerId(value.ToString()!));
                        }
                        else if (value is DateTimeOffset)
                        {
                            table.AddRow(friendlyKey, AnsiMarkupFormatter.FormatLongDateTime((DateTimeOffset)value!));
                        }
                        else
                        {
                            if (value is byte[])
                            {
                                string s = string.Empty;

                                var bytes = (byte[])value;

                                foreach (byte b in bytes)
                                {
                                    if (char.IsAsciiLetterOrDigit((char)b))
                                    {
                                        s += string.Format("[grey35]{0:X2}[/][cyan]{1}[/] ", b, (char)b);
                                    }
                                    else
                                    {
                                        s += string.Format("{0:X2}  ", b);
                                    }
                                }

                                table.AddRow(friendlyKey, s);
                                table.AddRow("", $"[cyan]({bytes.Length} bytes)[/]");

                            }
                            else
                            {
                                table.AddRow(friendlyKey, AnsiMarkupFormatter.EscapeString(value.ToString()!));
                            }

                        }

                    }
                    else
                    {
                        table.AddRow(friendlyKey, "[grey35]null[/]");
                    }
                }
            }
        }




    }
}
