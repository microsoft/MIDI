// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Messages;
using Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer;
using Windows.Security.Cryptography.Core;

namespace Microsoft.Midi.ConsoleApp
{
    internal class MidiMessageTableColumn
    {
        public int ParameterIndex { get; set; }
        public string HeaderText { get; set; }
        public int Width { get; set; }

        public bool NoLeftSeparator { get; set; }
        public string DataColor { get; set; }

        public string DataFormat { get; set; }
        public MidiMessageTableColumn(int parameterIndex, string headerText, int width, bool noLeftSeparator, string dataColor, string dataFormat)
        {
            ParameterIndex = parameterIndex;
            HeaderText = headerText;
            Width = width;
            NoLeftSeparator = noLeftSeparator;
            DataColor = dataColor;
            DataFormat = dataFormat;
        }
    }


    internal class MidiMessageTable
    {
        private string _headerFormat = string.Empty;
        private string _separatorLine = string.Empty;
        private string _messageFormat = string.Empty;
        private string _errorMessageFormat = string.Empty;

        public Dictionary<int, MidiMessageTableColumn> Columns { get; private set; } = new Dictionary<int, MidiMessageTableColumn>();

        private void BuildStringFormats()
        {
            char horizontalLine = '\u2500';
            string cross = "\u253C";

            //string errorVerticalLine = "[black]\u2573[/]";
            string errorVerticalLine = "\u2502";
            string verticalLine = "\u2502";




            foreach (var col in Columns.Values)
            {
                if (_headerFormat != string.Empty && !col.NoLeftSeparator)
                {
                    _headerFormat += $" {verticalLine} ";
                }
                else if (col.NoLeftSeparator)
                {
                    _headerFormat += " ";
                }

                var headerText = col.HeaderText.PadRight(Math.Abs(col.Width), ' ');

                _headerFormat += $"[steelblue1]{headerText}[/]";

                // separator line -----------------------------------------------------

                if (_separatorLine != string.Empty && !col.NoLeftSeparator)
                {
                    _separatorLine += horizontalLine + cross + horizontalLine;
                }
                else if (col.NoLeftSeparator)
                {
                    _separatorLine += horizontalLine;
                }

                _separatorLine += new string(horizontalLine, Math.Abs(col.Width));

                // message line -------------------------------------------------------

                if (_messageFormat != string.Empty && !col.NoLeftSeparator)
                {
                    _messageFormat += $" {verticalLine} ";
                    _errorMessageFormat += $" {errorVerticalLine} ";
                }
                else if (col.NoLeftSeparator)
                {
                    _messageFormat += " ";
                    _errorMessageFormat += " ";
                }

                if (col.DataFormat == string.Empty)
                {
                    _messageFormat += $"[{col.DataColor}]{{{col.ParameterIndex},{col.Width}}}[/]";
                    _errorMessageFormat += $"[{col.DataColor}]{{{col.ParameterIndex},{col.Width}}}[/]";
                }
                else
                {
                    _messageFormat += $"[{col.DataColor}]{{{col.ParameterIndex},{col.Width}:{col.DataFormat}}}[/]";
                    _errorMessageFormat += $"[{col.DataColor}]{{{col.ParameterIndex},{col.Width}:{col.DataFormat}}}[/]";

                }

            }

            _errorMessageFormat += "[white] ** possible error **[/] ";

            _errorMessageFormat = $"[default on darkred]{_errorMessageFormat}[/]";

        }

        const int timestampOffsetValueColumnWidth = 9;

        private bool m_expectMidi2Data = true; // if true, we'll plan for 4 words of data in the display
        private bool m_verbose = false;
        private bool m_includeTimestamps = false;
        private bool m_decodeMessages = false;

        private string m_offsetValueDefaultColor = "darkseagreen";
        private string m_offsetValueErrorColor = "red";

        private string m_deltaValueDefaultColor = "lightskyblue3_1";
        private string m_deltaValueErrorColor = "red";

