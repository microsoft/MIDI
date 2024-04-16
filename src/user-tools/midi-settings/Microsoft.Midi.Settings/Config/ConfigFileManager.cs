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
