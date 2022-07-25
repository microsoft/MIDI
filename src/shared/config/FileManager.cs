#pragma warning disable CA1416

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Metadata.Ecma335;
using System.Security;
using System.Text;
using System.Threading.Tasks;

namespace MidiConfig
{
    public enum FileType
    {
        ConfigFile,
        SetupFile
    }

    public class FileManager
    {
        // This key needs to be created by the installer, as it requires elevated permissions


        private const string AppDataSubfolder = "Microsoft Windows MIDI";

        public const string DefaultSetupsSubFolder = "Setups";
        public const string DefaultPluginsSubFolder = "Plugins"; // This is in a different path
        public const string DefaultIconsSubFolder = "Icons";

        private const string SetupFileExtension = "midisetup.json";
        private const string ConfigFileExtension = "midiconfig.json";

        private const string SetupFileBackupExtensionFormat = "backup.{0:0000}";
        private const string SetupFileBackupExtensionSearchPattern = "midisetup.backup.????";

        private const string ConfigFileBackupExtensionFormat = "backup.{0:0000}";
        private const string ConfigFileBackupExtensionSearchPattern = "midiconfig.backup.????";

        private const string DefaultSetupFileNameWithoutPath = "Default." + SetupFileExtension;


        private const string ConfigFileNameWithoutPath = "MainConfig." + ConfigFileExtension;
        
