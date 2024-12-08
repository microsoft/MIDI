using Microsoft.Windows.Devices.Midi2.Messages;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointPlayMidi1NotesCommand : Command<EndpointPlayMidi1NotesCommand.Settings>
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

            [LocalizedDescription("ParameterPlayNotesGroupIndex")]
            [CommandOption("-g|--group|--group-index")]
            [DefaultValue(0)]
            public int GroupIndex { get; set; }

            [LocalizedDescription("ParameterPlayNotesChannelIndex")]
            [CommandOption("-c|--channel|--channel-index")]
            [DefaultValue(0)]
            public int ChannelIndex { get; set; }

            [LocalizedDescription("ParameterPlayNotesVelocity")]
            [CommandOption("-v|--velocity")]
            [DefaultValue(127)]
            public int Velocity { get; set; }

            [LocalizedDescription("ParameterPlayNotesForever")]
            [CommandOption("-f|--forever|--repeat-forever")]
            [DefaultValue(false)]
            public bool Forever { get; set; }

        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            
            if (!MidiGroup.IsValidIndex((byte)settings.GroupIndex))
            {
                return ValidationResult.Error("Group index must be between 0 and 15 inclusive.");
            }

            if (!MidiChannel.IsValidIndex((byte)settings.ChannelIndex))
            {
                return ValidationResult.Error("Channel index must be between 0 and 15 inclusive.");
            }

            if (settings.Velocity > 127 || settings.Velocity < 0)
            {
                return ValidationResult.Error("Velocity must be between 0 and 127 inclusive.");
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
                    return ValidationResult.Error("All note indexes must be between 0 and 127");
                }
            }

            if (settings.Length < 30 )
            {
                return ValidationResult.Error("Please provide a note duration of at least 30 milliseconds");
            }

            if (settings.Rest < 0)
            {
                return ValidationResult.Error("Please provide a rest duration of zero or more milliseconds");
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

                MidiGroup group = new MidiGroup((byte)settings.GroupIndex);
                MidiChannel channel = new MidiChannel((byte)settings.ChannelIndex);
                byte velocity = (byte)settings.Velocity;

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

                        var noteOnMessage = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                                                            MidiClock.TimestampConstantSendImmediately,
                                                            group,
                                                            Midi1ChannelVoiceMessageStatus.NoteOn,
                                                            channel,
                                                            (byte)settings.NoteIndexes![noteArrayIndex],
                                                            velocity);

                        var noteOnSendResult = connection.SendSingleMessagePacket(noteOnMessage);

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

                            var noteOffMessage = MidiMessageBuilder.BuildMidi1ChannelVoiceMessage(
                                                                MidiClock.TimestampConstantSendImmediately,
                                                                group,
                                                                Midi1ChannelVoiceMessageStatus.NoteOff,
                                                                channel,
                                                                (byte)settings.NoteIndexes![noteArrayIndex],
                                                                velocity);

                            var noteOffSendResult = connection.SendSingleMessagePacket(noteOffMessage);

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