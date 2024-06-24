// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    public class EnumLocalizedDescriptionAttribute : DescriptionAttribute
    {
        private Type _t;

        public override string Description
        {
            get
            {
                // this gets the prefix
                string? value = Strings.ResourceManager.GetString(base.Description);

                if (value != null)
                {
                    // this gets the enum

                    value += " " + string.Join(", ", Enum.GetNames(_t));


                    return value;
                }
                else
                {
                    return "Missing resource: " + base.Description;
                }

            }
        }

        public EnumLocalizedDescriptionAttribute(string key, Type t) : base(key) { _t = t; }

    }
}