        private const int m_detailedMessageTypeTextWidth = 35;

        const int PARAMETER_INDEX = 0;
        const int PARAMETER_MESSAGE_TIMESTAMP = 1;
        const int PARAMETER_MESSAGE_TIMESTAMP_DELTA = 2;
        const int PARAMETER_MESSAGE_TIMESTAMP_DELTA_UNITS = 3;
        const int PARAMETER_RECEIVED_TIMESTAMP = 4;
        const int PARAMETER_RECEIVED_TIMESTAMP_DELTA = 5;
        const int PARAMETER_RECEIVED_TIMESTAMP_DELTA_UNITS = 6;
        const int PARAMETER_DATA_WORD0 = 7;
        const int PARAMETER_DATA_WORD1 = 8;
        const int PARAMETER_DATA_WORD2 = 9;
        const int PARAMETER_DATA_WORD3 = 10;
        const int PARAMETER_DECODED_MESSAGE_TYPE = 11;
        const int PARAMETER_DECODED_GROUP = 12;
        const int PARAMETER_DECODED_CHANNEL = 13;
        const int PARAMETER_DECODED_DATA = 14;



        public MidiMessageTable(
            bool expectMidi2Data,
            bool includeTimestamps,
            bool decodeMessages,
            bool verbose)
        {
            m_verbose = verbose;
            m_decodeMessages = decodeMessages;
            m_includeTimestamps = includeTimestamps;
            m_expectMidi2Data = expectMidi2Data;

            // todo: localize strings

            Columns.Add(PARAMETER_INDEX, new MidiMessageTableColumn(PARAMETER_INDEX, "Index", 8, true, "grey", ""));


            if (verbose || includeTimestamps)
            {
                Columns.Add(PARAMETER_MESSAGE_TIMESTAMP, new MidiMessageTableColumn(PARAMETER_MESSAGE_TIMESTAMP, "Message Timestamp", 19, false, "darkseagreen2", "N0"));                  // timestamp
                Columns.Add(PARAMETER_MESSAGE_TIMESTAMP_DELTA, new MidiMessageTableColumn(PARAMETER_MESSAGE_TIMESTAMP_DELTA, "From Last", timestampOffsetValueColumnWidth, false, m_offsetValueDefaultColor, ""));                                  // offset
                Columns.Add(PARAMETER_MESSAGE_TIMESTAMP_DELTA_UNITS, new MidiMessageTableColumn(PARAMETER_MESSAGE_TIMESTAMP_DELTA_UNITS, "", -2, true, "grey", ""));                                               // offset label
            }

            if (verbose)
            {
                Columns.Add(PARAMETER_RECEIVED_TIMESTAMP, new MidiMessageTableColumn(PARAMETER_RECEIVED_TIMESTAMP, "Received Timestamp", 19, false, "skyblue2", "N0"));    // recv timestamp
                Columns.Add(PARAMETER_RECEIVED_TIMESTAMP_DELTA, new MidiMessageTableColumn(PARAMETER_RECEIVED_TIMESTAMP_DELTA, "Receive \u0394", timestampOffsetValueColumnWidth, false, "lightskyblue3_1", ""));             // delta
                Columns.Add(PARAMETER_RECEIVED_TIMESTAMP_DELTA_UNITS, new MidiMessageTableColumn(PARAMETER_RECEIVED_TIMESTAMP_DELTA_UNITS, "", -2, true, "grey", ""));         // delta label
            }

            Columns.Add(PARAMETER_DATA_WORD0, new MidiMessageTableColumn(PARAMETER_DATA_WORD0, "Data", 8, false, "deepskyblue1", ""));
            Columns.Add(PARAMETER_DATA_WORD1, new MidiMessageTableColumn(PARAMETER_DATA_WORD1, "", 8, true, "deepskyblue2", ""));

            if (m_expectMidi2Data)
            {
                Columns.Add(PARAMETER_DATA_WORD2, new MidiMessageTableColumn(PARAMETER_DATA_WORD2, "", 8, true, "deepskyblue3", ""));
                Columns.Add(PARAMETER_DATA_WORD3, new MidiMessageTableColumn(PARAMETER_DATA_WORD3, "", 8, true, "deepskyblue4", ""));
            }

            if (verbose || decodeMessages)
            {
                Columns.Add(PARAMETER_DECODED_GROUP, new MidiMessageTableColumn(PARAMETER_DECODED_GROUP, "Gr", 2, false, "indianred", ""));
                Columns.Add(PARAMETER_DECODED_CHANNEL, new MidiMessageTableColumn(PARAMETER_DECODED_CHANNEL, "Ch", 2, true, "mediumorchid3", ""));
                Columns.Add(PARAMETER_DECODED_MESSAGE_TYPE, new MidiMessageTableColumn(PARAMETER_DECODED_MESSAGE_TYPE, "Message Type", m_detailedMessageTypeTextWidth * -1, false, "steelblue1_1", ""));
                Columns.Add(PARAMETER_DECODED_DATA,new MidiMessageTableColumn(PARAMETER_DECODED_DATA, "Decoded Data", -35, false, "grey", ""));
            }

            BuildStringFormats();
        }

