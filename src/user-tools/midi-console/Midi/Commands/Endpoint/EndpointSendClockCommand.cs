using Microsoft.Windows.Devices.Midi2.Messages;
using Microsoft.Windows.Devices.Midi2.Utilities.Sequencing;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointSendClockCommand : Command<EndpointSendClockCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            // Would be better to remove this and just do an enumeration lookup to see what type of endpoint it is

            [LocalizedDescription("ParameterSendClockTempo")]
            [CommandOption("-t|--tempo|--bpm|--beats-per-minute")]
            [DefaultValue(120.0)]
            public double Tempo { get; set; }

            [LocalizedDescription("ParameterSendClockPPQN")]
            [CommandOption("-p|--ppqn|--pulses|--pulses-per-quarter-note")]
            [DefaultValue(24)]
            public int PulsesPerQuarterNote { get; set; }

            [LocalizedDescription("ParameterSendClockGroupNumbers")]
            [CommandOption("-g|--group|--group-number <VALUES>")]
            public int[] GroupNumbers { get; set; }

            [LocalizedDescription("ParameterSendClockSendMidiStartMessage")]
            [CommandOption("-s|--send-midi-start-message|--send-start-message|--send-start")]
            [DefaultValue(false)]
            public bool SendMidiStartMessage { get; set; }

            [LocalizedDescription("ParameterSendClockSendMidiStopMessage")]
            [CommandOption("-x|--send-midi-stop-message|--send-stop-message|--send-stop")]
            [DefaultValue(false)]
            public bool SendMidiStopMessage { get; set; }

        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {           
            if (settings.GroupNumbers == null || settings.GroupNumbers.Length == 0)
            {
                return ValidationResult.Error("Please include at least one group number that is between 1 and 16 inclusive.");
            }

            foreach (var group in settings.GroupNumbers)
            {
                if (!MidiGroup.IsValidIndex(Convert.ToByte(group-1)))
                {
                    return ValidationResult.Error($"Invalid group number '{group}'. Please ensure all group numbers are between 1 and 16 inclusive.");
                }
            }

            if (settings.PulsesPerQuarterNote < 1 || settings.PulsesPerQuarterNote > 96)
            {
                return ValidationResult.Error("Please provide a PPQN between 1 and 96, inclusive. Normal is 24.");
            }

            if (settings.Tempo < 10 || settings.Tempo > 400)
            {
                return ValidationResult.Error("Please provide a Tempo between 10 and 400, inclusive. Fractional values are allowed.");
            }


            return base.Validate(context, settings);
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

                IMidiEndpointConnectionSettings connectionSettings;
                connectionSettings = new MidiEndpointConnectionBasicSettings(false, false);

                var connection = session.CreateEndpointConnection(endpointId, connectionSettings); 
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

                // the in-scope groups for the clock
                List<MidiGroup> groups = new List<MidiGroup>();
                foreach (var groupNumber in settings.GroupNumbers)
                {
                    groups.Add(new MidiGroup(Convert.ToByte(groupNumber-1)));
                }

                // the destination connections and their groups
                List<MidiClockDestination> destinations = new List<MidiClockDestination>();
                destinations.Add(new MidiClockDestination(connection, groups));

                // create and start the clock generator
                MidiClockGenerator generator = new MidiClockGenerator(destinations, settings.Tempo, Convert.ToUInt16(settings.PulsesPerQuarterNote));
                generator.Start(settings.SendMidiStartMessage);

                AnsiConsole.MarkupLine(Strings.SendClockPressEscapeToStopSendingMessage);
                AnsiConsole.WriteLine();

                while (stillSending)
                {
                    // check for input

                    if (Console.KeyAvailable)
                    {
                        var keyInfo = Console.ReadKey(true);

                        if (keyInfo.Key == ConsoleKey.Escape)
                        {
                            stillSending = false;

                            AnsiConsole.WriteLine();
                            AnsiConsole.MarkupLine("🛑 " + Strings.SendMessageEscapePressedMessage);

                            generator.Stop(settings.SendMidiStopMessage);

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

    //    bool _disconnected = false;

        // Auto-reconnect takes care of the internals. We're just reporting here
        private void Connection_EndpointDeviceReconnected(IMidiEndpointConnectionSource sender, object args)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(Strings.EndpointReconnected));

     //       _disconnected = false;
        }

        // Auto-reconnect takes care of the internals. We're just reporting here
        private void Connection_EndpointDeviceDisconnected(IMidiEndpointConnectionSource sender, object args)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.EndpointDisconnected));

     //       _disconnected = true;
        }

    }
}