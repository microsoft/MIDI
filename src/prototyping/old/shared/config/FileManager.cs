// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma warning disable CA1416

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Metadata.Ecma335;
using System.Security;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Config
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
        private const string CommonProgramFilesSubfolder = "Microsoft Windows MIDI";

        public const string DefaultSetupsSubFolder = "Setups";
        public const string DefaultIconsSubFolder = "Icons";

        private const string SetupFileExtension = "midisetup.json";
        private const string ConfigFileExtension = "midiconfig.json";

        private const string SetupFileBackupExtensionFormat = "backup.{0:00000}";
        //private const string SetupFileBackupExtensionSearchPattern = "backup.?????";
        private const string SetupFileBackupExtensionSearchPattern = "backup.*";            // support wrapping over 100k

        private const string ConfigFileBackupExtensionFormat = "backup.{0:00000}";
        //private const string ConfigFileBackupExtensionSearchPattern = "backup.?????";
        private const string ConfigFileBackupExtensionSearchPattern = "backup.*";           // support wrapping over 100k

        private const string DefaultSetupFileNameWithoutPath = "Default." + SetupFileExtension;


        public const string DefaultPluginsSubFolder = "Plugins";
        public const string DefaultPluginsDeviceSubFolder = "Device";
        public const string DefaultPluginsProcessingSubFolder = "Processing";


        private const string ConfigFileNameWithoutPath = "MainConfig." + ConfigFileExtension;
        

        // Note that CommonApplicationData is not the same as %APPDATA%. It's usually
        // something like c:\ProgramData. Storing here because it is for all users on
        // the machine, not just a single user.
        public static string AppDataFolder
        {
            get
            {
                return Path.Combine(
                    Environment.GetFolderPath(
                        Environment.SpecialFolder.CommonApplicationData, 
                        Environment.SpecialFolderOption.DoNotVerify), 
                    AppDataSubfolder);
            }
        }

        public static string DefaultPluginsRootFolder
        {
            get
            {
                return Path.Combine(
                    Environment.GetFolderPath(
                        Environment.SpecialFolder.CommonProgramFiles,
                        Environment.SpecialFolderOption.DoNotVerify),
                    CommonProgramFilesSubfolder,
                    DefaultPluginsSubFolder);
            }
        }



        public static string DefaultConfigFolder => AppDataFolder;
        public static string DefaultSetupsFolder => Path.Combine(AppDataFolder, DefaultSetupsSubFolder);
        public static string DefaultIconsFolder => Path.Combine(AppDataFolder, DefaultIconsSubFolder);





        private static string? _setupsFolder = null;
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

        private static string? _configFileFolder = null;
        public static string ConfigFileFolder
        {
            get
            {
                if (_configFileFolder == null || _configFileFolder == string.Empty)
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

        private static string? _iconsFolder = null;
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
        public static string PluginsRootFolder
        {
            get
            {
                if (_pluginsFolder == null || _pluginsFolder == string.Empty)
                {
                    _pluginsFolder = DefaultPluginsRootFolder;
                }

                return _pluginsFolder;
            }
            set
            {
                _pluginsFolder = value;
            }
        }



        //public static void LoadDefaults()
        //{
        //    SetupsFolder = DefaultSetupsFolder;
        //    IconsFolder = DefaultIconsFolder;
        //    PluginsRootFolder = DefaultPluginsRootFolder;
        //}









        public static string ConfigFileFullNameWithPath
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
                //return Path.Combine(AppDataFolder, DefaultSetupFileNameWithoutPath);
                return DefaultSetupFileNameWithoutPath;
            }
        }




        private static string BuildBackupFileName(int index, string rootFileName, FileType fileType)
        {
            string fileName = rootFileName + "." + string.Format(GetBackupFormatForFileType(fileType), index);

         //   System.Diagnostics.Debug.WriteLine($"BackupConfigFileName = {fileName}");

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
            string searchPattern = rootFileName + "." + GetBackupSearchPatternForFileType(fileType);          
            
            try
            {
                var highest = Directory.GetFiles(directory, searchPattern)
                        .Max(path => int.Parse(Path.GetExtension(path).Substring(1)));

                return highest;

            }
            catch (Exception ex)
            {
                // problem with a bad file extension in the search results
                return 0;
            }
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

                //    System.Diagnostics.Debug.WriteLine($"Attempting to rename file to {backupFileName}");

                    try
                    {
                        File.Move(fileName, Path.Combine(directory, backupFileName));

                        // no error? Get out of the loop
                     //   System.Diagnostics.Debug.WriteLine($"Moved file.");
                        break;
                    }
                    catch (IOException ex)
                    {
                        // File already exists. This may be because another process
                        // is doing the backup. Not a great scenario at all, but we don't
                        // want to completely blow up in that case and we do want to
                        // retain a backup. Really, in this case, we should stop everything
                        // and reload from an updated config, but that's not happening.
                        //
                        // Pause for a moment and then keep trying

                  //      System.Diagnostics.Debug.WriteLine($"IO Exception {ex.ToString()}");

                        Thread.Sleep(50);

                        continue;
                    }
                }
            }

        }


    }
}
