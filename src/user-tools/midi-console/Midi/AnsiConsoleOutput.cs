using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class AnsiConsoleOutput
    {


        public static void DisplayMidiMessageTableHeaders()
        {
            // this is all a manually-created table because the normal built-in tables
            // can't keep up with rendering for endpoint monitoring.


          //  string leftSideTee = "\u2502";
          //  string rightSideTee = "\u2502";
          //  string verticalLine = "\u2502";
            char horizontalLine = '\u2500';
            string cross = "\u253C";

            string headerLineFormat =
                "[steelblue1]{0,8}[/] \u2502 " +
                "[steelblue1]{1,19}[/] \u2502 " +
                "[steelblue1]{2,7}[/] \u2502 " +
                "[steelblue1]{3,-8}[/] {4,8} {5,8} {6,8} \u2502 " +
                "[steelblue1]{7,2}[/]|[steelblue1]{8,2}[/] \u2502 " +
                "[steelblue1]{9,-35}[/]";

            string headerSparatorLine =
                new string(horizontalLine, 8 + 1) + cross + new string(horizontalLine, 1) +
                new string(horizontalLine, 19 + 1) + cross + new string(horizontalLine, 1) +
                new string(horizontalLine, 7 + 3 + 1) + cross + new string(horizontalLine, 1) +
                new string(horizontalLine, 8 * 4 + 4) + cross + new string(horizontalLine, 1) +
                new string(horizontalLine, 5 + 1) + cross + new string(horizontalLine, 1) +
                new string(horizontalLine, 35 + 1);

            // TODO: Localize
            AnsiConsole.MarkupLine(
                headerLineFormat,
                "Index",
                "Timestamp",
                "    " + "Offset",
                "Words", "", "", "",
                "Gr", "Ch", 
                "Message Type"
                );

            AnsiConsole.MarkupLine(headerSparatorLine);
        }

        public static void DisplayMidiMessageTableRow(MidiMessageStruct ump, uint numWords, double offsetMicroseconds, UInt64 timestamp, uint index)
        {
            string data = string.Empty;

            string detailedMessageType = MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(ump.Word0);

            string word0Text = string.Empty;
            string word1Text = string.Empty;
            string word2Text = string.Empty;
            string word3Text = string.Empty;


            if (numWords >= 4)
            {
                word3Text = string.Format("{0:X8}", ump.Word3);
            }

            if (numWords >= 3)
            {
                word2Text = string.Format("{0:X8}", ump.Word2);
            }

            if (numWords >= 2)
            {
                word1Text = string.Format("{0:X8}", ump.Word1);
            }

            word0Text = string.Format("{0:X8}", ump.Word0);

            const string offsetUnitMicroseconds = "μs";
            const string offsetUnitMilliseconds = "ms";
            const string offsetUnitSeconds = "s ";

            double offsetValue;
            string unitLabel;

            if (offsetMicroseconds > 1000000)
            {
                unitLabel = offsetUnitSeconds;
                offsetValue = offsetMicroseconds / 1000000;
            }
            else if (offsetMicroseconds > 1000)
            {
                unitLabel = offsetUnitMilliseconds;
                offsetValue = offsetMicroseconds / 1000;
            }
            else
            {
                unitLabel = offsetUnitMicroseconds;
                offsetValue = offsetMicroseconds;
            }


            string groupText = string.Empty;

            var messageType = MidiMessageUtility.GetMessageTypeFromFirstMessageWord(ump.Word0);

            if (MidiMessageUtility.MessageTypeHasGroupField(messageType))
            {
                groupText = MidiMessageUtility.GetGroup(ump.Word0).NumberForDisplay.ToString().PadLeft(2);
            }

            string channelText = string.Empty;

            if (MidiMessageUtility.MessageTypeHasChannelField(messageType))
            {
                channelText = MidiMessageUtility.GetChannel(ump.Word0).NumberForDisplay.ToString().PadLeft(2);
            }


            string messageLineFormat =
                "[grey]{0,8}[/] \u2502 " +
                "[darkseagreen2]{1,19:N0}[/] \u2502 " +
                "[darkseagreen2]{2,7:F2}[/] " + unitLabel + " \u2502 " +
                "[deepskyblue1]{3,8}[/] [deepskyblue2]{4,8}[/] [deepskyblue3]{5,8}[/] [deepskyblue4]{6,8}[/] \u2502 " +
                "[indianred]{7, 2}[/] [mediumorchid3]{8, 2}[/] \u2502 " +
                "[steelblue1_1]{9,-35}[/]";


            AnsiConsole.MarkupLine(
                messageLineFormat,
                index,
                timestamp,
                offsetValue,
                word0Text, word1Text, word2Text, word3Text,
                groupText,
                channelText,
                detailedMessageType
                );

            //detailedMessageType.Substring(0, 35)
        }
    }
}
