// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

//using Microsoft.Windows.Devices.Midi2.Initialization;
using Microsoft.Windows.Devices.Midi2;
using Microsoft.Windows.Devices.Midi2.Messages;
using System.Net;
using System.Threading.Channels;

using System.Linq;

namespace Microsoft.Midi.ConsoleApp
{
    internal class MeasureLoopbackJitterCommand : Command<MeasureLoopbackJitterCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterMeasureDestinationEndpointDeviceId")]
            [CommandArgument(0, "[Destination Endpoint Device Id]")]
            public string? DestinationEndpointDeviceId { get; set; }

            [LocalizedDescription("ParameterMeasureSourceEndpointDeviceId")]
            [CommandArgument(1, "[Source Endpoint Device Id]")]
            public string? SourceEndpointDeviceId { get; set; }

            [LocalizedDescription("ParameterMeasureDestinationGroupNumber")]
            [CommandOption("-d|--destination-group|--destination-group-number")]
            [DefaultValue(1)]
            public int DestinationGroupNumber { get; set; }

            [LocalizedDescription("ParameterMeasureSourceGroupNumber")]
            [CommandOption("-s|--source-group|--source-group-number")]
            [DefaultValue(1)]
            public int SourceGroupNumber { get; set; }


            [LocalizedDescription("ParameterMeasureUseScheduler")]
            [CommandOption("-u|--use-scheduler")]
            [DefaultValue(true)]
            public bool UseScheduler { get; set; }

            [LocalizedDescription("ParameterMeasureUseMidi2")]
            [CommandOption("-m|--midi2")]
            [DefaultValue(false)]
            public bool Midi2 { get; set; }