        public static string AppDataFolder
        {
            get
            {
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), AppDataSubfolder);
            }
        }


        public static string DefaultConfigFolder => AppDataFolder;
        public static string DefaultSetupsFolder => Path.Combine(AppDataFolder, DefaultSetupsSubFolder);
        public static string DefaultIconsFolder => Path.Combine(AppDataFolder, DefaultIconsSubFolder);
        public static string DefaultPluginsFolder => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles), DefaultPluginsSubFolder);





        private static string? _setupsFolder;
        public static string SetupsFolder
        {
            get
            {
                if (_setupsFolder == null || _setupsFolder == string.Empty)
                {
                    _setupsFolder = DefaultSetupsFolder;
                }

                return _setupsFolder;
            }
            set
            {
                _setupsFolder = value;
            }
        }

        private static string? _configFileFolder;
        public static string ConfigFileFolder
        {
            get
            {
                if (_configFileFolder == null || ConfigFileFolder == string.Empty)
                {
                    _configFileFolder = AppDataFolder;
                }

                return _configFileFolder;
            }
            set
            {
                _configFileFolder = value;
            }
        }

        private static string? _iconsFolder;
        public static string IconsFolder
        {
            get
            {
                if (_iconsFolder == null || _iconsFolder == string.Empty)
                {
                    _iconsFolder = DefaultIconsFolder;
                }

                return _iconsFolder;
            }
            set
            {
                _iconsFolder = value;
            }
        }

        private static string? _pluginsFolder;
        public static string PluginsFolder
        {
            get
            {
                if (_pluginsFolder == null || _pluginsFolder == string.Empty)
                {
                    _pluginsFolder = DefaultPluginsFolder;
                }

                return _pluginsFolder;
            }
            set
            {
                _pluginsFolder = value;
            }
        }



        public static void LoadDefaults()
        {
            SetupsFolder = DefaultSetupsFolder;
            IconsFolder = DefaultIconsFolder;
            PluginsFolder = DefaultPluginsFolder;
        }









        public static string ConfigFileName
        {
            get
            {
                return Path.Combine(AppDataFolder, ConfigFileNameWithoutPath);
            }
        }



        public static string DefaultSetupFileName
        {
            get
            {
                return Path.Combine(AppDataFolder, DefaultSetupFileNameWithoutPath);
            }
        }




        private static string BuildBackupFileName(int index, string rootFileName, FileType fileType)
        {
            string fileName = rootFileName + "." + string.Format(GetBackupFormatForFileType(fileType), index);

            System.Diagnostics.Debug.WriteLine($"BackupConfigFileName = {fileName}");

            return fileName;
        }


        private static string GetDirectoryForFileType(FileType fileType)
        {
            switch (fileType)
            {
                case FileType.SetupFile:
                    return SetupsFolder;
                case FileType.ConfigFile:
                    return ConfigFileFolder;
                default:
                    throw new Exception($"Unexpected fileType in GetDirectoryForFileType: {fileType}");
            }
        }

        private static string GetExtensionForFileType(FileType fileType)
        {
            switch (fileType)
            {
                case FileType.SetupFile:
                    return SetupFileExtension;
                case FileType.ConfigFile:
                    return ConfigFileExtension;
                default:
                    throw new Exception($"Unexpected fileType in GetExtensionForFileType: {fileType}");
            }
        }

        private static string GetBackupFormatForFileType(FileType fileType)
        {
            switch (fileType)
            {
                case FileType.SetupFile:
                    return SetupFileBackupExtensionFormat;
                case FileType.ConfigFile:
                    return ConfigFileBackupExtensionFormat;
                default:
                    throw new Exception($"Unexpected fileType in GetBackupFormatForFileType: {fileType}");
            }
        }

        private static string GetBackupSearchPatternForFileType(FileType fileType)
        {
            switch (fileType)
            {
                case FileType.SetupFile:
                    return SetupFileBackupExtensionSearchPattern;
                case FileType.ConfigFile:
                    return ConfigFileBackupExtensionSearchPattern;
                default:
                    throw new Exception($"Unexpected fileType in GetBackupSearchPatternForFileType: {fileType}");
            }
        }


        private static int GetHighestFileBackupIndex(string rootFileName, FileType fileType)
        {
            string directory = GetDirectoryForFileType(fileType);

            var files = Directory.GetFiles(directory, rootFileName + "." + GetBackupSearchPatternForFileType(fileType)).OrderBy(path => Path.GetExtension(path));

            string highest = "0000";

            if (files.Count<string>() > 0)
            {
                highest = files.Last();
            }


            if (highest != null && highest != string.Empty)
            {
                System.Diagnostics.Debug.WriteLine($"Highest = {highest}");

                var extension = Path.GetExtension(highest);

                if (extension != null && extension != string.Empty)
                {
                    int index = 0;

                    if (int.TryParse(extension, out index))
                    {
                        return index;
                    }
                    else
                    {
                        // not an integer. Not sure what's up here. 
                        throw new Exception($"Unable to get highest file backup index. int.TryParse failed with file extension {extension}");
                    }
                }
            }
            else
            {
                //System.Diagnostics.Debug.WriteLine("Highest is null or empty");
            }

            return 0;
        }

        public static void MoveFileToBackup(string fileName, FileType fileType)
        {
            // file already exists, so figure out index of next file and then rename it to that
            string? directory = Path.GetDirectoryName(fileName);

            // this is the file name without the trailing numbers
            string rootFileName = Path.GetFileNameWithoutExtension(Path.GetFileName(fileName));

            if (directory != null)
            {
                int highestIndex = GetHighestFileBackupIndex(rootFileName, fileType);

                // try to rename existing file. In case two processes are doing
                // this at the same time, try a few times

                int nextIndex = highestIndex + 1;
                const int numTries = 10;

                for (int i = nextIndex; i < nextIndex + numTries; i++)
                {
                    string backupFileName = BuildBackupFileName(i, rootFileName, fileType);

                    System.Diagnostics.Debug.WriteLine($"Attempting to rename file to {backupFileName}");

                    try
                    {
                        File.Move(fileName, backupFileName);

                        // no error? Get out of the loop
                        break;
                    }
                    catch (IOException)
                    {
                        // File already exists. This may be because another process
                        // is doing the backup. Not a great scenario at all, but we don't
                        // want to completely blow up in that case and we do want to
                        // retain a backup. Really, in this case, we should stop everything
                        // and reload from an updated config, but that's not happening.
                        //
                        // Pause for a moment and then keep trying

                        Thread.Sleep(50);

                        continue;
                    }
                }
            }

        }


    }
}