        public void OutputHeader()
        {
            AnsiConsole.MarkupLine(_headerFormat);
            AnsiConsole.MarkupLine(_separatorLine);
        }

        public void OutputRow(ReceivedMidiMessage message)
        {
            //Console.WriteLine(message.Index);
            //return;

            string data = string.Empty;

            string detailedMessageType = string.Empty;
            string decodedData = string.Empty;

            var messageType = MidiMessageHelper.GetMessageTypeFromMessageFirstWord(message.Word0);


            if (m_verbose || m_decodeMessages)
            {
                detailedMessageType = MidiMessageHelper.GetMessageDisplayNameFromFirstWord(message.Word0);

                decodedData = GetDecodedOutput(message);
            }

            string word0 = string.Empty;
            string word1 = string.Empty;
            string word2 = string.Empty;
            string word3 = string.Empty;

            word0 = string.Format("{0:X8}", message.Word0);


            if (message.NumWords >= 2)
            {
                word1 = string.Format("{0:X8}", message.Word1);
            }

            if (message.NumWords >= 3)
            {
                word2 = string.Format("{0:X8}", message.Word2);
            }

            if (message.NumWords == 4)
            {
                word3 = string.Format("{0:X8}", message.Word3);
            }


            double offsetValue = 0;
            string offsetUnitLabel = string.Empty;

            AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit(message.ReceivedOffsetFromLastMessage, out offsetValue, out offsetUnitLabel);


            double deltaValue = 0;
            string deltaUnitLabel = string.Empty;

            if (m_verbose)
            {
                if (message.ReceivedTimestamp >= message.MessageTimestamp)
                {
                    AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit((UInt64)(message.ReceivedTimestamp - message.MessageTimestamp), out deltaValue, out deltaUnitLabel);
                }
                else
                {
                    // we received the message early, so the offset is negative
                    AnsiConsoleOutput.ConvertTicksToFriendlyTimeUnit((UInt64)(message.ReceivedTimestamp - message.MessageTimestamp), out deltaValue, out deltaUnitLabel);
                    deltaValue = deltaValue * -1;
                }

            }


            string groupText = string.Empty;
            string channelText = string.Empty;

            if (m_verbose || m_decodeMessages)
            {
                if (MidiMessageHelper.MessageTypeHasGroupField(messageType))
                {
                    groupText = MidiMessageHelper.GetGroupFromMessageFirstWord(message.Word0).DisplayValue.ToString().PadLeft(2);
                }

                if (MidiMessageHelper.MessageTypeHasChannelField(messageType))
                {
                    channelText = MidiMessageHelper.GetChannelFromMessageFirstWord(message.Word0).DisplayValue.ToString().PadLeft(2);
                }
            }

            // some cleanup


            string offsetValueText = offsetValue.ToString("F2");
            string deltaValueText = "";

            if (m_verbose || m_includeTimestamps)
            {
                // 0 is the magic "send now" value. You'll only see it
                // come back this way on a loopback. 
                if (message.MessageTimestamp == 0)
                {
                    deltaValueText = "--";
                    deltaUnitLabel = "";
                }
                else
                {
                    deltaValueText = deltaValue.ToString("F2");
                }

                if (Columns.ContainsKey(PARAMETER_MESSAGE_TIMESTAMP_DELTA))
                {
                    if (deltaValueText.Length > timestampOffsetValueColumnWidth)
                    {
                        deltaValueText = "000000";
                        deltaUnitLabel = "";

                        Columns[PARAMETER_MESSAGE_TIMESTAMP_DELTA].DataColor = m_deltaValueErrorColor;
                    }
                    else
                    {
                        Columns[PARAMETER_MESSAGE_TIMESTAMP_DELTA].DataColor = m_deltaValueDefaultColor;
                    }
                }

                if (Columns.ContainsKey(PARAMETER_RECEIVED_TIMESTAMP_DELTA))
                {
                    if (offsetValueText.Length > timestampOffsetValueColumnWidth)
                    {
                        offsetValueText = "000000";
                        offsetUnitLabel = "";
                        Columns[PARAMETER_RECEIVED_TIMESTAMP_DELTA].DataColor = m_offsetValueErrorColor;
                    }
                    else
                    {
                        Columns[PARAMETER_RECEIVED_TIMESTAMP_DELTA].DataColor = m_offsetValueDefaultColor;
                    }
                }
            }

            if (message.HasError)
            {
                AnsiConsole.MarkupLine(
                    _errorMessageFormat,
                    message.Index,
                    message.MessageTimestamp,
                    offsetValueText,
                    offsetUnitLabel,
                    message.ReceivedTimestamp,
                    deltaValueText,
                    deltaUnitLabel,
                    word0, word1, word2, word3,
                    detailedMessageType.PadRight(m_detailedMessageTypeTextWidth, ' ').Substring(0, m_detailedMessageTypeTextWidth),
                    groupText,
                    channelText,
                    decodedData
                    );
            }
            else
            {
                AnsiConsole.MarkupLine(
                    _messageFormat,
                    message.Index,
                    message.MessageTimestamp,
                    offsetValueText,
                    offsetUnitLabel,
                    message.ReceivedTimestamp,
                    deltaValueText,
                    deltaUnitLabel,
                    word0, word1, word2, word3,
                    detailedMessageType.PadRight(m_detailedMessageTypeTextWidth, ' ').Substring(0, m_detailedMessageTypeTextWidth),
                    groupText,
                    channelText,
                    decodedData
                    );

                UInt64 outputEnd = MidiClock.Now;
            }
        }
    
    
    