            //[LocalizedDescription("ParameterMeasureMessageCount")]
            //[CommandOption("-c|--count")]
            //[DefaultValue(127)]
            //public int MessageCount { get; set; }

        }

        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate endpoints 

            if ((settings.DestinationGroupNumber < 1 || settings.SourceGroupNumber < 1) ||
                !MidiGroup.IsValidIndex((byte)(settings.DestinationGroupNumber - 1)) ||
                !MidiGroup.IsValidIndex((byte)(settings.SourceGroupNumber - 1)))
            {
                return ValidationResult.Error("Please provide a group number between 1 and 16, inclusive.");
            }

            return ValidationResult.Success();
        }


        internal class MessageTrackingEntry
        {
            // we track the messages by note number and channel, which is why we're limited to 127*16 messages
            internal byte ChannelIndex;
            internal byte NoteNumber;

            internal UInt64 SendTimestamp;           // actual sent or scheduled
            internal UInt64 ServiceReceiveTimestamp; // time when it was received in the service
            internal UInt64 ClientReceiveTimestamp;  // time when it was received in the event

            internal UInt32 word0;
            internal UInt32 word1;
        }

        const uint TOTAL_CHANNELS = 16;
        const uint TOTAL_NOTES = 128;
        const uint TOTAL_MESSAGE_COUNT = TOTAL_CHANNELS * TOTAL_NOTES;     // we keep track of messages by using channel and note

        private uint m_receivedCount = 0;

        private Dictionary<UInt16, MessageTrackingEntry> m_messageEntries = [];    // key is channel << 8 + note;

        MidiGroup m_activeSourceGroup;
        MidiGroup m_activeDestinationGroup;

        ManualResetEvent m_allMessagesReceivedEvent = new ManualResetEvent(false);

        MidiEndpointConnection? sourceConnection = null;
        MidiEndpointConnection? destinationConnection = null;

        bool m_useMidi2 = false;

        private void SendWorker()
        {
            destinationConnection!.Open();

            // send all messages

         //   AnsiConsole.WriteLine($"Message count to send: {m_messageEntries.Count}");

            UInt16 sentCount = 0;

            foreach (var entry in m_messageEntries.Values)
            {
                entry.SendTimestamp = MidiClock.Now;

                if (m_useMidi2)
                {
                    destinationConnection!.SendSingleMessageWords(0, entry.word0, entry.word1);
                }
                else
                {
                    destinationConnection!.SendSingleMessageWords(0, entry.word0);
                }

                Thread.Sleep(1);

                sentCount++;
            }

            //AnsiConsole.WriteLine($"Count Messages Sent: {sentCount}");
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            string destinationEndpointDeviceId = string.Empty;
            string sourceEndpointDeviceId = string.Empty;  

            if (!string.IsNullOrEmpty(settings.DestinationEndpointDeviceId))
            {
                destinationEndpointDeviceId = settings.DestinationEndpointDeviceId.Trim();
            }
            else
            {
                destinationEndpointDeviceId = UmpEndpointPicker.PickEndpoint(Strings.PleasePickDestinationEndpoint);
            }

            if (string.IsNullOrEmpty(destinationEndpointDeviceId))
            {
                // no endpoint chosen. Exit
                return (int)MidiConsoleReturnCode.ErrorUserCanceled;
            }

            if (!string.IsNullOrEmpty(settings.SourceEndpointDeviceId))
            {
                sourceEndpointDeviceId = settings.SourceEndpointDeviceId.Trim();
            }
            else
            {
                sourceEndpointDeviceId = UmpEndpointPicker.PickEndpoint(Strings.PleasePickSourceEndpoint);
            }

            if (string.IsNullOrEmpty(sourceEndpointDeviceId))
            {
                // no endpoint chosen. Exit
                return (int)MidiConsoleReturnCode.ErrorUserCanceled;
            }


            // find and validate the endpoints

            var destinationEndpointInfo = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(destinationEndpointDeviceId);
            var sourceEndpointInfo = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(sourceEndpointDeviceId);

            if (destinationEndpointInfo == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToFindEndpointDevice));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(destinationEndpointDeviceId));

                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }

            if (sourceEndpointInfo == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToFindEndpointDevice));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(sourceEndpointDeviceId));

                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }

            var destinationGroup = new MidiGroup((byte)(settings.DestinationGroupNumber - 1));
            var sourceGroup = new MidiGroup((byte)(settings.SourceGroupNumber - 1));

            m_activeSourceGroup = sourceGroup;
            m_activeDestinationGroup = destinationGroup;

            // get information about the endpoints and display to the user
            // check MIDI 1.0 and MIDI 2.0 capabilities and inform the user
            // about any translation involved, and where it is done


            AnsiConsole.MarkupLine($"Destination (outgoing) Endpoint { AnsiMarkupFormatter.FormatEndpointName(destinationEndpointInfo.Name)}, {destinationGroup.ToString()}");
            AnsiConsole.MarkupLine($"Source (incoming) Endpoint      { AnsiMarkupFormatter.FormatEndpointName(sourceEndpointInfo.Name)}, {sourceGroup.ToString()}");

            // create the session. Set it to call dispose when it goes out of scope

            using var session = MidiSession.Create("Jitter Measurement");

            if (session == null || !session.IsOpen)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }

            sourceConnection = session.CreateEndpointConnection(sourceEndpointInfo.EndpointDeviceId);
            destinationConnection = session.CreateEndpointConnection(destinationEndpointInfo.EndpointDeviceId);

            if (sourceConnection == null || destinationConnection == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));
                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }

            m_useMidi2 = settings.Midi2;

            for (byte ch = 0; ch < 16; ch++)
            {
                var channel = new MidiChannel(ch);

                for (byte note = 0; note < 127; note++)
                {
                    var entry = new MessageTrackingEntry();

                    entry.NoteNumber = note;
                    entry.ChannelIndex = ch;

                    if (settings.Midi2)
                    {
                        entry.word0 = 0x40800000;
                        entry.word1 = 0x00000000;
                    }
                    else
                    {
                        // status 8, velocity > 0 (A in this case)
                        entry.word0 = 0x2080000A;
                    }

                    entry.word0 = MidiMessageHelper.ReplaceGroupInMessageFirstWord(entry.word0, m_activeDestinationGroup);
                    entry.word0 = MidiMessageHelper.ReplaceChannelInMessageFirstWord(entry.word0, channel);
                    entry.word0 |= ((UInt32)note << 8);

                    entry.word1 = 0;

                    UInt16 key = (UInt16)((UInt16)(ch << 8) | (UInt16)note);

                    m_messageEntries.Add(key, entry);
                }
            }

            AnsiConsole.MarkupLine($"Created {m_messageEntries.Count} messages to send.");


            // set up the message listener
            //Thread receiveWorker = new Thread(new ThreadStart(ReceiveWorker));
            //receiveWorker.Start();

            sourceConnection!.MessageReceived += SourceConnection_MessageReceived;
            sourceConnection!.Open();


            // start sending messages
            Thread sendWorker = new Thread(new ThreadStart(SendWorker));
            sendWorker.Start();

            // wait for all messages to be received, or a timeout

            //AnsiConsole.MarkupLine("Waiting...");

            m_allMessagesReceivedEvent.WaitOne();

            //AnsiConsole.MarkupLine("All messages received.");

            // clean up event handler
            sourceConnection.MessageReceived -= SourceConnection_MessageReceived;

            // analysis -------------------------------------------------------------------------------------------------

            // calculate average delta between when send timestamp and service receive timestamp

            Int64 totalSendToServiceReceiveDelta = 0;
            Int64 totalSendToClientReceiveDelta = 0;

            Int64 averageSendToServiceReceiveDelta = 0;
            Int64 averageSendToClientReceiveDelta = 0;

            UInt64 totalSendToServiceReceiveJitterSumOfSquares = 0;
            UInt64 totalSendToClientReceiveJitterSumOfSquares = 0;

            UInt64 maxSendToServiceReceiveDeviationFromMean = 0;
            UInt64 maxSendToClientReceiveDeviationFromMean = 0;


            // calculate averages
            foreach (var entry in m_messageEntries.Values)
            {
                totalSendToServiceReceiveDelta += (Int64)(entry.ServiceReceiveTimestamp - entry.SendTimestamp);
                totalSendToClientReceiveDelta += (Int64)(entry.ClientReceiveTimestamp - entry.SendTimestamp);
            }

            averageSendToServiceReceiveDelta = totalSendToServiceReceiveDelta / TOTAL_MESSAGE_COUNT;
            averageSendToClientReceiveDelta = totalSendToClientReceiveDelta / TOTAL_MESSAGE_COUNT;

            // calculate stdev
            foreach (var entry in m_messageEntries.Values)
            {
                var sendToServiceReceiveDelta = (Int64)(entry.ServiceReceiveTimestamp - entry.SendTimestamp);
                var sendToClientReceiveDelta = (Int64)(entry.ClientReceiveTimestamp - entry.SendTimestamp);

                maxSendToServiceReceiveDeviationFromMean = Math.Max((ulong)Math.Abs(sendToServiceReceiveDelta - averageSendToServiceReceiveDelta), maxSendToServiceReceiveDeviationFromMean);
                maxSendToClientReceiveDeviationFromMean = Math.Max((ulong)Math.Abs(sendToClientReceiveDelta - averageSendToClientReceiveDelta), maxSendToClientReceiveDeviationFromMean);

                totalSendToServiceReceiveJitterSumOfSquares += (UInt64)(Math.Pow(sendToServiceReceiveDelta - averageSendToServiceReceiveDelta, 2));
                totalSendToClientReceiveJitterSumOfSquares += (UInt64)(Math.Pow(sendToClientReceiveDelta - averageSendToClientReceiveDelta, 2));
            }

            var stdDevSendToServiceReceiveJitter = Math.Sqrt(totalSendToServiceReceiveJitterSumOfSquares / TOTAL_MESSAGE_COUNT);
            var stdDevSendToClientReceiveJitter = Math.Sqrt(totalSendToClientReceiveJitterSumOfSquares / TOTAL_MESSAGE_COUNT);



            //AnsiConsole.MarkupLine($"Service Received Time Average {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)averageSendToServiceReceiveDelta)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)averageSendToServiceReceiveDelta)} milliseconds)");
            //AnsiConsole.MarkupLine($"Client Received Time Average  {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)averageSendToClientReceiveDelta)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)averageSendToClientReceiveDelta)} milliseconds)");

            AnsiConsole.MarkupLine("");
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatTableColumnHeading("Round trip from client to endpoint and then back to service"));
            AnsiConsole.MarkupLine($"  Standard Deviation       {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)stdDevSendToServiceReceiveJitter)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)stdDevSendToServiceReceiveJitter)} milliseconds)");
            //AnsiConsole.MarkupLine($"Mean {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)averageSendToServiceReceiveDelta)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)averageSendToServiceReceiveDelta)} milliseconds)");
            AnsiConsole.MarkupLine($"  Max Deviation from Mean  {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)maxSendToServiceReceiveDeviationFromMean)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)maxSendToServiceReceiveDeviationFromMean)} milliseconds)");
            AnsiConsole.MarkupLine("");

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatTableColumnHeading("Full round trip from Client to endpoint and then back to this client"));
            AnsiConsole.MarkupLine($"  Standard Deviation       {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)stdDevSendToClientReceiveJitter)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)stdDevSendToClientReceiveJitter)} milliseconds)");
            //AnsiConsole.MarkupLine($"Mean {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)averageSendToClientReceiveDelta)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)averageSendToClientReceiveDelta)} milliseconds)");
            AnsiConsole.MarkupLine($"  Max Deviation from Mean  {MidiClock.ConvertTimestampTicksToMicroseconds((UInt64)maxSendToClientReceiveDeviationFromMean)} microseconds, ({MidiClock.ConvertTimestampTicksToMilliseconds((UInt64)maxSendToClientReceiveDeviationFromMean)} milliseconds)");






            return (int)MidiConsoleReturnCode.Success;
        }

        private void SourceConnection_MessageReceived(IMidiMessageReceivedEventSource sender, MidiMessageReceivedEventArgs args)
        {
            UInt64 clientReceiveTimestamp = MidiClock.Now;

            MidiMessageStruct message;

            args.FillMessageStruct(out message);


            byte group = (byte)((message.Word0 & 0x0F000000) >> 24) ;

            // get out quickly if this group was not the expected group to receive from
            if (group != m_activeSourceGroup.Index) return;

            UInt16 channelShifted = 0;
            byte note = 0;

            if ((message.Word0 & 0xF0000000) == 0x20000000)
            {
                // type 2: MIDI 1 channel voice message
                channelShifted = (UInt16)((message.Word0 & 0x000F0000) >> 8);
                note = (byte)((message.Word0 & 0x00007F00) >> 8);
            }
            else if ((message.Word0 & 0xF0000000) == 0x40000000)
            {
                // type 4: MIDI 2 channel voice message
                channelShifted = (UInt16)((message.Word0 & 0x000F0000) >> 8);
                note = (byte)((message.Word0 & 0x00007F00) >> 8);
            }
            else
            {
                // unexpected.
                return;
            }

            UInt16 key = (UInt16)(channelShifted | note);

            // message key is channel << 8 + note

            // not one of ours
            if (!m_messageEntries.ContainsKey(key)) return;


            m_messageEntries[key].ClientReceiveTimestamp = clientReceiveTimestamp;
            m_messageEntries[key].ServiceReceiveTimestamp = args.Timestamp;

            m_receivedCount++;

            //AnsiConsole.WriteLine($"Received {channelShifted} {note}");

            if (m_receivedCount == TOTAL_MESSAGE_COUNT)
            {
                m_allMessagesReceivedEvent.Set();
            }
        }
    }
}
