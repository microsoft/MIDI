using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Xaml.Media;
using Microsoft.VisualBasic;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Win32;

using Windows.Data.Json;
using Microsoft.Midi.Settings.Config;    // we use the WinRT JSON libraries to be consistent with the service code

namespace Microsoft.Midi.Settings.Services
{
    internal class MidiConfigConstants
    {
        // consider moving these to the SDK so it can use the same
        // C++ headers / #defines the service is using


        internal class JsonKeys
        {
            public const string CommonComment = "_comment";

            public const string CommonCreate = "create";
            public const string CommonUpdate = "update";
            public const string CommonRemove = "remove";

            public const string Header = "header";
            public const string HeaderConfigName = "configName";
            public const string HeaderProduct = "product";
            public const string HeaderFileVersion = "fileVersion";

            public const string TransportPluginSettings = "endpointTransportPluginSettings";

        }




        public const string DefaultConfigurationName = "WindowsMidiServices";
        public const string ConfigExtension = ".midiconfig.json";
        public const string RawConfigFileLocation = @"%allusersprofile%\Microsoft\MIDI";
        public const string ConfigFileRegKey = @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services";
        public const string ConfigFileCurrentRegValue = @"CurrentConfig";

        public readonly static string ConfigFileLocation;


        static MidiConfigConstants()
        {
            ConfigFileLocation = Environment.ExpandEnvironmentVariables(RawConfigFileLocation);
        }

    }


    public class MidiConfigFileHeader
    {
        public string Comment { get; set; }
        public string Name { get; set; }
        public string Product { get; set; }
        public double FileVersion { get; set; }
    }


    public class MidiConfigFile
    {
        private JsonObject? m_config;

        private bool m_isLoaded = false;
        private bool m_isHeaderLoaded = false;


        private string m_fullFileName;
        private string m_fileName;
        public string FileName
        {
            get { return m_fileName; }
            set
            {
                m_fileName = value.Trim();

                m_fullFileName = Path.Combine(MidiConfigConstants.ConfigFileLocation, m_fileName);
            }
        }

        public string Name { get; set; }

        private MidiConfigFileHeader? m_header = null;

        public MidiConfigFileHeader? Header
        {
            get
            {
                if (m_header == null)
                {
                    LoadHeaderOnly();
                }
                return m_header;
            }
        }

        public bool LoadHeaderOnly()
        {
            try
            {
                string contents;

                using (var fs = File.OpenText(m_fullFileName))
                {
                    contents = fs.ReadToEnd();
                }

                JsonObject obj;

                if (JsonObject.TryParse(contents, out obj))
                {
                    if (!obj.Keys.Contains(JsonKey_Header))
                    {
                        return false;
                    }

                    var headerObject = obj[JsonKey_Header].GetObject();

                    if (!headerObject.Keys.Contains(JsonKey_HeaderConfigName))
                    {
                        return false;
                    }

                    var header = new MidiConfigFileHeader();

                    header.Comment = headerObject.GetNamedString(JsonKey_CommonComment, string.Empty);
                    header.Name = headerObject.GetNamedString(JsonKey_HeaderConfigName, string.Empty);
                    header.Product = headerObject.GetNamedString(JsonKey_HeaderProduct, string.Empty);
                    header.FileVersion = headerObject.GetNamedNumber(JsonKey_HeaderFileVersion, 0.0);

                    return true;
                }
                else
                {
                    return false;
                }

            }
            catch (Exception)
            {
                return false;
            }
        }

