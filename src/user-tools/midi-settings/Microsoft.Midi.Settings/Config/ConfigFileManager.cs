// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Extensions.Hosting;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Config
{
    internal class ConfigFileManager
    {

        // should likely take a path
        public void ChangeActiveConfigFile(string configFileName)
        {

        }

        // should likely return a path
        public string GetConfigFileFolder()
        {
            return string.Empty;
        }

        // should likely return a path or similar object
        public string GetActiveConfigFileName()
        {
            // gets the active config file name from the registry

            return string.Empty;

        }

        public void BackupCurrentConfigFile()
        {
            // backs up the current config file using a numbering scheme


        }


    }
}
