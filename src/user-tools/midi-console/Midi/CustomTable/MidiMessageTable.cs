// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



using Microsoft.Windows.Devices.Midi2.Messages;

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

        public List<MidiMessageTableColumn> Columns { get; private set; } = new List<MidiMessageTableColumn>();

        private void BuildStringFormats()
        {
            char horizontalLine = '\u2500';
            string cross = "\u253C";

            //string errorVerticalLine = "[black]\u2573[/]";
            string errorVerticalLine = "\u2502";
            string verticalLine = "\u2502";




            foreach (var col in Columns)
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

        private bool m_verbose = false;
        private int m_offsetColumnNumber = 2;
        private int m_deltaColumnNumber = 5;

        private string m_offsetValueDefaultColor = "darkseagreen";
        private string m_offsetValueErrorColor = "red";

        private string m_deltaValueDefaultColor = "lightskyblue3_1";
        private string m_deltaValueErrorColor = "red";

        public MidiMessageTable(bool verbose)
        {
            m_verbose = verbose;

            // todo: localize strings

            Columns.Add(new MidiMessageTableColumn(0, "Index", 8, true, "grey", ""));
            Columns.Add(new MidiMessageTableColumn(1, "Message Timestamp", 19, false, "darkseagreen2", "N0"));                  // timestamp
            Columns.Add(new MidiMessageTableColumn(m_offsetColumnNumber, "From Last", timestampOffsetValueColumnWidth, false, m_offsetValueDefaultColor, ""));                                  // offset
            Columns.Add(new MidiMessageTableColumn(3, "", -2, true, "grey", ""));                                               // offset label
            if (verbose) Columns.Add(new MidiMessageTableColumn(4, "Received Timestamp", 19, false, "skyblue2", "N0"));    // recv timestamp
            if (verbose) Columns.Add(new MidiMessageTableColumn(m_deltaColumnNumber, "Receive \u0394", timestampOffsetValueColumnWidth, false, "lightskyblue3_1", ""));             // delta
            if (verbose) Columns.Add(new MidiMessageTableColumn(6, "", -2, true, "grey", ""));                                  // delta label
            Columns.Add(new MidiMessageTableColumn(7, "Words", 8, false, "deepskyblue1", ""));
            Columns.Add(new MidiMessageTableColumn(8, "", 8, true, "deepskyblue2", ""));
            Columns.Add(new MidiMessageTableColumn(9, "", 8, true, "deepskyblue3", ""));
            Columns.Add(new MidiMessageTableColumn(10, "", 8, true, "deepskyblue4", ""));

            if (verbose)
            {
                Columns.Add(new MidiMessageTableColumn(11, "Gr", 2, false, "indianred", ""));
                Columns.Add(new MidiMessageTableColumn(12, "Ch", 2, true, "mediumorchid3", ""));
                Columns.Add(new MidiMessageTableColumn(13, "Decoded Data", -35, false, "deepskyblue1", ""));
                Columns.Add(new MidiMessageTableColumn(14, "Message Type", -35, false, "steelblue1_1", ""));
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


            if (m_verbose)
            {
                detailedMessageType = MidiMessageHelper.GetMessageDisplayNameFromFirstWord(message.Word0);

                // For SysEx, we display the bytes here. We may want to offer decoding for other message
                // types as well, in the future, and just have a dedicated column for the decoding.

                const uint maxSysEx7BytesPerMessage = 6;
                if (MidiMessageHelper.MessageIsSystemExclusive7Message(message.Word0))
                {
                    // show the bytes
                    var byteCount = MidiMessageHelper.GetDataByteCountFromSystemExclusive7MessageFirstWord(message.Word0);

                    foreach (var b in MidiMessageHelper.GetDataBytesFromSingleSystemExclusive7Message(message.Word0, message.Word1))
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

                //if (messageType == MidiMessageType.DataMessage64)
                //{
                //}
                //else if (MessageIsSysEx8(message))
                //{
                //}
                //else
                //{
                //}
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

            if (m_verbose)
            {
                if (MidiMessageHelper.MessageTypeHasGroupField(messageType))
                {
                    groupText = MidiMessageHelper.GetGroupFromMessageFirstWord(message.Word0).DisplayValue.ToString().PadLeft(2);
                }
            }

            string channelText = string.Empty;

            if (m_verbose)
            {
                if (MidiMessageHelper.MessageTypeHasChannelField(messageType))
                {
                    channelText = MidiMessageHelper.GetChannelFromMessageFirstWord(message.Word0).DisplayValue.ToString().PadLeft(2);
                }
            }

            // some cleanup


            string offsetValueText = offsetValue.ToString("F2");
            string deltaValueText = "";

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

            if (deltaValueText.Length > timestampOffsetValueColumnWidth)
            {
                deltaValueText = "######";
                deltaUnitLabel = "";
                Columns[m_deltaColumnNumber].DataColor = m_deltaValueErrorColor;
            }
            else
            {
                Columns[m_deltaColumnNumber].DataColor = m_deltaValueDefaultColor;
            }

            if (offsetValueText.Length > timestampOffsetValueColumnWidth)
            {
                offsetValueText = "######";
                offsetUnitLabel = "";
                Columns[m_offsetColumnNumber].DataColor = m_offsetValueErrorColor;
            }
            else
            {
                Columns[m_offsetColumnNumber].DataColor = m_offsetValueDefaultColor;
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
                    groupText,
                    channelText,
                    decodedData,
                    detailedMessageType
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
                    groupText,
                    channelText,
                    decodedData,
                    detailedMessageType
                    );

                UInt64 outputEnd = MidiClock.Now;
            }
        }
    }


}
