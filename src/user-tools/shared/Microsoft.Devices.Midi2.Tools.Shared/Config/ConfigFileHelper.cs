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
    public class ConfigFileHelper
    {
       


        public static string? GetConfigFileFullPath()
        {
            // return the full path to the config file

            var fileName = RegistryHelper.GetCurrentConfigFileName();

            if (fileName != null)
            {
                // concat the known path + file name and return it

                const string rootFolder = @"%ALLUSERSPROFILE%\Microsoft\MIDI\";

                var fullPath = Path.Combine(Environment.ExpandEnvironmentVariables(rootFolder), fileName);

                System.Diagnostics.Debug.WriteLine(fullPath);

                return fullPath;
            }

            return null;
        }


    }
}
