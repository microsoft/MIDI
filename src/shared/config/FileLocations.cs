#pragma warning disable CA1416

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security;
using System.Text;
using System.Threading.Tasks;

namespace MidiConfig
{
    public class FileLocations
    {
        // This key needs to be created by the installer, as it requires elevated permissions

        //public const string DefaultAppFolder = "%ALLUSERSPROFILE%\\Microsoft Windows MIDI\\";

        private const string AppDataSubfolder = "Microsoft Windows MIDI";

        public const string SetupsSubFolder = "Setups";
        public const string PluginsSubFolder = "Plugins";    // will want to revisit this
        public const string IconsSubFolder = "Icons";

        public const string SetupFileExtension = "midisetup.json";
        public const string ConfigFileExtension = "midiconfig.json";

        public const string SetupFileBackupExtension = "backup.{0:0000}";
        public const string ConfigFileBackupExtension = "backup.{0:0000}";
        public const string ConfigFileBackupExtensionSearch = "midiconfig.backup.????";

        //       private const string ConfigFileBackupSearchPattern = "midiconfig.json.????";


        // this key can only be created by an admin acct. Originally, this was HKLU but 
        // everything else is system-wide, so that made no sense.
        //public const string RegKey = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\MIDI\\";
        //public const string ConfigFileRegValueName = "MainConfig";

        //       public const string DefaultConfigFileName = DefaultAppFolder + "MainConfig." + ConfigFileExtension;


        private const string ConfigFileNameWithoutPath = "MainConfig." + ConfigFileExtension;
        
        public static string AppDataFolder
        {
            get
            {
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), AppDataSubfolder);
            }
            
        }

        public static string ConfigFileFolder
        {
            get
            {
                return AppDataFolder;
            }
        }

        public static string ConfigFileName
        {
            get
            {
                return Path.Combine(AppDataFolder, ConfigFileNameWithoutPath);
            }
        }

        
        //private string? _configFileLocation;

        // get main config file location from registry. If not present there
        // default to the appdata location and then write the key

        // TODO: Need to verify permissions on the location, or at least throw appropriate
        // exceptions. It may be service can't read from / write to there, but setup can. 


        //public static string GetOrCreateConfigFileEntryInRegistry()
        //{
        //    object? location;

        //    try
        //    {
        //        location = Registry.GetValue(RegKey, ConfigFileRegValueName, null);
        //    }
        //    catch (SecurityException ex)
        //    {
        //        throw new Exception($"Insufficient registry access rights when trying to get config file entry from {RegKey} | {ConfigFileRegValueName}.", ex);
        //    }
        //    catch (Exception ex)
        //    {
        //        throw new Exception($"Unable to get config file entry from {RegKey} | {ConfigFileRegValueName}.", ex);
        //    }

        //    try
        //    {
        //        if (location != null)
        //        {
        //            string path = location as string;

        //            return path;
        //        }
        //        else
        //        {
        //            // entry didn't exist in the registry. Create it.

        //            Registry.SetValue(RegKey, ConfigFileRegValueName, DefaultConfigFileName);

        //            return DefaultConfigFileName;
        //        }
        //    }
        //    catch (SecurityException ex)
        //    {
        //        throw new Exception($"Insufficient registry access rights when trying to create missing config file entry in {RegKey} | {ConfigFileRegValueName}.", ex);
        //    }
        //    catch (Exception ex)
        //    {
        //        throw new Exception($"Unable to write config file entry to {RegKey} | {ConfigFileRegValueName}.", ex);
        //    }
        //}





        // serializable bits






    }
}