        public bool Load()
        {
            try
            {
                string contents;

                using (var fs = File.OpenText(m_fullFileName))
                {
                    contents = fs.ReadToEnd();
                }

                JsonObject obj;

                if (JsonObject.TryParse(contents, out obj))
                {
                    if (!obj.Keys.Contains(JsonKey_Header))
                    {
                        return false;
                    }

                    var headerObject = obj[JsonKey_Header].GetObject();

                    if (!headerObject.Keys.Contains(JsonKey_HeaderConfigName))
                    {
                        return false;
                    }

                    var header = new MidiConfigFileHeader();

                    header.Comment = headerObject.GetNamedString(JsonKey_CommonComment, string.Empty);
                    header.Name = headerObject.GetNamedString(JsonKey_HeaderConfigName, string.Empty);
                    header.Product = headerObject.GetNamedString(JsonKey_HeaderProduct, string.Empty);
                    header.FileVersion = headerObject.GetNamedNumber(JsonKey_HeaderFileVersion, 0.0);

                    return true;
                }
                else
                {
                    return false;
                }

            }
            catch (Exception)
            {
                return false;
            }
        }

        public bool Save()
        {
            string filePath = GetFullConfigFilePath(GetCurrentConfigName());

            // CreateText opens and overwrites if already there
            using (var fs = File.CreateText(filePath))
            {
                fs.Write(rootObject.Stringify());
                fs.Close();
            }

            return true;
        }



        // Methods to add common functions to the config, like a new endpoint name, or new loopbacks
        // expose strongly typed stuff here, and let this class take care of the details within.
        // Each discrete function should result in a commit to the file.


        private bool MergeSectionIntoJsonObject(JsonObject parentObject, JsonObject objectToMergeIn)
        {
            // this assumes the object to merge in has a singular tree structure to pull in. We're
            // not doing any recursion here. The intent is to do something like add another endpoint
            // create section to the master document, without having to manually create everything.
            //
            // This works because the config json is always a full rooted object -- a valid config file
            // in itself, albeit with only one specific section.

            bool done = false;

            JsonObject currentParentLevel = parentObject;
            JsonObject currentIncomingLevel = objectToMergeIn;

            while (!done)
            {
                foreach (var key in currentIncomingLevel.Keys)
                {
                    if (currentParentLevel.Keys.Contains(key))
                    {
                        // found the key, move to the next level. We're assuming objects here
                        // which will throw an exception if it's not an object
                        currentParentLevel = currentParentLevel[key].GetObject();
                        currentIncomingLevel = currentIncomingLevel[key].GetObject();
                    }
                    else
                    {
                        // add it and we're done
                        currentParentLevel.Add(key, currentIncomingLevel);

                        return true;
                    }
                }
            }

            return false;
        }

        // this assumes the config has already been run against the service to create the pair. We're just storing them here.
        public bool StoreLoopbackEndpointPair(Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointCreationConfig creationConfig)
        {
            creationConfig.GetConfigJson();

            JsonObject configFileObject;

            if (!ReadCurrentConfigFile(out configFileObject))
            {
                return false;
            }

            JsonObject mergeObject;
            if (JsonObject.TryParse(creationConfig.GetConfigJson(), out mergeObject))
            {
                if (MergeSectionIntoJsonObject(configFileObject, mergeObject))
                {
                    // write the property

                    return SaveCurrentConfig(configFileObject);
                }
            }

            return false;

        }

    }




    class MidiConfigFileService : IMidiConfigFileService
    {

        private string m_currentConfigName;
        private string m_currentConfigFileName;


        private MidiConfigFile? m_currentConfigFile = null;
        public MidiConfigFile? CurrentConfig
        {
            get
            {
                return m_currentConfigFile;
            }
        }

        public MidiConfigFileService()
        {
            m_currentConfigFileName = string.Empty;
            m_currentConfigName = string.Empty;


           // LoadConfigFileInfoFromRegistry();
        }


        public IList<MidiConfigFile> GetAllConfigFiles()
        {
            var configList = new List<MidiConfigFile>();

            var files = Directory.GetFiles(MidiConfigConstants.ConfigFileLocation, MidiConfigConstants.ConfigExtension);

            foreach (var file in files)
            {
                var configFile = new MidiConfigFile();

                configFile.FileName = Path.GetFileName(file);
                if (configFile.LoadHeaderOnly())
                {
                    configList.Add(configFile);
                }

            }

            return configList;
        }

