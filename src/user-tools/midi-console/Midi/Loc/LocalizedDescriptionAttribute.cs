// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    public class LocalizedDescriptionAttribute : DescriptionAttribute
    {
        public override string Description
        {
            get
            {
                //return Localize(base.Description);

                string? value = Strings.ResourceManager.GetString(base.Description);

                //System.Diagnostics.Debug.WriteLine("Resource {0}={1}", base.Description, value);

                if (value != null)
                {
                    return value;
                }
                else
                {
                    return "Missing resource: " + base.Description;
                }

            }
        }

        public LocalizedDescriptionAttribute(string key) : base(key) { }

    }
}
