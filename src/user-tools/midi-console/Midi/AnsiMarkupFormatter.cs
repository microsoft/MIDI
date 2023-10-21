using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    // TODO: Add theme support here to allow for no colors, or colors which work on a lighter console.
    // theme should be saved in a prefs file
    // individual themes could be json files or, to keep things simple, just resources here
    // valid color codes https://spectreconsole.net/appendix/colors

    internal class AnsiMarkupFormatter
    {
        public static string EscapeString(string s)
        {
            return s.Replace("[", "[[").Replace("]", "]]");
        }

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
            return "[steelblue1_1]" + EscapeString(detailedMessageType) + "[/]";
        }


        public static string FormatAppTitle(string title)
        {
            return "[deepskyblue1]" + EscapeString(title) + "[/]";
        }

        public static string FormatAppDescription(string description)
        {
            return "[deepskyblue2]" + EscapeString(description) + "[/]";
        }


        public static string FormatError(string error)
        {
            return "[red]" + EscapeString(error) + "[/]";
        }

        public static string FormatSuccess(string message)
        {
            return "[green]" + EscapeString(message) + "[/]";
        }

        public static string FormatTimestamp(UInt64 timestamp)
        {
            return "[darkseagreen2]" + timestamp.ToString() + "[/]";
        }

        public static string FormatDeviceInstanceId(string id)
        {
            return "[darkolivegreen3]" + EscapeString(id.Trim()) + "[/]";
        }

        public static string FormatDeviceKind(DeviceInformationKind kind)
        {
            return "[orange4]" + EscapeString(kind.ToString()) + "[/]";
        }

        public static string FormatContainerId(string id)
        {
            return "[darkseagreen]" + EscapeString(id.Trim()) + "[/]";
        }


        public static string FormatFullEndpointInterfaceId(string id)
        {
            return "[olive]" + EscapeString(id.Trim()) + "[/]";
        }

        public static string FormatDeviceParentId(string id)
        {
            return "[orange3]" + EscapeString(id.Trim()) + "[/]";
        }


        public static string FormatEndpointName(string name)
        {
            return "[steelblue1_1]" + EscapeString(name.Trim()) + "[/]";
        }

        public static string FormatGeneralNumber(UInt64 i)
        {
            return "[olive]" + i.ToString() + "[/]";
        }

        public static string FormatGeneralNumber(double d)
        {
            return "[olive]" + d.ToString() + "[/]";
        }

        public static string FormatTableColumnHeading(string heading)
        {
            return "[steelblue1]" + EscapeString(heading) + "[/]";
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