        private MidiConfigFile? LoadCurrentConfigFileFromRegistry()
        {
            // get reg key
            var currentFile = (string)Registry.GetValue(MidiConfigConstants.ConfigFileRegKey, MidiConfigConstants.ConfigFileCurrentRegValue, string.Empty);

            if (currentFile != null && currentFile != string.Empty)
            {
                var config = new MidiConfigFile();

                config.FileName = currentFile;

                if (config.Load())
                {
                    return config;
                }
            }

            return null;

        }

        public bool SetCurrentConfigInRegistry(string configFileName)
        {
            try
            {
                Registry.SetValue(MidiConfigConstants.ConfigFileRegKey, MidiConfigConstants.ConfigFileCurrentRegValue, configFileName, RegistryValueKind.String);

                return true;
            }
            catch (Exception)
            {

            }

            return false;
        }

        public string GetDefaultConfigName()
        {
            return MidiConfigConstants.DefaultConfigurationName;
        }


        public string BuildConfigFileName(string configName)
        {
            var cleanedConfigurationName = CleanupConfigName(configName);

            cleanedConfigurationName = string.Join("", cleanedConfigurationName.Split(Path.GetInvalidFileNameChars()));
            cleanedConfigurationName = string.Join("", cleanedConfigurationName.Split(Path.GetInvalidPathChars()));

            return cleanedConfigurationName + MidiConfigConstants.ConfigExtension;
        }

        public string CleanupConfigName(string configName)
        {
            var cleanedConfigurationName = string.Join("", configName.Split(['.', ':', ';', '`', '%', '@', '#', '&', '$', '+', ',', '"', '\'', '{', '}', '[', ']'])).Trim();

            if (string.IsNullOrEmpty(cleanedConfigurationName))
            {
                return MidiConfigConstants.DefaultConfigurationName;
            }

            return cleanedConfigurationName;
        }

        public bool ConfigFileExists(string configFileName)
        {
            var fullPath = Path.Combine(MidiConfigConstants.ConfigFileLocation, configFileName);

            return Path.Exists(fullPath);
        }




        private JsonObject CreateConfigFileHeaderJson(string configName)
        {
            var o = new JsonObject();

            // TODO: some of these property values should be localized

            o.Add(MidiConfigConstants.JsonKeys.CommonComment, JsonValue.CreateStringValue("NOTE: All json keys are case-sensitive, including GUIDs. Don't change the config name in this file, as it must align with the file name."));
            o.Add(MidiConfigConstants.JsonKeys.HeaderConfigName, JsonValue.CreateStringValue(configName));
            o.Add(MidiConfigConstants.JsonKeys.HeaderProduct, JsonValue.CreateStringValue("Windows MIDI Services"));
            o.Add(MidiConfigConstants.JsonKeys.HeaderFileVersion, JsonValue.CreateNumberValue(1.0));

            return o;
        }

        public ConfigFile? CreateNewConfigFile(string configName)
        {
            try
            {
                // cleanup the name
                configName = CleanupConfigName(configName);

                // make sure the config doesn't already exist

                if (ConfigExists(configName))
                {
                    return false;
                }

                var outerJsonObject = new JsonObject();

                // create the config header

                var header = CreateConfigFileHeaderJson(configName);
                outerJsonObject.Add(JsonKey_Header, header);

                // create section for transports

                var transportsObject = new JsonObject();
                outerJsonObject.Add(JsonKey_TransportPluginSettings, transportsObject);

                var filePath = GetFullConfigFilePath(configName);

                if (Path.Exists(filePath))
                {
                    return false;
                }

                using (var fs = File.CreateText(filePath))
                {
                    fs.Write(outerJsonObject.Stringify());
                    fs.Close();
                }

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

    }
}