        public string GetDecodedOutput(ReceivedMidiMessage message)
        {
            // For SysEx, we display the bytes here. We may want to offer decoding for other message
            // types as well, in the future, like note on/off (easier with M1 protocol)

            var messageType = MidiMessageHelper.GetMessageTypeFromMessageFirstWord(message.Word0);

            switch(messageType)
            {
                case MidiMessageType.Midi1ChannelVoice32:
                    return GetDecodedMidi1ChannelVoiceData(message);

                case MidiMessageType.Midi2ChannelVoice64:
                    return GetDecodedMidi2ChannelVoiceData(message);

                case MidiMessageType.DataMessage64:
                    return GetDecodedSysEx7MessageData(message);

                default:
                    return string.Empty;

            }
        }

        private string FormatByteDecimal(byte value)
        {
            return value.ToString().PadLeft(3, ' ');
        }

        private string GetDecodedMidi1ChannelVoiceData(ReceivedMidiMessage message)
        {
            string decodedData = string.Empty;

            var status = MidiMessageHelper.GetStatusFromMidi1ChannelVoiceMessage(message.Word0);

            byte dataByte1 = (byte)((message.Word0 >> 8) & 0xFF);
            byte dataByte2 = (byte)((message.Word0) & 0xFF);

            switch (status)
            {
                case Midi1ChannelVoiceMessageStatus.NoteOn:
                case Midi1ChannelVoiceMessageStatus.NoteOff:

                    string noteInfo = $"({MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(dataByte1)}{MidiMessageHelper.GetNoteOctaveFromNoteIndex(dataByte1)})".PadRight(9);

                    decodedData = $"Note [darkseagreen2]{FormatByteDecimal(dataByte1)}[/] [deepskyblue1]{noteInfo}[/] "+
                        $"Velocity [darkseagreen2]{FormatByteDecimal(dataByte2)}[/]";
                    break;

                case Midi1ChannelVoiceMessageStatus.ControlChange:
                    decodedData = $"Controller [darkseagreen2]{FormatByteDecimal(dataByte1)}[/], Value [darkseagreen2]{FormatByteDecimal(dataByte2)}[/]";
                    break;

                case Midi1ChannelVoiceMessageStatus.PitchBend:
                    decodedData = $"Fine [darkseagreen2]{FormatByteDecimal(dataByte1)}[/], Coarse [darkseagreen2]{FormatByteDecimal(dataByte2)}[/]";
                    break;

                case Midi1ChannelVoiceMessageStatus.ChannelPressure:
                    decodedData = $"Value [darkseagreen2]{FormatByteDecimal(dataByte1)}[/]";
                    break;

                case Midi1ChannelVoiceMessageStatus.PolyPressure:
                    decodedData = $"Key [darkseagreen2]{FormatByteDecimal(dataByte1)}[/], Value [darkseagreen2]{FormatByteDecimal(dataByte2)}[/]";
                    break;

                case Midi1ChannelVoiceMessageStatus.ProgramChange:
                    decodedData = $"Value [darkseagreen2]{FormatByteDecimal(dataByte1)}[/]";
                    break;

                default:
                    break;
            }

            return decodedData;
        }

