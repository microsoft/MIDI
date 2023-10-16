﻿using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class SendMessageCommand : Command<SendMessageCommand.Settings>
    {
        public sealed class Settings : SendMessageCommandSettings
        {
            // Would be better to remove this and just do an enumeration lookup to see what type of endpoint it is

            [LocalizedDescription("ParameterSendMessageWords")]
            [CommandArgument(1, "<MIDI Words>")]
            public UInt32[]? Words { get; set; }

            [LocalizedDescription("ParameterSendMessageCount")]
            [CommandOption("-c|--count")]
            [DefaultValue(1)]
            public int Count { get; set; }

        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.Words == null)
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooFewWords);
            }
            if (settings.Words.Length == 0)
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooFewWords);
            }
            else if (settings.Words.Length > 4)
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorTooManyWords);
            }
            else if (!ValidateMessage(settings.Words))
            {
                return Spectre.Console.ValidationResult.Error(Strings.MessageValidationErrorInvalidUmp);
            }


            if (settings.Count < 1)
            {
                return Spectre.Console.ValidationResult.Error(Strings.ValidationErrorInvalidMessageCount);
            }

            return base.Validate(context, settings);
        }

        // this should be moved to a base class
        private bool ValidateMessage(UInt32[] words)
        {
            if (words != null && words.Length > 0 && words.Length <= 4)
            {
                // allowed behavior is to cast the packet type to the word count
                return (bool)((int)MidiMessageUtility.GetPacketTypeFromFirstMessageWord(words[0]) == words.Length);
            }
            else
            {
                return false;
            }
        }

        private void SendSingleMessage(IMidiOutputConnection endpoint, UInt32[] words)
        {

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickOutput();
            }

            AnsiConsole.MarkupLine(Strings.SendMessageSendingThroughEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.WriteLine();

            bool openSuccess = false;

            // when this goes out of scope, it will dispose of the session, which closes the connections
            using var session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");

            var bidiOpenOptions = new MidiBidirectionalEndpointOpenOptions();


            if (settings.AutoProtocolNegotiation == false)
            {
                bidiOpenOptions.DisableAutomaticStreamConfiguration = true;
            }

            if (settings.AutoDiscovery == false)
            {
                bidiOpenOptions.DisableAutomaticEndpointMetadataHandling = true;
                bidiOpenOptions.DisableAutomaticFunctionBlockMetadataHandling = true;
            }




            if (session == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }

            using var connection = session.ConnectBidirectionalEndpoint(endpointId, bidiOpenOptions);

            if (connection != null)
            {
                openSuccess = connection.Open();
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));
                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }

            if (!openSuccess)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));
                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }

            AnsiConsole.Progress()
                .Start(ctx =>
                {
                    var sendTask = ctx.AddTask("[white]Sending messages[/]");
                    sendTask.MaxValue = settings.Count;
                    sendTask.Value = 0;

                    uint messagesSent = 0;

                    while (messagesSent < settings.Count)
                    {
                        UInt64 timestamp = MidiClock.GetMidiTimestamp();
                        connection.SendMessageWordArray(timestamp, settings.Words, 0, (byte)settings.Words.Count());

                        messagesSent++;
                        sendTask.Value = messagesSent;

                        ctx.Refresh();

                        Thread.Sleep(settings.DelayBetweenMessages);
                    }
                });

            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
