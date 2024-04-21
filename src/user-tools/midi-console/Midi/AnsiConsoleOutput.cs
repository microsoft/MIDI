// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
{
    internal class AnsiConsoleOutput
    {

        public static void ConvertTicksToFriendlyTimeUnit(UInt64 ticksToConvert, out double convertedValue, out string convertedUnitLabel, bool longLabel = false)
        {
            // TODO: These should likely be localized

            string offsetUnitNanoseconds = "ns";
            string offsetUnitMicroseconds = "μs";
            string offsetUnitMilliseconds = "ms";
            string offsetUnitSeconds = "s";
            string offsetUnitMinutes = "mn";
            string offsetUnitHours = "hr";
            string offsetUnitDays = "dd";
            string offsetUnitYears = "yy";

            string offsetUnitNanosecondsLong = "nanoseconds";
            string offsetUnitMicrosecondsLong = "microseconds";
            string offsetUnitMillisecondsLong = "milliseconds";
            string offsetUnitSecondsLong = "seconds";
            string offsetUnitMinutesLong = "minutes";
            string offsetUnitHoursLong = "hours";
            string offsetUnitDaysLong = "days";
            string offsetUnitYearsLong = "years";


            const double secondsPerMinute = 60;
            const double secondsPerHour = secondsPerMinute * 60;
            const double secondsPerDay = secondsPerHour * 24;
            const double secondsPerYear = secondsPerDay * 365;  // yes, this is approximate, but good enough for where it is used

            // these just make the code much more readible
            double ticksPerSecond = MidiClock.TimestampFrequency;
            double ticksPerMinute = MidiClock.TimestampFrequency * secondsPerMinute;
            double ticksPerHour = MidiClock.TimestampFrequency * secondsPerHour;
            double ticksPerDay = MidiClock.TimestampFrequency * secondsPerDay;
            double ticksPerYear = MidiClock.TimestampFrequency * secondsPerYear;

            double ticksPerMillisecond = MidiClock.TimestampFrequency / 1000;
            double ticksPerMicrosecond = MidiClock.TimestampFrequency / 1000000;
            double ticksPerNanosecond = MidiClock.TimestampFrequency / 1000000000;


            if (ticksToConvert == 0)
            {
                convertedValue = 0;
                convertedUnitLabel = "--";
            }
            else if (ticksToConvert > ticksPerYear)
            {
                // convert to days
                convertedUnitLabel = longLabel ? offsetUnitYearsLong : offsetUnitYears;
                convertedValue = (double)ticksToConvert / ticksPerYear;

            }
            else if (ticksToConvert > ticksPerDay)
            {
                // convert to days
                convertedUnitLabel = longLabel ? offsetUnitDaysLong : offsetUnitDays;
                convertedValue = (double)ticksToConvert / ticksPerDay;

            }
            else if (ticksToConvert > ticksPerHour)
            {
                // convert to hours

                convertedUnitLabel = longLabel ? offsetUnitHoursLong : offsetUnitHours;
                convertedValue = (double)ticksToConvert / ticksPerHour;
            }
            else if (ticksToConvert > ticksPerMinute)
            {
                // convert to minutes

                convertedUnitLabel = longLabel ? offsetUnitMinutesLong : offsetUnitMinutes;
                convertedValue = (double)ticksToConvert / ticksPerMinute;
            }
            else if (ticksToConvert > ticksPerSecond)
            {
                // convert to seconds

                convertedUnitLabel = longLabel ? offsetUnitSecondsLong : offsetUnitSeconds;
                convertedValue = (double)ticksToConvert / ticksPerSecond;
            }
            else if (ticksToConvert > ticksPerMillisecond)
            {
                // convert to milliseconds
                convertedUnitLabel = longLabel ? offsetUnitMillisecondsLong : offsetUnitMilliseconds;
                convertedValue = (double)ticksToConvert / ticksPerMillisecond;
            }
            else if (ticksToConvert > ticksPerMicrosecond)
            {
                // convert to microseconds
                convertedUnitLabel = longLabel ? offsetUnitMicrosecondsLong : offsetUnitMicroseconds;
                convertedValue = (double)ticksToConvert / ticksPerMicrosecond;
            }
            else
            {
                // convert to nanoseconds
                convertedUnitLabel = longLabel ? offsetUnitNanosecondsLong : offsetUnitNanoseconds;
                convertedValue = (double)ticksToConvert / ticksPerNanosecond;
            }

        }

        //public static void DisplayMidiMessageTableHeaders(bool verbose)
        //{
        //    // this is all a manually-created table because the normal built-in tables
        //    // can't keep up with rendering for endpoint monitoring.


        //  //  string leftSideTee = "\u2502";
        //  //  string rightSideTee = "\u2502";
        //  //  string verticalLine = "\u2502";
        //    char horizontalLine = '\u2500';
        //    string cross = "\u253C";

        //    if (verbose)
        //    {
        //        string headerLineFormat =
        //            "[steelblue1]{0,8}[/] \u2502 " +                            // index
        //            "[steelblue1]{1,19}[/] \u2502 " +                           // timestamp
        //            "[steelblue1]{2,7}[/] \u2502 " +                            // offset
        //            "[steelblue1]{3,7}[/] \u2502 " +                            // Delta
        //            "[steelblue1]{4,-8}[/] {5,8} {6,8} {7,8} \u2502 " +         // words
        //            "[steelblue1]{8,2}[/]|[steelblue1]{9,2}[/] \u2502 " +       // group and channel
        //            "[steelblue1]{10,-35}[/]";                                  // Message type



        //        string headerSparatorLine =
        //            new string(horizontalLine, 8 + 1) + cross + new string(horizontalLine, 1) +         // index
        //            new string(horizontalLine, 19 + 1) + cross + new string(horizontalLine, 1) +        // timestamp
        //            new string(horizontalLine, 7 + 3 + 1) + cross + new string(horizontalLine, 1) +     // offset
        //            new string(horizontalLine, 7 + 3 + 1) + cross + new string(horizontalLine, 1) +     // delta
        //            new string(horizontalLine, 8 * 4 + 4) + cross + new string(horizontalLine, 1) +     // words
        //            new string(horizontalLine, 5 + 1) + cross + new string(horizontalLine, 1) +         // group and channel
        //            new string(horizontalLine, 35 + 1);                                                 // Message type

        //        // TODO: Localize
        //        AnsiConsole.MarkupLine(
        //            headerLineFormat,
        //            "Index",
        //            "Message Timestamp",
        //            "    " + "Offset",
        //            "" + "Received \u0394",
        //            "Words", "", "", "",
        //            "Gr", "Ch",
        //            "Message Type"
        //            );

        //    }


        //    AnsiConsole.MarkupLine(headerSparatorLine);
        //}

        //public static void DisplayMidiMessageTableRow(ReceivedMidiMessage message, double deltaMicrosecondsFromPreviousMessage, bool verbose)
        //{
        //    string data = string.Empty;

        //    string detailedMessageType = MidiMessageUtility.GetMessageFriendlyNameFromFirstWord(message.Word0);

        //    string word0Text = string.Empty;
        //    string word1Text = string.Empty;
        //    string word2Text = string.Empty;
        //    string word3Text = string.Empty;


        //    if (message.NumWords >= 4)
        //    {
        //        word3Text = string.Format("{0:X8}", message.Word3);
        //    }

        //    if (message.NumWords >= 3)
        //    {
        //        word2Text = string.Format("{0:X8}", message.Word2);
        //    }

        //    if (message.NumWords >= 2)
        //    {
        //        word1Text = string.Format("{0:X8}", message.Word1);
        //    }

        //    word0Text = string.Format("{0:X8}", message.Word0);


        //    double offsetValue;
        //    string offsetUnitLabel;

        //    ConvertToFriendlyTimeUnit(deltaMicrosecondsFromPreviousMessage, out offsetValue, out offsetUnitLabel, false);


        //    double deltaValue;
        //    string deltaUnitLabel;

        //    double deltaSecheduledTimestampMicroseconds = 0.0;

        //    if (message.ReceivedTimestamp >= message.MessageTimestamp)
        //    {
        //        deltaSecheduledTimestampMicroseconds = MidiClock.ConvertTimestampToMicroseconds(message.ReceivedTimestamp - message.MessageTimestamp);
        //    }
        //    else
        //    {
        //        // we received the message early, so the offset is negative
        //        deltaSecheduledTimestampMicroseconds = -1 * MidiClock.ConvertTimestampToMicroseconds(message.MessageTimestamp - message.ReceivedTimestamp);
        //    }


        //    ConvertToFriendlyTimeUnit(deltaSecheduledTimestampMicroseconds, out deltaValue, out deltaUnitLabel, true);


        //    string groupText = string.Empty;

        //    var messageType = MidiMessageUtility.GetMessageTypeFromMessageFirstWord(message.Word0);

        //    if (MidiMessageUtility.MessageTypeHasGroupField(messageType))
        //    {
        //        groupText = MidiMessageUtility.GetGroupFromMessageFirstWord(message.Word0).NumberForDisplay.ToString().PadLeft(2);
        //    }

        //    string channelText = string.Empty;

        //    if (MidiMessageUtility.MessageTypeHasChannelField(messageType))
        //    {
        //        channelText = MidiMessageUtility.GetChannelFromMessageFirstWord(message.Word0).NumberForDisplay.ToString().PadLeft(2);
        //    }


        //    string messageLineFormat =
        //        "[grey]{0,8}[/] \u2502 " +
        //        "[darkseagreen2]{1,19:N0}[/] \u2502 " +
        //        "[darkseagreen2]{2,7:F2}[/] " + offsetUnitLabel + " \u2502 " +
        //        "[darkseagreen2]{3,7:F2}[/] " + deltaUnitLabel + " \u2502 " +
        //        "[deepskyblue1]{4,8}[/] [deepskyblue2]{5,8}[/] [deepskyblue3]{6,8}[/] [deepskyblue4]{7,8}[/] \u2502 " +
        //        "[indianred]{8, 2}[/] [mediumorchid3]{9, 2}[/] \u2502 " +
        //        "[steelblue1_1]{10,-35}[/]";


        //    AnsiConsole.MarkupLine(
        //        messageLineFormat,
        //        message.Index,
        //        message.MessageTimestamp,
        //        offsetValue,
        //        deltaValue,
        //        word0Text, word1Text, word2Text, word3Text,
        //        groupText,
        //        channelText,
        //        detailedMessageType
        //        );

        //    //detailedMessageType.Substring(0, 35)
        //}
    }
}
