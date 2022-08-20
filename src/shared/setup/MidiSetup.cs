// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Config;
using Microsoft.Windows.Midi.Internal.Setup.Schema;
using System.IO;
using System.Text.Json;

namespace Microsoft.Windows.Midi.Internal.Setup
{
    public class MidiSetup
    {
        // header
        public SetupHeader Header { get; set; }


        // transports

        // devices and endpoints


        public static MidiSetup Load(string fileName)
        {
            // we ensure the file name here is just a file name and not a path
            // we only want to load setups from the official setups folder
            string fullPath = Path.Combine(FileManager.SetupsFolder, Path.GetFileName(fileName));

            if (!Directory.Exists(FileManager.SetupsFolder))
            {
                throw new Exception($"Setups folder does not exist or is inaccessible. {FileManager.SetupsFolder}.");
            }

            if (!File.Exists(fullPath))
            {
                throw new Exception($"Setups file does not exist or is inaccessible. {fullPath}.");
            }

            // Now we can load it up

            JsonSerializerOptions options = GetSerializerOptions();

            MidiSetup? setup;

            using (FileStream stream = File.OpenRead(fullPath))
            {
                setup = JsonSerializer.Deserialize<MidiSetup>(stream, options);

                if (setup == null)
                {
                    // bad file
                    throw new Exception($"Setups file could not deserialize. Corrupted? {fullPath}.");
                }
            }

            return (MidiSetup)setup;
        }


        private static JsonSerializerOptions GetSerializerOptions()
        {
            return new JsonSerializerOptions()
            {
                AllowTrailingCommas = true,
                WriteIndented = true,
            };
        }

        public static MidiSetup Create(string fileName)
        {
            // TODO: create a new file based on defaults
            // 
            throw new NotImplementedException();
        }





        public void Write()
        {
            // TODO: See if there is an easy way to write discrete changes to an existing doc
            // without parsing everything through jsondocument
        //    var options = new JsonSerializerOptions { AllowTrailingCommas = true, WriteIndented = true, } };
        }


    }
}