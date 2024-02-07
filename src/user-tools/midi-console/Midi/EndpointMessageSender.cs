using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class EndpointMessageSender
    {
        public static MidiConsoleReturnCode OpenTemporaryConnectionAndSendMidiMessage(string? endpointDeviceId, MidiMessage128 message)
        {
            UInt32[] words = { message.Word0, message.Word1, message.Word2, message.Word3 };

            return OpenTemporaryConnectionAndSendMidiMessage(endpointDeviceId, message.Timestamp, words);
        }

        public static MidiConsoleReturnCode OpenTemporaryConnectionAndSendMidiMessage(string? endpointDeviceId, MidiMessage96 message)
        {
            UInt32[] words = { message.Word0, message.Word1, message.Word2 };

            return OpenTemporaryConnectionAndSendMidiMessage(endpointDeviceId, message.Timestamp, words);
        }

        public static MidiConsoleReturnCode OpenTemporaryConnectionAndSendMidiMessage(string? endpointDeviceId, MidiMessage64 message)
        {
            UInt32[] words = { message.Word0, message.Word1 };

            return OpenTemporaryConnectionAndSendMidiMessage(endpointDeviceId, message.Timestamp, words);
        }

        public static MidiConsoleReturnCode OpenTemporaryConnectionAndSendMidiMessage(string? endpointDeviceId, MidiMessage32 message)
        {
            UInt32[] words = { message.Word0 };

            return OpenTemporaryConnectionAndSendMidiMessage(endpointDeviceId, message.Timestamp, words);
        }


        public static MidiConsoleReturnCode OpenTemporaryConnectionAndSendMidiMessage(string? endpointDeviceId, UInt64 timestamp, UInt32[] words)
        {
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(endpointDeviceId))
            {
                endpointId = endpointDeviceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            if (!string.IsNullOrEmpty(endpointId))
            {
                //AnsiConsole.MarkupLine(Strings.MonitorMonitoringOnEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));

                //var table = new Table();

                // when this goes out of scope, it will dispose of the session, which closes the connections
                using var session = MidiSession.CreateSession($"{Strings.AppShortName}");
                if (session == null)
                {
                    AnsiConsole.WriteLine(Strings.ErrorUnableToCreateSession);
                    return MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId);
                if (connection == null)
                {
                    AnsiConsole.WriteLine(Strings.ErrorUnableToCreateEndpointConnection);
                    return MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
                }

                if (connection.Open())
                {
                    // send the message
                    var sendResult = connection.SendMessageWordArray(timestamp, words, 0, (byte)words.Count());

                    if (MidiEndpointConnection.SendMessageSucceeded(sendResult))
                    {
                        return MidiConsoleReturnCode.Success;
                    }
                    else
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToSendMessage));

                        // TODO: Could return more detailed info here.
                        return MidiConsoleReturnCode.ErrorGeneralFailure;
                    }

                }
                else
                {
                    AnsiConsole.WriteLine(Strings.ErrorUnableToOpenEndpoint);
                    return MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }


            }

            return (int)MidiConsoleReturnCode.Success;
        }


    }
}
