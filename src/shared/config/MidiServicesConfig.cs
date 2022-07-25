using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

//using System.Text.Json;
using MidiConfig.Schema;
using System.Text.Json.Serialization;
using System.Text.Json;
using System.Security.AccessControl;
using System.Security.Principal;

using Microsoft.Extensions.Logging;

namespace MidiConfig
{
    public class MidiServicesConfig
    {
        private readonly ILogger<MidiServicesConfig> _logger;
        public MidiServicesConfig(ILogger<MidiServicesConfig> logger)
        {
            _logger = logger;

            _logger.LogInformation("DEBUG: MidiServicesConfig constructor");


            CurrentSetupFileName = FileManager.DefaultSetupFileName;
        }

        public Header Header { get; set; } = new Header(); 


        public Locations Locations { get; set; } = new Locations();


        public List<Plugin> TrustedPlugins { get; set; } = new List<Plugin>();

        public string CurrentSetupFileName { get; set; }

        public void LoadDefaults()
        {
            _logger.LogDebug("MidiServicesConfig : Loading Defaults");

            // TODO: We need to get this schema version from someplace else
            Header.SchemaVersion = new Version(1, 0, 0);

            Locations.MainAppFolder = FileManager.AppDataFolder;
            Locations.SetupsFolder = FileManager.DefaultSetupsFolder;
            Locations.PluginsFolder = FileManager.DefaultPluginsFolder;
            Locations.IconsFolder = FileManager.DefaultIconsFolder;

            CurrentSetupFileName = FileManager.DefaultSetupFileName;

            FileManager.LoadDefaults();
        }

        public void Load()
        {
            // Get file path

            // deserialize
        }


        public void Save()
        {
            _logger.LogDebug("MidiServicesConfig : Saving Config File");

            CreateConfigFile();
        }


        private void LoadConfigFile()
        {
            _logger.LogDebug("MidiServicesConfig : LoadConfigFile");

            string fileName = FileManager.ConfigFileName;

            if (File.Exists(fileName))
            {
                // TODO
            }
            else
            {
                // file does not exist

                //LoadDefaults();

                CreateConfigFile();
            }

        }



        private void CreateConfigFile()
        {
            string fileName = FileManager.ConfigFileName;

            System.Diagnostics.Debug.WriteLine($"MainConfigFileLocation = {fileName}");

            string configDirectory = FileManager.ConfigFileFolder;

            if (!Directory.Exists(configDirectory))
            {
                throw new Exception($"Configuration directory is missing. {configDirectory}");
            }

            // If the config file already exists, back it up

            if (File.Exists(fileName))
            {
                System.Diagnostics.Debug.WriteLine("MainConfigFile exists}");

                FileManager.MoveFileToBackup(fileName, FileType.ConfigFile);
            }

            // write file
            JsonSerializerOptions options = GetSerializerOptions();            

            using (FileStream stream = File.Create(fileName))
            {
                JsonSerializer.Serialize<MidiServicesConfig>(stream, this, options);
            }
        }


        private JsonSerializerOptions GetSerializerOptions()
        {
            return new JsonSerializerOptions()
            {
                AllowTrailingCommas = true,
                WriteIndented = true,
            };
        }


        //private string GetMainConfigFileLocation()
        //{
        //    string path = Environment.ExpandEnvironmentVariables(FileLocations.GetOrCreateConfigFileEntryInRegistry());

        //    return path;
        //}

        //private string GetAndValidateMainConfigFileLocation()
        //{

        //    if (Directory.Exists(path))
        //    {
        //        // file is there. We're good
        //        return path;
        //    }
        //    else
        //    {
        //        // file is missing. Try to create it. It's possible we don't have permissions
        //        // so be sure to pass along any exception

        //        try
        //        {
        //            File.Create(path);

        //            return path;
        //        }
        //        catch (Exception ex)
        //        {
        //            throw new Exception($"Could not create main config file {path}", ex);
        //        }

        //    }
        //}




    }
}
