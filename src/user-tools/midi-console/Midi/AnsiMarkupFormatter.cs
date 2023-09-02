using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class AnsiMarkupFormatter
    {
        public static string FormatError(string error)
        {
            return "[red]" + error + "[/]";
        }
        public static string FormatTimestamp(UInt64 timestamp)
        {
            return "[olive]" + timestamp.ToString() + "[/]";
        }

        public static string FormatDeviceInstanceId(string id)
        {
            return "[olive]" + id.Trim() + "[/]";
        }

        public static string FormatEndpointName(string name)
        {
            return "[steelblue1_1]" + name.Trim() + "[/]";
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
