// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointMessageSender
    {
        public static MidiConsoleReturnCode OpenTemporaryConnectionAndSendMidiMessage(string? endpointDeviceId, IMidiUniversalPacket message)
        {
            var words = message.GetAllWords();

            return OpenTemporaryConnectionAndSendMidiMessage(endpointDeviceId, message.Timestamp, words.ToArray<UInt32>());
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
                using var session = MidiSession.Create($"{Strings.AppShortName}");
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
                    var sendResult = connection.SendSingleMessageWordArray(timestamp, 0, (byte)words.Count(), words);

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
