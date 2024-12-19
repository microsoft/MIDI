using Microsoft.Windows.Devices.Midi2.Messages;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointPlayNotesCommand : Command<EndpointPlayNotesCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            // Would be better to remove this and just do an enumeration lookup to see what type of endpoint it is

            [LocalizedDescription("ParameterPlayNotesIndexes")]
            [CommandArgument(1, "<indexes>")]
            public int[]? NoteIndexes { get; set; }

            [LocalizedDescription("ParameterPlayNotesLengthMilliseconds")]
            [CommandOption("-l|--length|--length-ms|--length-milliseconds")]
            [DefaultValue(250)]
            public int Length { get; set; }

            [LocalizedDescription("ParameterPlayNotesRestMilliseconds")]
            [CommandOption("-r|--rest|--rest-ms|--rest-milliseconds")]
            [DefaultValue(250)]
            public int Rest { get; set; }

            [LocalizedDescription("ParameterPlayNotesGroupNumber")]
            [CommandOption("-g|--group|--group-number")]
            [DefaultValue(1)]
            public int GroupNumber { get; set; }

            [LocalizedDescription("ParameterPlayNotesChannelNumber")]
            [CommandOption("-c|--channel|--channel-number")]
            [DefaultValue(1)]
            public int ChannelNumber { get; set; }

            [LocalizedDescription("ParameterPlayNotesVelocity")]
            [CommandOption("-v|--velocity|--velocity-percent")]
            [DefaultValue(75.0)]
            public double Velocity { get; set; }

            [LocalizedDescription("ParameterPlayNotesForever")]
            [CommandOption("-f|--forever|--repeat-forever")]
            [DefaultValue(false)]
            public bool Forever { get; set; }

            [LocalizedDescription("ParameterPlayNotesMidi2")]
            [CommandOption("-m|--midi2")]
            [DefaultValue(false)]
            public bool Midi2 { get; set; }
        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            
            if (!MidiGroup.IsValidIndex((byte)(settings.GroupNumber-1)))
            {
                return ValidationResult.Error("Group number must be between 1 and 16 inclusive.");
            }

            if (!MidiChannel.IsValidIndex((byte)(settings.ChannelNumber-1)))
            {
                return ValidationResult.Error("Channel index must be between 1 and 16 inclusive.");
            }

            if (settings.Velocity > 100.0 || settings.Velocity < 1.0)
            {
                return ValidationResult.Error("Velocity percentage must be between 1.0 and 100.0 inclusive.");
            }

            // todo: validate notes

            if (settings.NoteIndexes == null || settings.NoteIndexes!.Length == 0)
            {
                return ValidationResult.Error("Please provide at least one note index to play.");
            }

            foreach (var index in settings.NoteIndexes)
            {
                if (index > 127 || index < 0)
                {
                    return ValidationResult.Error("All note indexes must be between 0 and 127.");
                }
            }

            if (settings.Length < 30 )
            {
                return ValidationResult.Error("Please provide a note duration of at least 30 milliseconds.");
            }

            if (settings.Rest < 0)
            {
                return ValidationResult.Error("Please provide a rest duration of zero or more milliseconds.");
            }

            return base.Validate(context, settings);
        }



        public override int Execute(CommandContext context, Settings settings)
        {
            // we'll use this info when sleeping
    //        var systemTimerInfo = MidiClock.GetCurrentSystemTimerInfo();


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
                string endpointName = EndpointUtility.GetEndpointNameFromEndpointInterfaceId(endpointId);

                AnsiConsole.Markup(Strings.SendMessageSendingThroughEndpointLabel);
                AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatEndpointName(endpointName));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointId));
                AnsiConsole.WriteLine();

                bool openSuccess = false;

                // when this goes out of scope, it will dispose of the session, which closes the connections
                using var session = MidiSession.Create($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");

                if (session == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId, true);
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


                connection.EndpointDeviceDisconnected += Connection_EndpointDeviceDisconnected;
                connection.EndpointDeviceReconnected += Connection_EndpointDeviceReconnected;


                using AutoResetEvent m_messageDispatcherThreadWakeup = new AutoResetEvent(false);

                bool stillSending = true;

                MidiGroup group = new MidiGroup((byte)(settings.GroupNumber-1));
                MidiChannel channel = new MidiChannel((byte)(settings.ChannelNumber-1));

                var messageSenderThread = new Thread(() =>
                {
                    UInt32 noteArrayIndex = 0;

                    while (stillSending)
                    {
                        if (_disconnected)
                        {
                            stillSending = false;

                            // if we disconnect, we just have a short delay and then skip the rest of the loop
                            //Thread.Sleep(200);
                            break;
                        }

                        MidiSendMessageResults noteOnSendResult;

                        if (settings.Midi2)
                        {
                            UInt32 velocity = (UInt32)(settings.Velocity / 100.0) * UInt32.MaxValue;

                            var noteOnMessage = MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
                                                                MidiClock.TimestampConstantSendImmediately,
                                                                group,
                                                                Midi2ChannelVoiceMessageStatus.NoteOn,
                                                                channel,
                                                                (ushort)settings.NoteIndexes![noteArrayIndex],
                                                                velocity);

                            noteOnSendResult = connection.SendSingleMessagePacket(noteOnMessage);
                        }
                        else
                        {
                            byte velocity = (byte)settings.Velocity;

                            var noteOnMessage = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                                                                MidiClock.TimestampConstantSendImmediately,
                                                                group,
                                                                Midi1ChannelVoiceMessageStatus.NoteOn,
                                                                channel,
                                                                (byte)settings.NoteIndexes![noteArrayIndex],
                                                                velocity);

                            noteOnSendResult = connection.SendSingleMessagePacket(noteOnMessage);
                        }

                        // don't check still-sending here, or we can end up with a stuck note.
                        if (MidiEndpointConnection.SendMessageSucceeded(noteOnSendResult))
                        {
                            Console.Write("[");
                            AnsiConsole.Markup(AnsiMarkupFormatter.FormatMidi1Note((byte)settings.NoteIndexes![noteArrayIndex]));

                            // a note length of 0 is not allowed, so we go right to waiting

                            if (stillSending)
                            {
                                m_messageDispatcherThreadWakeup.WaitOne(settings.Length);
                            }

                            MidiSendMessageResults noteOffSendResult;

                            if (settings.Midi2)
                            {
                                UInt32 velocity = (UInt32)(settings.Velocity / 100.0) * UInt32.MaxValue;

                                var noteOffMessage = MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(
                                                                MidiClock.TimestampConstantSendImmediately,
                                                                group,
                                                                Midi2ChannelVoiceMessageStatus.NoteOff,
                                                                channel,
                                                                (ushort)settings.NoteIndexes![noteArrayIndex],
                                                                velocity);

                                noteOffSendResult = connection.SendSingleMessagePacket(noteOffMessage);
                            }
                            else
                            {
                                byte velocity = (byte)settings.Velocity;

                                var noteOffMessage = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                                                                MidiClock.TimestampConstantSendImmediately,
                                                                group,
                                                                Midi1ChannelVoiceMessageStatus.NoteOff,
                                                                channel,
                                                                (byte)settings.NoteIndexes![noteArrayIndex],
                                                                velocity);

                                noteOffSendResult = connection.SendSingleMessagePacket(noteOffMessage);
                            }

                            if (MidiEndpointConnection.SendMessageSucceeded(noteOffSendResult))
                            {
                                Console.Write("]");

                                if (settings.Rest > 0 && stillSending)
                                {
                                    m_messageDispatcherThreadWakeup.WaitOne(settings.Rest);
                                }
                                else
                                {
                                    // we just tight loop
                                }
                            }
                            else
                            {
                                // display failure message
                                AnsiConsole.WriteLine();
                                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Failed to send note-off message."));
                                AnsiConsole.WriteLine();
                                stillSending = false;
                            }
                        }
                        else
                        {
                            // display failure message
                            AnsiConsole.WriteLine();
                            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Failed to send note-on message."));
                            AnsiConsole.WriteLine();
                            stillSending = false;
                        }

                        if (stillSending)
                        {
                            noteArrayIndex++;
                            if (noteArrayIndex >= settings.NoteIndexes.Length)
                            {
                                if (settings.Forever)
                                {
                                    noteArrayIndex = 0;
                                }
                                else
                                {
                                    stillSending = false;
                                }
                            }
                        }
                    }

                });


                //    const uint bufferWarningThreshold = 10000;

                if (settings.NoteIndexes!.Length > 1 || settings.Forever)
                {
                    AnsiConsole.MarkupLine(Strings.SendMessagePressEscapeToStopSendingMessage);
                    AnsiConsole.WriteLine();
                }

                messageSenderThread.Start();

                while (stillSending)
                {
                    // check for input

                    if (Console.KeyAvailable)
                    {
                        var keyInfo = Console.ReadKey(true);

                        if (keyInfo.Key == ConsoleKey.Escape)
                        {
                            stillSending = false;

                            // wake up the threads so they terminate
                            m_messageDispatcherThreadWakeup.Set();

                            AnsiConsole.WriteLine();
                            AnsiConsole.MarkupLine("🛑 " + Strings.SendMessageEscapePressedMessage);
                        }

                    }

                    if (stillSending) Thread.Sleep(1);
                }

                session.DisconnectEndpointConnection(connection.ConnectionId);

                return (int)MidiConsoleReturnCode.Success;
            }
            else
            {
                // no endpoint

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }
        }

        bool _disconnected = false;

        // Auto-reconnect takes care of the internals. We're just reporting here
        private void Connection_EndpointDeviceReconnected(IMidiEndpointConnectionSource sender, object args)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(Strings.EndpointReconnected));

            _disconnected = false;
        }

        // Auto-reconnect takes care of the internals. We're just reporting here
        private void Connection_EndpointDeviceDisconnected(IMidiEndpointConnectionSource sender, object args)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.EndpointDisconnected));

            _disconnected = true;
        }

    }
}