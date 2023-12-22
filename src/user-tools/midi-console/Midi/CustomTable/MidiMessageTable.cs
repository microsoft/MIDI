using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal struct MidiMessageTableColumn
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

        public List<MidiMessageTableColumn> Columns { get; private set; } = new List<MidiMessageTableColumn>();

        private void BuildStringFormats()
        {
            char horizontalLine = '\u2500';
            string cross = "\u253C";

            foreach (var col in Columns)
            {
                if (_headerFormat != string.Empty && !col.NoLeftSeparator)
                {
                    _headerFormat += " \u2502 ";
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
                    _messageFormat += " \u2502 ";
                }
                else if (col.NoLeftSeparator)
                {
                    _messageFormat += " ";
                }

                if (col.DataFormat == string.Empty)
                {
                    _messageFormat += $"[{col.DataColor}]{{{col.ParameterIndex},{col.Width}}}[/]";
                }
                else
                {
                    _messageFormat += $"[{col.DataColor}]{{{col.ParameterIndex},{col.Width}:{col.DataFormat}}}[/]";

                }

            }

        }
       
        public MidiMessageTable(bool verbose)
        {
            // todo: localize strings

            Columns.Add(new MidiMessageTableColumn(0, "Index", 8, true, "grey", ""));
            Columns.Add(new MidiMessageTableColumn(1, "Message Timestamp", 19, false, "darkseagreen2", "N0"));
            Columns.Add(new MidiMessageTableColumn(2, "Offset", 7, false, "darkseagreen2", "F2"));
            Columns.Add(new MidiMessageTableColumn(3, "", 2, true, "darkseagreen2", ""));                // offset label
            if (verbose) Columns.Add(new MidiMessageTableColumn(4, "Recv \u0394", 7, false, "darkseagreen2", "F2"));
            if (verbose) Columns.Add(new MidiMessageTableColumn(5, "", 2, true, "darkseagreen2", ""));                // offset label
            Columns.Add(new MidiMessageTableColumn(6, "Words", 8, false, "deepskyblue1", ""));
            Columns.Add(new MidiMessageTableColumn(7, "", 8, true, "deepskyblue2", ""));
            Columns.Add(new MidiMessageTableColumn(8, "", 8, true, "deepskyblue3", ""));
            Columns.Add(new MidiMessageTableColumn(9, "", 8, true, "deepskyblue4", ""));
            if (verbose) Columns.Add(new MidiMessageTableColumn(10, "Gr", 2, false, "indianred", ""));
            if (verbose) Columns.Add(new MidiMessageTableColumn(11, "Ch", 2, true, "mediumorchid3", ""));
            if (verbose) Columns.Add(new MidiMessageTableColumn(12, "Message Type", -35, false,"steelblue1_1", ""));

            BuildStringFormats();
        }

        public void OutputHeader()
        {
            AnsiConsole.MarkupLine(_headerFormat);
            AnsiConsole.MarkupLine(_separatorLine);
        }

        public void OutputRow(ReceivedMidiMessage message, double deltaMicrosecondsFromPreviousMessage)
        {
            string data = string.Empty;

            string detailedMessageType = MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(message.Word0);

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

            AnsiConsoleOutput.ConvertToFriendlyTimeUnit(deltaMicrosecondsFromPreviousMessage, out offsetValue, out offsetUnitLabel, false);


            double deltaValue = 0;
            string deltaUnitLabel = string.Empty;

            double deltaSecheduledTimestampMicroseconds = 0.0;

            if (message.ReceivedTimestamp >= message.MessageTimestamp)
            {
                deltaSecheduledTimestampMicroseconds = MidiClock.ConvertTimestampToMicroseconds(message.ReceivedTimestamp - message.MessageTimestamp);
            }
            else
            {
                // we received the message early, so the offset is negative
                deltaSecheduledTimestampMicroseconds = -1 * MidiClock.ConvertTimestampToMicroseconds(message.MessageTimestamp - message.ReceivedTimestamp);
            }


            AnsiConsoleOutput.ConvertToFriendlyTimeUnit(deltaSecheduledTimestampMicroseconds, out deltaValue, out deltaUnitLabel, true);


            string groupText = string.Empty;

            var messageType = MidiMessageUtility.GetMessageTypeFromMessageFirstWord(message.Word0);

            if (MidiMessageUtility.MessageTypeHasGroupField(messageType))
            {
                groupText = MidiMessageUtility.GetGroupFromMessageFirstWord(message.Word0).NumberForDisplay.ToString().PadLeft(2);
            }

            string channelText = string.Empty;

            if (MidiMessageUtility.MessageTypeHasChannelField(messageType))
            {
                channelText = MidiMessageUtility.GetChannelFromMessageFirstWord(message.Word0).NumberForDisplay.ToString().PadLeft(2);
            }

            AnsiConsole.MarkupLine(
                _messageFormat,
                message.Index,
                message.MessageTimestamp,
                offsetValue,
                offsetUnitLabel,
                deltaValue,
                deltaUnitLabel,
                word0, word1, word2, word3,
                groupText,
                channelText,
                detailedMessageType
                );

        }
    }


}