        private string GetDecodedMidi2ChannelVoiceData(ReceivedMidiMessage message)
        {
            string decodedData = string.Empty;

            var status = MidiMessageHelper.GetStatusFromMidi2ChannelVoiceMessageFirstWord(message.Word0);

            byte dataByte1 = (byte)((message.Word0 >> 8) & 0xFF);
            byte dataByte2 = (byte)((message.Word0) & 0xFF);

            switch (status)
            {
                case Midi2ChannelVoiceMessageStatus.NoteOn:
                case Midi2ChannelVoiceMessageStatus.NoteOff:
                    {
                        UInt16 velocity = (UInt16)((message.Word1 >> 16) & 0xFFFF);
                        UInt16 attribute = (UInt16)(message.Word1 & 0xFFFF);

                        string noteData = $"({MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(dataByte1)})".PadRight(7);

                        decodedData =
                            $"Note [darkseagreen2]{FormatByteDecimal(dataByte1)}[/] [deepskyblue1]{noteData}[/] " +
                            /*$"Type [darkseagreen2]{FormatByteDecimal(dataByte2)}[/] " + */
                            $"Vel  [darkseagreen2]{velocity.ToString().PadLeft(6)}[/] " +
                            $"Attr [darkseagreen2]{attribute}[/]";
                    }
                    break;

                case Midi2ChannelVoiceMessageStatus.PolyPressure:
                    decodedData =
                        $"Note [darkseagreen2]{FormatByteDecimal(dataByte1)}[/] [deepskyblue1](May be: {MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(dataByte1)}[/], " +
                        $"Data [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.RegisteredPerNoteController:
                case Midi2ChannelVoiceMessageStatus.AssignablePerNoteController:
                    decodedData =
                        $"Note [darkseagreen2]{FormatByteDecimal(dataByte1)}[/] [deepskyblue1](May be: {MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(dataByte1)}[/], " +
                        $"Index [darkseagreen2]{FormatByteDecimal(dataByte2)}[/], " + 
                        $"Data [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.PerNoteManagement:
                    decodedData =
                        $"Note: [darkseagreen2]{FormatByteDecimal(dataByte1)}[/] [deepskyblue1](May be: {MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(dataByte1)}[/], " +
                        $"Detach: [darkseagreen2]{(bool)((dataByte1 & 0x2) == 0x2)}[/], " +
                        $"Set: [darkseagreen2]{(bool)((dataByte1 & 0x1) == 0x1)}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.ControlChange:
                    decodedData = 
                        $"Controller: [darkseagreen2]{FormatByteDecimal(dataByte1)}[/], " +
                        $"Data: [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.RegisteredController:
                case Midi2ChannelVoiceMessageStatus.AssignableController:
                    decodedData =
                        $"Bank: [darkseagreen2]{FormatByteDecimal(dataByte1)}[/], " +
                        $"Index: [darkseagreen2]{FormatByteDecimal(dataByte2)}[/], " +
                        $"Data: [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.RelativeRegisteredController:
                case Midi2ChannelVoiceMessageStatus.RelativeAssignableController:
                    decodedData =
                        $"Bank: [darkseagreen2]{FormatByteDecimal(dataByte1)}[/], " +
                        $"Index: [darkseagreen2]{FormatByteDecimal(dataByte2)}[/], " +
                        $"Data: [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.ProgramChange:
                    decodedData = 
                        $"Program: [darkseagreen2]{FormatByteDecimal((byte)(message.Word1 & 0xFF000000 >> 24))}[/], " +
                        $"Bank Valid: [darkseagreen2]{(bool)((dataByte1 & 0x1) == 0x1)}[/], " +
                        $"MSB: [darkseagreen2]{FormatByteDecimal((byte)(message.Word1 & 0x0000FF00 >> 8))}[/], " +
                        $"LSB: [darkseagreen2]{FormatByteDecimal((byte)(message.Word1 & 0x000000FF))}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.ChannelPressure:
                    decodedData = $"Data: [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.PitchBend:
                    decodedData = $"Data: [darkseagreen2]{message.Word1}[/]";
                    break;

                case Midi2ChannelVoiceMessageStatus.PerNotePitchBend:
                    decodedData = 
                        $"Note: [darkseagreen2]{FormatByteDecimal(dataByte1)}[/] [deepskyblue1](May be: {MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(dataByte1)}[/], " +
                        $"Data: [darkseagreen2]{message.Word1}[/]";
                    break;

                default:
                    break;
            }

            return decodedData;
        }



        private string GetDecodedSysEx7MessageData(ReceivedMidiMessage message)
        {
            string decodedData = string.Empty;

            const uint maxSysEx7BytesPerMessage = 6;
            if (MidiSystemExclusiveMessageHelper.MessageIsSystemExclusive7Message(message.Word0))
            {
                // show the bytes
                var byteCount = MidiSystemExclusiveMessageHelper.GetDataByteCountFromSystemExclusive7MessageFirstWord(message.Word0);

                foreach (var b in MidiSystemExclusiveMessageHelper.GetDataBytesFromSingleSystemExclusive7Message(message.Word0, message.Word1))
                {
                    decodedData += b.ToString("X2") + " ";
                }

                if (byteCount < maxSysEx7BytesPerMessage)
                {
                    decodedData += "[grey]";

                    for (int i = 0; i < maxSysEx7BytesPerMessage - byteCount; i++)
                    {
                        decodedData += "-- ";
                    }

                    decodedData += "[/]";
                }

                decodedData = $"[darkseagreen2]{decodedData}[/]";
            }

            return decodedData;
        }

    }


}
