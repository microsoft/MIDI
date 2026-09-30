// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Globalization;
using System.Text;

namespace WindowsMidiServices
{
    internal static class LoopbackNaming
    {
        // Loopback endpoints also create MIDI 1.0 ports, and a WinMM port name cannot be longer
        // than this, so names are trimmed here rather than being silently truncated later.
        internal const int MaxPortNameLength = 31;

        // The base name is shortened by however much the side format adds, so the finished name
        // still fits.
        internal static string FormatSideName(string format, string baseName)
        {
            var added = string.Format(CultureInfo.CurrentCulture, format, string.Empty).Length;

            var root = Truncate(baseName, Math.Max(0, MaxPortNameLength - added));

            return string.Format(CultureInfo.CurrentCulture, format, root);
        }

        internal static string Truncate(string name, int maxLength)
        {
            name = name.Trim();

            return name.Length <= maxLength ? name : name.Substring(0, maxLength);
        }

        // The unique identifier ends up inside the endpoint device id, so anything which is not
        // a letter or a digit is dropped.
        internal static string CleanUniqueId(string source)
        {
            var builder = new StringBuilder(source.Length);

            foreach (var ch in source)
            {
                if (char.IsAsciiLetterOrDigit(ch))
                {
                    builder.Append(ch);
                }
            }

            return builder.ToString();
        }

        internal static string NewUniqueId()
        {
            return CleanUniqueId(Guid.NewGuid().ToString());
        }
    }
}
