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


        private string FormatFieldLabel(string label)
        {
            return "[grey23]>[/] " + label + "";
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
                if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionIdentification), "");
                table.AddEmptyRow();
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelName), AnsiMarkupFormatter.FormatEndpointName(di.Name));
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelId), AnsiMarkupFormatter.FormatFullEndpointInterfaceId(di.EndpointDeviceId));
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelPurpose), MidiFriendlyNames.DevicePurpose(di.EndpointPurpose));


                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelSerialNumber), AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.SerialNumber));
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelManufacturer), AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.ManufacturerName));

                if (transportSuppliedInfo.VendorId > 0 || transportSuppliedInfo.ProductId > 0)
                {
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelVidPid), 
                        AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.VendorId.ToString("X4")
                        + " / " + 
                        AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.ProductId.ToString("X4"))));
                }

                if (transportSuppliedInfo.Description != string.Empty)
                {
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelTransportSuppliedDescription), AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.Description));
                }

                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelMultiClient), transportSuppliedInfo.SupportsMultiClient.ToString());


                // data format

                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelNativeDataFormat), Strings.PropertyValueNativeDataFormatUmp);
                }
                else if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.Midi1ByteFormat)
                {
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelNativeDataFormat), Strings.PropertyValueNativeDataFormatByteStream);
                }
                else
                {
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelNativeDataFormat), Strings.PropertyValueNativeDataFormatUnknown);
                }





                if (settings.Verbose)
                {
                    //table.AddRow("Kind", AnsiMarkupFormatter.FormatDeviceKind(di.DeviceInformation.Kind));
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelContainerId), AnsiMarkupFormatter.FormatContainerId(di.ContainerId.ToString()));
                    table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelDeviceInstanceId), AnsiMarkupFormatter.FormatDeviceInstanceId(di.DeviceInstanceId));
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
                if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionUserData), "");
                table.AddEmptyRow();
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelUserSuppliedName), AnsiMarkupFormatter.FormatEndpointName(userInfo.Name));
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelDescription), AnsiMarkupFormatter.EscapeString(userInfo.Description));
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelSmallImagePath), AnsiMarkupFormatter.EscapeString(userInfo.SmallImagePath));
                table.AddRow(FormatFieldLabel(Strings.PropertiesTablePropertyLabelLargeImagePath), AnsiMarkupFormatter.EscapeString(userInfo.LargeImagePath));



                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderActiveConfiguration), "");
                    if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionActiveConfiguration), "");
                    table.AddEmptyRow();
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelProtocol), config.Protocol.ToString());
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelConfiguredSendJR), config.SendJitterReductionTimestamps.ToString());
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelConfiguredReceiveJR), config.ReceiveJitterReductionTimestamps.ToString());

                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderDeclaredCapabilities), "");
                    if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionDeclaredCapabilities), "");
                    table.AddEmptyRow();
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelConfiguredMIDI1Protocol), epinfo.SupportsMidi10Protocol.ToString());
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelConfiguredMIDI2Protocol), epinfo.SupportsMidi20Protocol.ToString());

                    if (settings.Verbose)
                    {
                        table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelUMPVersion), epinfo.SpecificationVersionMajor + "." + epinfo.SpecificationVersionMinor);
                        table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelSupportsSendingJR), epinfo.SupportsSendingJitterReductionTimestamps.ToString());
                        table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelSupportsReceivingJR), epinfo.SupportsReceivingJitterReductionTimestamps.ToString());
                    }
                }

                if (settings.Verbose)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderTransportInformation), "");
                    if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionTransportInformation), "");
                    table.AddEmptyRow();
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelTransportSuppliedName), AnsiMarkupFormatter.FormatEndpointName(transportSuppliedInfo.Name));
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelTransportId), AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.TransportId.ToString()));
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelTransportCode), AnsiMarkupFormatter.EscapeString(transportSuppliedInfo.TransportCode));
                }

                // Function Blocks --------------------------
                if (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat)
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderFunctionBlocks), "");
                    if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionFunctionBlocks), "");
                    table.AddEmptyRow();
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelFunctionBlocksStatic), epinfo.HasStaticFunctionBlocks.ToString());
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelFunctionBlocksDeclaredCount), epinfo.DeclaredFunctionBlockCount.ToString());
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

                                table.AddRow(FormatFieldLabel(
                                    AnsiMarkupFormatter.FormatBlockNumberLabel(functionBlock.Number) + " " +
                                    AnsiMarkupFormatter.FormatBlockName(functionBlock.Name) +
                                    active),
                                    functionInformation);
                            }
                            else
                            {
                                table.AddEmptyRow();
                                table.AddRow(Strings.PropertyTablePropertyLabelFunctionBlock, AnsiMarkupFormatter.FormatBlockNumberValue(functionBlock.Number) + " " + AnsiMarkupFormatter.FormatBlockName(functionBlock.Name));
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


                // Group Terminal Blocks --------------------------
                // Show group terminal blocks only if verbose or if there are no function blocks

                if ((settings.Verbose || (transportSuppliedInfo.NativeDataFormat == MidiEndpointNativeDataFormat.UniversalMidiPacketFormat && di.GetDeclaredEndpointInfo().DeclaredFunctionBlockCount == 0))
                    && di.GetGroupTerminalBlocks().Count > 0
                    )
                {
                    table.AddEmptyRow();
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderGroupTerminalBlocks), "");
                    if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionGroupTerminalBlocks), "");
                    table.AddEmptyRow();

                    if (di.GetDeclaredFunctionBlocks().Count > 0)
                    {
                        table.AddRow(FormatFieldLabel(Strings.PropertyTableSectionNoteGTBFunctionBlocksAlsoPresent), "");
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

                            table.AddRow(FormatFieldLabel(
                                AnsiMarkupFormatter.FormatBlockNumberLabel(groupTerminalBlock.Number) + " " +
                                AnsiMarkupFormatter.FormatBlockName(groupTerminalBlock.Name)),
                                groupInformation);
                        }
                        else
                        {
                            table.AddRow(FormatFieldLabel(Strings.CommonStringGroupTerminalBlockSingular), AnsiMarkupFormatter.FormatBlockNumberValue(groupTerminalBlock.Number) + " " + AnsiMarkupFormatter.FormatBlockName(groupTerminalBlock.Name));

                            if (groupTerminalBlock.GroupCount == 1)
                            {
                                table.AddRow(FormatFieldLabel(MidiGroup.LongLabel), $"{groupTerminalBlock.FirstGroup.DisplayValue} ({Strings.CommonStringIndexSingular} {groupTerminalBlock.FirstGroup.Index})");
                            }
                            else
                            {
                                int stopGroupIndex = groupTerminalBlock.FirstGroup.Index + groupTerminalBlock.GroupCount - 1;
                                table.AddRow(FormatFieldLabel(MidiGroup.LongLabelPlural), $"{groupTerminalBlock.FirstGroup.DisplayValue}-{stopGroupIndex + 1} ({Strings.CommonStringIndexPlural} {groupTerminalBlock.FirstGroup.Index}-{stopGroupIndex})");
                            }

                            table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelGTBGroupCount), groupTerminalBlock.GroupCount.ToString());
                            table.AddRow(FormatFieldLabel(Strings.CommonStringDirectionSingular), groupTerminalBlock.Direction.ToString());
                            table.AddRow(FormatFieldLabel(Strings.CommonStringProtocolSingular), groupTerminalBlock.Protocol.ToString());

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

                            table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelGTBMaxInputBandwidth), inputBandwidth);
                            table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelGTBMaxOutputBandwidth), outputBandwidth);

                            table.AddEmptyRow();

                        }
                    }
                }



                // Related MIDI 1.0 ports (WinMM and WinRT MIDI 1.0) -------------------------------------------------------

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderMidi1Ports), "");
                if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionMidi1Ports), "");
                table.AddEmptyRow();

                var midi1DestinationPorts = di.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageDestination);
                if (midi1DestinationPorts != null && midi1DestinationPorts.Count > 0)
                {
                    foreach (var port in midi1DestinationPorts.OrderBy(p => p.Group.Index))
                    {
                        DisplayMidi1PortInformation(table, port, settings.Verbose);
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableNoAssociatedMidi1DestinationPorts), "");
                }

                var midi1SourcePorts = di.FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow.MidiMessageSource);
                if (midi1SourcePorts != null && midi1SourcePorts.Count > 0)
                {
                    foreach (var port in midi1SourcePorts.OrderBy(p => p.Group.Index))
                    {
                        DisplayMidi1PortInformation(table, port, settings.Verbose);
                    }
                }
                else
                {
                    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableNoAssociatedMidi1SourcePorts), "");
                }


                // Additional Properties -----------------------------------

                //if (settings.Verbose)
                //{
                //    table.AddEmptyRow();
                //    table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderAdditionalProperties), "");
                //    //table.AddRow("Requires Note Off Translation", di.NativeDataFormat.ToString());
                //}



                // container ---------------------------------------------------------------------------------

                var containerInfo = di.GetContainerDeviceInformation();

                table.AddEmptyRow();
                table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderContainer), "");
                if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionContainer), "");
                table.AddEmptyRow();

                if (containerInfo != null)
                {
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelContainerName), AnsiMarkupFormatter.FormatEndpointName(containerInfo.Name));
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelContainerId), AnsiMarkupFormatter.FormatContainerId(containerInfo.Id));
                    table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelContainerKind), AnsiMarkupFormatter.FormatDeviceKind(containerInfo.Kind));

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
                if (settings.Verbose) table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionParentDevice), "");
                table.AddEmptyRow();

                if (containerInfo != null)
                {
                    var parentInfo = di.GetParentDeviceInformation();

                    if (parentInfo != null)
                    {

                        table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelParentName), AnsiMarkupFormatter.FormatEndpointName(parentInfo.Name));
                        table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelParentId), AnsiMarkupFormatter.FormatDeviceInstanceId(parentInfo.Id));
                        table.AddRow(FormatFieldLabel(Strings.PropertyTablePropertyLabelParentKind), AnsiMarkupFormatter.FormatDeviceKind(parentInfo.Kind));

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
                    rawColumn1.Width(50);

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


        private void DisplayMidi1PortInformation(Table table, MidiEndpointAssociatedPortDeviceInformation port, bool verbose)
        {
            string portInformation = string.Empty;

            portInformation = $"[grey]{MidiGroup.LongLabel}[/]" + " " + port.Group.DisplayValue +
                $" ({Strings.CommonStringIndexSingular} {port.Group.Index})";

            portInformation += $", [grey]{Strings.CommonStringDirectionSingular}[/] " + MidiFriendlyNames.Midi1PortDirection(port.PortFlow);

            if (verbose)
            {
                portInformation += "\n" + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(port.PortDeviceId);
            }

            table.AddRow(FormatFieldLabel(
                AnsiMarkupFormatter.FormatPortIndex(port.PortNumber) + " " +
                AnsiMarkupFormatter.FormatPortName(port.PortName)),
                portInformation);
        }

        private void DisplayProperties(Table table, IReadOnlyDictionary<string, object> properties)
        {
            bool midiHeaderShown = false;

            foreach (string key in properties.Keys)
            {
                object? value;
                bool found = properties.TryGetValue(key, out value);

                if (found)
                {
                    string friendlyKey = key;

                    if (MidiEndpointDevicePropertyHelper.IsMidiPropertyKey(key))
                    {
                        friendlyKey = MidiEndpointDevicePropertyHelper.GetMidiPropertyNameFromPropertyKey(key);

                        // remove the "STRING_" frm the beginning of the property key, just to make things more compact
                        // the prefix is needed when actually using properties, but not for display.
                        string unnecessaryPrefix = "STRING_PKEY_MIDI_";
                        if (friendlyKey.StartsWith(unnecessaryPrefix))
                        {
                            friendlyKey = friendlyKey.Substring(unnecessaryPrefix.Length);
                        }

                        friendlyKey = AnsiMarkupFormatter.FormatFriendlyPropertyKey(friendlyKey);

                        if (!midiHeaderShown)
                        {
                            table.AddEmptyRow();
                            table.AddRow(AnsiMarkupFormatter.FormatTableColumnHeading(Strings.PropertyTableSectionHeaderMidiProperties), "");
                            table.AddRow(AnsiMarkupFormatter.FormatPropertySectionDescription(Strings.PropertyTableSectionDescriptionMidiProperties), "");
                            table.AddEmptyRow();

                            midiHeaderShown = true;
                        }
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
                            table.AddRow(FormatFieldLabel(friendlyKey), AnsiMarkupFormatter.FormatEndpointName(value.ToString()!));
                        }
                        else if (key == "System.Devices.DeviceInstanceId")
                        {
                            table.AddRow(FormatFieldLabel(friendlyKey), AnsiMarkupFormatter.FormatDeviceInstanceId(value.ToString()!));
                        }
                        else if (key == "System.Devices.Parent")
                        {
                            table.AddRow(FormatFieldLabel(friendlyKey), AnsiMarkupFormatter.FormatDeviceParentId(value.ToString()!));
                        }
                        else if (key == "System.Devices.ContainerId")
                        {
                            table.AddRow(FormatFieldLabel(friendlyKey), AnsiMarkupFormatter.FormatContainerId(value.ToString()!));
                        }
                        else if (value is DateTimeOffset)
                        {
                            table.AddRow(FormatFieldLabel(friendlyKey), AnsiMarkupFormatter.FormatLongDateTime((DateTimeOffset)value!));
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

                                table.AddRow(FormatFieldLabel(friendlyKey), s);
                                table.AddRow("", $"[cyan]({bytes.Length} bytes)[/]");

                            }
                            else
                            {
                                table.AddRow(FormatFieldLabel(friendlyKey), AnsiMarkupFormatter.EscapeString(value.ToString()!));
                            }

                        }

                    }
                    else
                    {
                        table.AddRow(FormatFieldLabel(friendlyKey), "[grey35]null[/]");
                    }
                }
            }
        }




    }
}
