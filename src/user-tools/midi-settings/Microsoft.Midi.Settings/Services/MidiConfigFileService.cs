using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    class MidiConfigFileService : IMidiConfigFileService
    {
        private const string DefaultConfigurationName = "Default";
        private const string ConfigExtension = ".midiconfig.json";
        private const string ConfigFileLocation = @"%allusersprofile%\Microsoft\MIDI";

        private string m_currentConfigName;
        private string m_currentConfigFileName;

        public string GetDefaultConfigName()
        {
            return DefaultConfigurationName;
        }

        public string BuildConfigFileName(string configName)
        {
            var cleanedConfigurationName = CleanupConfigName(configName);

            cleanedConfigurationName = string.Join("", cleanedConfigurationName.Split(Path.GetInvalidFileNameChars()));
            cleanedConfigurationName = string.Join("", cleanedConfigurationName.Split(Path.GetInvalidPathChars()));

            return cleanedConfigurationName + ConfigExtension;
        }

        public string CleanupConfigName(string configName)
        {
            var cleanedConfigurationName = string.Join("", configName.Split(['.', ':', ';', '`', '%', '@', '#', '&', '$', '+', ',', '"', '\'', '{', '}', '[', ']'])).Trim();

            if (string.IsNullOrEmpty(cleanedConfigurationName))
            {
                return DefaultConfigurationName;
            }

            return cleanedConfigurationName;
        }

        public bool ConfigExists(string configName)
        {
            var fullPath = Path.Combine(ConfigFileLocation, BuildConfigFileName(configName));

            return Path.Exists(fullPath);
        }

        public bool CreateNewConfigFile(string configName)
        {
            // TODO

            // cleanup the names

            // create the file

            if (ConfigExists(configName))
            {
                return false;
            }

            // create the config header

            // create section for transports




            throw new NotImplementedException();
        }

        public bool SetCurrentConfig(string configName)
        {
            // Switch to this file as the current config, and so set the registry entry for the file




            return false;
        }

        public string GetConfigFilePath()
        {
            return Environment.ExpandEnvironmentVariables(ConfigFileLocation);
        }

        public string GetCurrentConfigName()
        {
            return m_currentConfigName;
        }

        public string GetCurrentConfigFileName()
        {
            return m_currentConfigFileName;
        }



        // TODO: Methods to add common functions to the config, like a new endpoint name, or new loopbacks
        // expose strongly typed stuff here, and let this class take care of the details within
        // each discrete function should result in a commit to the file.




    }
}
