// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.Tools.Shared.Config
{
    public class RegistryHelper
    {
        public static RegistryKey? GetWindowsMidiServicesBaseRegKey()
        {
            var root = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);

            if (root != null)
            {
                return root.OpenSubKey(@"SOFTWARE\Microsoft\Windows MIDI Services");
            }
            else
            {
                return null;
            }

        }

        public static string? GetCurrentConfigFileName()
        {
            var wmsRegKey = GetWindowsMidiServicesBaseRegKey();

            if (wmsRegKey != null)
            {
                var value = wmsRegKey.GetValue("CurrentConfig");

                if (value != null && value.ToString() != string.Empty)
                {
                    var fileName = value!.ToString()!;

                    if (fileName.Contains('\\') || fileName.Contains('/') || fileName.Contains(':') || fileName.Contains('%'))
                    {
                        // invalid entry. We only support storing the file name, not the path

                        return null;
                    }
                    else
                    {
                        return fileName;
                    }
                }
            }

            return null;

        }


    }
}
