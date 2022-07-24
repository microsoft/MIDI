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

namespace MidiConfig
{
    public class MidiServicesConfig
    {
        public Header Header { get; set; } = new Header(); 


        public Locations Locations { get; set; } = new Locations();


        public List<Plugin> TrustedPlugins { get; set; } = new List<Plugin>();

        public void LoadDefaults()
        {
            // TODO: We need to get this schema version from someplace else
            Header.SchemaVersion = new Version(1, 0, 0);

            Locations.MainAppFolder = FileLocations.AppDataFolder;
            Locations.SetupsFolder = Path.Combine(FileLocations.AppDataFolder, FileLocations.SetupsSubFolder);
            Locations.PluginsFolder = Path.Combine(FileLocations.AppDataFolder, FileLocations.PluginsSubFolder);
            Locations.IconsFolder = Path.Combine(FileLocations.AppDataFolder, FileLocations.IconsSubFolder);

        }

        public void Load()
        {
            // Get file path

            // deserialize
        }


        public void Save()
        {
            CreateConfigFile();
        }

        private string BuildBackupConfigFileName(int index, string rootFileName)
        {
            string fileName = rootFileName + "." + string.Format(FileLocations.ConfigFileBackupExtension, index);

            System.Diagnostics.Debug.WriteLine($"BackupConfigFileName = {fileName}");

            return fileName;
        }

        private int GetHighestFileBackupIndex(string directory, string rootFileName)
        {
            var files = Directory.GetFiles(directory, rootFileName + "." + FileLocations.ConfigFileBackupExtensionSearch).OrderBy(path => Path.GetExtension(path));

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

        private void LoadConfigFile()
        {
            string fileName = FileLocations.ConfigFileName;

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

        private void CreateAppDataFolder(string configDirectory)
        {
            //var info = Directory.CreateDirectory(configDirectory);

            //DirectorySecurity security = info.GetAccessControl();

            //security.AddAccessRule(new FileSystemAccessRule())
            //info.SetAccessControl(security);

        }

        private void CreateConfigFile()
        {
            string fileName = FileLocations.ConfigFileName;

            System.Diagnostics.Debug.WriteLine($"MainConfigFileLocation = {fileName}");

            string? configDirectory = Path.GetDirectoryName(fileName);

            if (configDirectory != null && !Directory.Exists(configDirectory))
            {
                // create the directories

                CreateAppDataFolder(configDirectory);

            }
            else if (configDirectory == null)
            {
                // this is a problem. The directory is null

                throw new Exception("Configuration directory is null.");
            }


            if (File.Exists(fileName))
            {
                System.Diagnostics.Debug.WriteLine("MainConfigFile exists}");

                // file already exists, so figure out index of next file and then rename it to that
                string? directory = Path.GetDirectoryName(fileName);

                // this is the file name without the trailing numbers
                string rootFileName = Path.GetFileNameWithoutExtension(Path.GetFileName(fileName));

                if (directory != null)
                {
                    int highestIndex = GetHighestFileBackupIndex(directory, rootFileName);

                    // try to rename existing file. In case two processes are doing
                    // this at the same time, try a few times

                    int nextIndex = highestIndex + 1;
                    const int numTries = 10;

                    for (int i = nextIndex; i < nextIndex + numTries; i++)
                    {
                        string backupFileName = BuildBackupConfigFileName(i, Path.Combine(directory, rootFileName));

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
