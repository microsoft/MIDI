using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    // TODO: Add theme support here to allow for no colors, or colors which work on a lighter console.
    // theme should be saved in a prefs file
    // individual themes could be json files or, to keep things simple, just resources here
    // valid color codes https://spectreconsole.net/appendix/colors

    internal class AnsiMarkupFormatter
    {
        public static string FormatRowIndex(UInt32 index)
        {
            return "[grey]" + index.ToString() + "[/]";
        }

        public static string FormatMessageType(MidiMessageType messageType)
        {
            return "[darkseagreen3]" + messageType.ToString() + "[/]";
        }

        public static string FormatDetailedMessageType(string detailedMessageType)
        {
            return "[steelblue1_1]" + detailedMessageType + "[/]";
        }


        public static string FormatAppTitle(string title)
        {
            return "[deepskyblue1]" + title + "[/]";
        }

        public static string FormatAppDescription(string description)
        {
            return "[deepskyblue2]" + description + "[/]";
        }


        public static string FormatError(string error)
        {
            return "[red]" + error + "[/]";
        }

        public static string FormatSuccess(string message)
        {
            return "[green]" + message + "[/]";
        }

        public static string FormatTimestamp(UInt64 timestamp)
        {
            return "[darkseagreen2]" + timestamp.ToString() + "[/]";
        }

        public static string FormatDeviceInstanceId(string id)
        {
            return "[olive]" + id.Trim() + "[/]";
        }

        public static string FormatEndpointName(string name)
        {
            return "[steelblue1_1]" + name.Trim() + "[/]";
        }

        public static string FormatGeneralNumber(UInt64 i)
        {
            return "[olive]" + i.ToString() + "[/]";
        }

        public static string FormatGeneralNumber(double d)
        {
            return "[olive]" + d.ToString() + "[/]";
        }



        public static string FormatMidiWords(params UInt32[] words)
        {
            string output = string.Empty;

            string[] colors = { "[deepskyblue1]", "[deepskyblue2]", "[deepskyblue3]", "[deepskyblue4]" };

            for (int i = 0; i < words.Length; i++)
            {
                output += string.Format(colors[i]+"{0:X8}[/] ", words[i]);

            }

            return output.Trim();
        }






    }
}
