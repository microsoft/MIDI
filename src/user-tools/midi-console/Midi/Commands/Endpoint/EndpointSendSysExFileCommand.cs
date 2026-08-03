// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using ABI.Windows.Devices.Midi2.Utilities.Messages;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Windows.Storage;

namespace Microsoft.Midi.ConsoleApp
{

    internal class EndpointSendSysExFileCommand : AsyncCommand<EndpointSendSysExFileCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterSendSysExFileCommandFile")]
            [CommandArgument(1, "<Input File>")]
            public string? InputFile { get; set; }

            //[LocalizedDescription("ParameterSendSysExFileVerbose")]
            //[CommandOption("-v|--verbose")]
            //[DefaultValue(false)]
            //public bool Verbose { get; set; }

            //[EnumLocalizedDescription("ParameterSysExFileFormat", typeof(MidiSystemExclusiveDataReaderFormat))]
            //[CommandOption("-f|--sysex-file-type")]
            //[DefaultValue(MidiSystemExclusiveDataReaderFormat.InferFromData)]
            //public MidiSystemExclusiveDataReaderFormat FileType { get; set; }

            //[EnumLocalizedDescription("ParameterSysExDataFormat", typeof(MidiSystemExclusiveDataFormat))]
            //[CommandOption("-d|--source-data-format")]
            //[DefaultValue(MidiSystemExclusiveDataFormat.InferFromData)]
            //public MidiSystemExclusiveDataFormat SourceDataFormat { get; set; }

            [LocalizedDescription("ParameterSendSysExFileReplaceGroup")]
            [CommandOption("-g|--group-index|--group")]
            public int? NewGroupIndex { get; set; }


            [LocalizedDescription("ParameterSendSysExFileDelayBetweenTransfers")]
            [CommandOption("-p|--pause|--delay")]
            [DefaultValue(500)]
            public int DelayBetweenMessages { get; set; }

            [LocalizedDescription("ParameterSendSysExFileMessagesPerTransfer")]
            [CommandOption("-m|--message-transfer-count|--messages")]
            [DefaultValue(64)]
            public int MessageTransferCount { get; set; }

        }


        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.InputFile == null)
            {
                // TODO: Localize
                return ValidationResult.Error($"File not specified.");
            }

            if (settings.InputFile != null && !System.IO.File.Exists(settings.InputFile))
            {
                // TODO: Localize
                return ValidationResult.Error($"File not found {settings.InputFile}.");
            }

            if (settings.NewGroupIndex.HasValue)
            {
                byte newGroup = (byte)settings.NewGroupIndex.GetValueOrDefault(0);

                if (!MidiGroup.IsValidIndex(newGroup))
                {
                    return ValidationResult.Error(Strings.ValidationErrorInvalidGroup);
                }
            }
            else
            {
                return ValidationResult.Error(Strings.ValidationErrorInvalidGroup);
            }

            return base.Validate(context, settings);
        }


        public override async Task<int> ExecuteAsync(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            if (!string.IsNullOrEmpty(endpointId))
            {
                // TODO: Update loc strings
                string endpointName = EndpointUtility.GetEndpointNameFromEndpointInterfaceId(endpointId);

                AnsiConsole.Markup(Strings.SendMessageSendingThroughEndpointLabel);
                AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatEndpointName(endpointName));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointId));
                AnsiConsole.WriteLine();

                using var session = MidiSession.Create($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");
                if (session == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId);
                if (connection == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));

                    return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
                }

                bool openSuccess = openSuccess = connection.Open(); ;
                if (!openSuccess)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));

                    return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }

                var info = new FileInfo(settings.InputFile!);
                var fullFilePath = info.FullName;

                AnsiConsole.WriteLine($"{fullFilePath}");

                var file = await StorageFile.GetFileFromPathAsync(fullFilePath);
                var props = await file.GetBasicPropertiesAsync();

               // AnsiConsole.WriteLine($"{props.Size} bytes");

                if (file != null)
                {
                    using var stream = await file.OpenStreamForReadAsync();

                    var totalBytesToSend = props.Size;

                    if (stream != null)
                    {
                        //var sendTask = ctx.AddTask("[white]Transmitting binary SysEx 7 file[/]");
                        //sendTask.MaxValue = props.Size;
                        //sendTask.Value = 0;

                        var inputStream = stream.AsInputStream();

                        MidiGroup? group = null;

                        if (settings.NewGroupIndex != null)
                        {
                            group = new MidiGroup((byte)(settings.NewGroupIndex!));
                        }

                        try
                        {
                            var converterState = new global::Windows.Devices.Midi2.Utilities.Messages.MidiBytestreamToUmpMessageConverterState();

                            UInt64 countBytesRead = 0;
                            UInt64 countMessagesSent = 0;
                            bool success = false;

                            await AnsiConsole.Progress()
                                .AutoClear(true)
                                .HideCompleted(true)
                                .Columns(new ProgressColumn[]
                                {
                                    new TaskDescriptionColumn(),    // Task name
                                    new ProgressBarColumn(),        // Progress bar
                                    new PercentageColumn(),         // Percentage
                                    //new RemainingTimeColumn(),      // Estimated remaining time
                                    new SpinnerColumn()             // Spinner animation
                                })
                                .StartAsync(async ctx =>
                                {
                                    var senderTask = ctx.AddTask("[green]Transferring data[/]", maxValue: totalBytesToSend);

                                    var operation = MidiSystemExclusiveSender.SendBinarySysEx7ByteDataAsync(
                                        connection,
                                        group,
                                        inputStream,
                                        (UInt32)settings.MessageTransferCount,
                                        (UInt16)settings.DelayBetweenMessages,
                                        converterState
                                    );

                                    operation.Progress = (op, progress) =>
                                    {
                                        countBytesRead = progress.CountBytesRead;
                                        countMessagesSent = progress.CountMessagesSent;

                                        senderTask.Value = countBytesRead;
                                       // ctx.Status($"Read {countBytesRead} bytes, sent {countMessagesSent} messages");
                                    };

                                    success = await operation;
                                });

                            AnsiConsole.WriteLine();

                            if (success)
                            {
                                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Data transfer complete."));
                                AnsiConsole.MarkupLine($"Read {AnsiMarkupFormatter.FormatGeneralNumber(countBytesRead)} bytes, sent {AnsiMarkupFormatter.FormatGeneralNumber(countMessagesSent)} UMP messages");
                            }
                            else
                            {
                                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Transfer failed."));
                            }

                            AnsiConsole.WriteLine();

                        }
                        catch (Exception ex)
                        {
                            LoggingService.Current.LogError("Exception sending SysEx data", ex);

                            AnsiConsole.WriteLine($"Exception sending data: {ex.Message}");
                        }
                    }
                }

                return (int)MidiConsoleReturnCode.Success;

            }
            else
            {
                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }

        }






    }
}
