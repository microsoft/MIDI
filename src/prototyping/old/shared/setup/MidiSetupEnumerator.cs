// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Setup
{
    public class MidiSetupEnumerator
    {


        // TODO: Need to see if this is ok to do from service/app, or really this needs to 
        // be done from the installer
        private void CreateAppDataFolder()
        {
            //string appFolderPath = FileLocations.AppFolder;
            //string setupsFolderPath = FileLocations.SetupsFolder;

            //try
            //{
            //    if (!Directory.Exists(appFolderPath))
            //    {
            //        Directory.CreateDirectory(appFolderPath);
            //    }

            //    if (!Directory.Exists(setupsFolderPath))
            //    {
            //        Directory.CreateDirectory(setupsFolderPath);
            //    }
            //}
            //catch (Exception ex)
            //{
            //    throw new Exception("Could not create appdata folders to store setups. Please verify the folder exists and you have permissions to write to it. Also check your anti-malware protection settings. Folder: " + setupsFolderPath, ex);
            //}
        }

        //public IEnumerable<MidiSetupFile> GetAllSetups()
        //{
        //    string[] fileNames = Directory.GetFiles(SetupsFolderPath);

        //    List<MidiSetupFile> setupFiles = new List<MidiSetupFile>();

        //    foreach (string fileName in fileNames)
        //    {
        //        // open file and verify that it is a setup file

        //        // get the header properties from the file (name, version, etc.)


        //    }

        //    return setupFiles;
        //}




    }
}
