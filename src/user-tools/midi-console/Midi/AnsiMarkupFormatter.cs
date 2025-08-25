// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.AI.MachineLearning;
using Windows.Devices.Enumeration;


namespace Microsoft.Midi.ConsoleApp
{
    // TODO: Add theme support here to allow for no colors, or colors which work on a lighter console.
    // theme should be saved in a prefs file
    // individual themes could be json files or, to keep things simple, just resources here
    // valid color codes https://spectreconsole.net/appendix/colors

    internal class AnsiMarkupFormatter
    {
        public static string FormatGroupSpan(byte firstGroupIndex, byte groupSpanCount)
        {
            var group = new MidiGroup(firstGroupIndex);

            if (groupSpanCount > 1)
            {
                return $"{MidiGroup.LongLabel}s {FormatGeneralNumber(group.DisplayValue)}-{FormatGeneralNumber(group.DisplayValue + groupSpanCount-1)}";
            }
            else
            {
                return $"{MidiGroup.LongLabel} {FormatGeneralNumber(group.DisplayValue)}";
            }
        }


        public static void SetTableBorderStyle(Table table)
        {
            table.Border(TableBorder.Rounded);
            table.BorderColor(Color.Grey35);
        }

        public static string GetEndpointIcon(MidiEndpointDevicePurpose purpose)
        {
            switch (purpose)
            {
                case MidiEndpointDevicePurpose.DiagnosticPing:
                    return "📶";
                case MidiEndpointDevicePurpose.DiagnosticLoopback:
                    return "🔁";
                case MidiEndpointDevicePurpose.NormalMessageEndpoint:
                    return "🎹";
                case MidiEndpointDevicePurpose.VirtualDeviceResponder:
                    return "💻";
                default:
                    return "🎵";
            }
        }
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

        public static string FormatAppVersionInformation(string versionInformation)
        {
            return "[grey35]" + EscapeString(versionInformation) + "[/]";
        }

        public static string FormatSdkVersionInformation(string versionInformation)
        {
            return "[darkseagreen3]" + EscapeString(versionInformation) + "[/]";
        }

        

        public static string FormatAppDescription(string description)
        {
            return "[deepskyblue2]" + EscapeString(description) + "[/]";
        }

        public static string FormatPropertySectionDescription(string description)
        {
            return "[grey35]" + EscapeString(description) + "[/]";
        }


        public static string FormatError(string error)
        {
            return "[red]" + EscapeString(error) + "[/]";
        }

        public static string FormatWarning(string warning)
        {
            return "[yellow]" + EscapeString(warning) + "[/]";
        }

        public static string FormatSuccess(string message)
        {
            return "[greenyellow]" + EscapeString(message) + "[/]";
        }




        public static string FormatTimestamp(UInt64 timestamp)
        {
            return "[darkseagreen2]" + timestamp.ToString("N0") + "[/]";
        }

        public static string FormatDeviceInstanceId(string id)
        {
            return "[darkolivegreen3]" + EscapeString(id.Trim()) + "[/]";
        }

        public static string FormatDeviceKind(DeviceInformationKind kind)
        {
            return "[lightsalmon3]" + EscapeString(kind.ToString()) + "[/]";
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


        public static string FormatPortIndex(uint index)
        {
            //return "[orange3]" + index.ToString().PadLeft(3) + "[/]";

            // we're optimizing for the normal case of < 100 ports on a system
            return "[orange3]" + index.ToString().PadLeft(2) + "[/]";
        }

        public static string FormatPortName(string name)
        {
            return "[steelblue1_1]" + EscapeString(name.Trim()) + "[/]";
        }


        public static string FormatBlockNumberLabel(int number)
        {
            return "[orange3]" + number.ToString().PadLeft(2) + "[/]";
        }

        public static string FormatBlockNumberValue(int number)
        {
            return "[orange3]" + number.ToString() + "[/]";
        }


        public static string FormatBlockName(string name)
        {
            return "[steelblue1_1]" + EscapeString(name.Trim()) + "[/]";
        }

        public static string FormatEndpointName(string name)
        {
            return "[steelblue1_1]" + EscapeString(name) + "[/]";
        }

        public static string FormatManufacturerName(string name)
        {
            return "[steelblue3]" + EscapeString(name) + "[/]";
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

        public static string FormatFileName(string fileName)
        {
            return "[steelblue1]" + EscapeString(fileName) + "[/]";
        }



        public static string FormatProcessName(string processName)
        {
            return "[steelblue1_1]" + EscapeString(processName) + "[/]";
        }

        public static string FormatProcessId(UInt64 pid)
        {
            return "[steelblue]" + pid.ToString() + "[/]";
        }

        public static string FormatSessionName(string sessionName)
        {
            return "[deepskyblue1]" + EscapeString(sessionName) + "[/]";
        }

        public static string FormatLongDateTime(DateTimeOffset time)
        {
            // Sunday, December 31, 1600 7:00:00 PM is the "empty" date value for a foundation::DateTime
            if (time.Year <= 1600)
            {
                return string.Empty;
            }
            else
            {
                return "[cadetblue]" + time.DateTime.ToLongDateString() + "[/] [cadetblue]" + time.DateTime.ToLongTimeString() + "[/]";
            }
        }

        public static string FormatMidi1Note(byte noteIndex)
        {
            return $"[deepskyblue1]{noteIndex}[/]";
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


        public static string FormatFriendlyPropertyKey(string key)
        {
            return "[deepskyblue2]" + key + "[/]";
        }

        public static string FormatUnrecognizedPropertyKey(string key)
        {
            return "[grey]" + key + "[/]";
        }



        public static string FormatTransportName(string name)
        {
            return "[darkgoldenrod]" + EscapeString(name.Trim()) + "[/]";
        }

        public static string FormatTransportDescription(string description)
        {
            return "[grey]" + EscapeString(description.Trim()) + "[/]";
        }

        public static string FormatTransportId(Guid id)
        {
            return "[darkseagreen]" + id.ToString() + "[/]";
        }

        public static string FormatTransportMnemonic(string mnemonic)
        {
            return "[darkgoldenrod]" + EscapeString(mnemonic.Trim()) + "[/]";
        }

        public static string FormatTransportVersion(string version)
        {
            return "[skyblue2]" + EscapeString(version.Trim()) + "[/]";
        }

        public static string FormatTransportAuthor(string author)
        {
            return "[lightslateblue]" + EscapeString(author.Trim()) + "[/]";
        }


    }
}
