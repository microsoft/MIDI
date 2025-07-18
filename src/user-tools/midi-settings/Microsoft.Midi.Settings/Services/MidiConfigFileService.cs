// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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

        public const double CurrentFileVersion = 1.0;

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

        //internal class Reg
        //{
        //    public const string ConfigFileRegKey = @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services";
        //    public const string ConfigFileCurrentRegValue = @"CurrentConfig";

        //    public const string UseMmcssRegValue = @"UseMMCSS";
        //    public const string Midi2DiscoveryEnabled = @"Midi2DiscoveryEnabled";
        //    public const string Midi2DiscoveryTimeout = @"Midi2DiscoveryTimeoutMS";

        //    public const string DefaultToOldMidi1PortNaming = @"DefaultToOldMidi1PortNaming";
        //}

        public const string DefaultConfigurationName = "WindowsMidiServices";
        
        public const string ConfigFileExtension = ".midiconfig.json";

        public const string DefaultConfigurationFileName = DefaultConfigurationName + ConfigFileExtension;

        private const string RawConfigFileLocation = @"%allusersprofile%\Microsoft\MIDI";
        public readonly static string ConfigFileLocation;

        static MidiConfigConstants()
        {
            ConfigFileLocation = Environment.ExpandEnvironmentVariables(RawConfigFileLocation);
        }

    }




    public class MidiConfigFile : IMidiConfigFile
    {
        private JsonObject? m_config = null;

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


        public MidiConfigFile(string localFileName)
        {
            FileName = localFileName;
        }


        internal bool LoadHeaderOnly()
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
                    if (!obj.Keys.Contains(MidiConfigConstants.JsonKeys.Header))
                    {
                        return false;
                    }

                    var headerObject = obj[MidiConfigConstants.JsonKeys.Header].GetObject();

                    if (!headerObject.Keys.Contains(MidiConfigConstants.JsonKeys.HeaderConfigName))
                    {
                        return false;
                    }

                    var header = new MidiConfigFileHeader();

                    header.Comment = headerObject.GetNamedString(MidiConfigConstants.JsonKeys.CommonComment, string.Empty);
                    header.Name = headerObject.GetNamedString(MidiConfigConstants.JsonKeys.HeaderConfigName, string.Empty);
                    header.Product = headerObject.GetNamedString(MidiConfigConstants.JsonKeys.HeaderProduct, string.Empty);
                    header.FileVersion = headerObject.GetNamedNumber(MidiConfigConstants.JsonKeys.HeaderFileVersion, 0.0);

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
                if (!File.Exists(m_fullFileName))
                {
                    return false;
                }

                string contents;

                using (var fs = File.OpenText(m_fullFileName))
                {
                    contents = fs.ReadToEnd();
                }

                JsonObject obj;

                if (JsonObject.TryParse(contents, out obj))
                {
                    m_config = obj;

                    if (!m_config.Keys.Contains(MidiConfigConstants.JsonKeys.Header))
                    {
                        return false;
                    }

                    var headerObject = m_config[MidiConfigConstants.JsonKeys.Header].GetObject();

                    if (!headerObject.Keys.Contains(MidiConfigConstants.JsonKeys.HeaderConfigName))
                    {
                        return false;
                    }

                    var header = new MidiConfigFileHeader();

                    header.Comment = headerObject.GetNamedString(MidiConfigConstants.JsonKeys.CommonComment, string.Empty);
                    header.Name = headerObject.GetNamedString(MidiConfigConstants.JsonKeys.HeaderConfigName, string.Empty);
                    header.Product = headerObject.GetNamedString(MidiConfigConstants.JsonKeys.HeaderProduct, string.Empty);
                    header.FileVersion = headerObject.GetNamedNumber(MidiConfigConstants.JsonKeys.HeaderFileVersion, 0.0);

                    m_header = header;

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

        private bool Save()
        {
            try
            {
                if (m_config != null)
                {
                    // CreateText opens and overwrites if already there
                    using (var fs = File.CreateText(m_fullFileName))
                    {
                        fs.Write(m_config.Stringify());
                        fs.Close();
                    }

                    return true;
                }
            }
            catch (Exception)
            {
            }

            return false;
        }



        private bool MergeEndpointTransportSectionIntoJsonObject(JsonObject mainConfigObject, JsonObject objectToMergeIn)
        {
            // this assumes the object to merge in has a singular tree structure to pull in. We're
            // not doing any recursion here. The intent is to do something like add another endpoint
            // create section to the master document, without having to manually create everything.
            //
            // This works because the config json is always a full rooted object -- a valid config file
            // in itself, albeit with only one specific section.


            JsonObject currentMainConfigLevel = mainConfigObject;
            JsonObject currentIncomingLevel = objectToMergeIn;

            // find endpointTransportPluginSettings section in the object to merge in.

            bool foundIncoming = false;
            while (!foundIncoming && currentIncomingLevel.Keys.Count > 0)
            {
                if (currentIncomingLevel.Keys.Contains(MidiConfigConstants.JsonKeys.TransportPluginSettings))
                {
                    currentIncomingLevel = currentIncomingLevel[MidiConfigConstants.JsonKeys.TransportPluginSettings].GetObject();

                    foundIncoming = true;
                }
                else
                {
                    // need to move down a level
                }

            }

            if (!foundIncoming)
            {
                return false;
            }

            if (!mainConfigObject.Keys.Contains(MidiConfigConstants.JsonKeys.TransportPluginSettings))
            {
                var obj = new JsonObject();
                mainConfigObject.Add(MidiConfigConstants.JsonKeys.TransportPluginSettings, obj);
            }

            currentMainConfigLevel = mainConfigObject[MidiConfigConstants.JsonKeys.TransportPluginSettings].GetObject();


            bool done = false;
            while (!done)
            {
                foreach (var key in currentIncomingLevel.Keys)
                {
                    if (currentMainConfigLevel.Keys.Contains(key))
                    {
                        // found the key, move to the next level. We're assuming objects here
                        // which will throw an exception if it's not an object
                        currentMainConfigLevel = currentMainConfigLevel[key].GetObject();
                        currentIncomingLevel = currentIncomingLevel[key].GetObject();
                    }
                    else
                    {
                        // add it and we're done
                        currentMainConfigLevel.Add(key, currentIncomingLevel[key]);

                        return true;
                    }
                }
            }

            return false;
        }



        // Methods to add common functions to the config, like a new endpoint name, or new loopbacks
        // expose strongly typed stuff here, and let this class take care of the details within.
        // Each discrete function should result in a commit to the file.

        // this assumes the config has already been run against the service to create the pair. We're just storing them here.
        public bool StoreLoopbackEndpointPair(Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointCreationConfig creationConfig)
        {
            // get the latest from disk
            if (!Load())
            {
                return false;
            }

            JsonObject mergeObject;
            if (JsonObject.TryParse(creationConfig.GetConfigJson(), out mergeObject))
            {
                if (MergeEndpointTransportSectionIntoJsonObject(m_config, mergeObject))
                {
                    // write the property

                    return Save();
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
        public IMidiConfigFile? CurrentConfig
        {
            get
            {
                return m_currentConfigFile;
            }
            set
            {
                m_currentConfigFile = (MidiConfigFile?)value;
            }
        }

        public bool IsConfigFileActive
        {
            get { return CurrentConfig != null; }
        }


        public MidiConfigFileService()
        {
            m_currentConfigFileName = string.Empty;
            m_currentConfigName = string.Empty;

            //m_currentConfigFile = LoadCurrentConfigFileFromRegistry();
        }




        public string GetConfigFilesLocation()
        {
            return MidiConfigConstants.ConfigFileLocation;
        }

        public IList<IMidiConfigFile> GetAllConfigFiles()
        {
            var configList = new List<IMidiConfigFile>();

            var files = Directory.GetFiles(MidiConfigConstants.ConfigFileLocation, MidiConfigConstants.ConfigFileExtension);

            foreach (var file in files)
            {
                string localFileName = Path.GetFileName(file);

                // early Canary builds went out with a file of this name, and
                // it's protected in a way that we can't write to it without
                // some futzing around, so we just pretend it doesn't exist.
                if (localFileName.ToLower() != "default.midiconfig.json")
                {
                    var configFile = new MidiConfigFile(localFileName);

                    if (configFile.LoadHeaderOnly())
                    {
                        configList.Add(configFile);
                    }
                }

            }

            return configList;
        }

        private MidiConfigFile? LoadConfigFile(string localFileName)
        {
            // early Canary builds went out with a file of this name, and
            // it's protected in a way that we can't write to it without
            // some futzing around, so we just pretend it doesn't exist.
            if (localFileName != null &&
                localFileName != string.Empty &&
                localFileName.ToLower() != "default.midiconfig.json")
            {
                var config = new MidiConfigFile(localFileName);

                if (config.Load())
                {
                    return config;
                }
            }

            return null;
        }

        //public bool UpdateRegistryCurrentConfigFile(string configFileName)
        //{
        //    try
        //    {
        //        Registry.SetValue(MidiConfigConstants.Reg.ConfigFileRegKey, MidiConfigConstants.Reg.ConfigFileCurrentRegValue, configFileName, RegistryValueKind.String);

        //        var config = LoadCurrentConfigFileFromRegistry();

        //        // set the config as current
        //        m_currentConfigFile = config;

        //        return true;
        //    }
        //    catch (Exception)
        //    {

        //    }

        //    return false;
        //}

        public string GetDefaultConfigName()
        {
            return MidiConfigConstants.DefaultConfigurationName;
        }

        public string BuildConfigLocalFileNameFromConfigName(string configName)
        {
            var cleanedConfigurationName = CleanupConfigName(configName);

            cleanedConfigurationName = string.Join("", cleanedConfigurationName.Split(Path.GetInvalidFileNameChars()));
            cleanedConfigurationName = string.Join("", cleanedConfigurationName.Split(Path.GetInvalidPathChars()));

            return cleanedConfigurationName + MidiConfigConstants.ConfigFileExtension;
        }

        public string BuildConfigFullFileNameWithPathFromLocalFileName(string configLocalFileName)
        {
            return Path.Combine(MidiConfigConstants.ConfigFileLocation, configLocalFileName);
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

        public bool ConfigFileExists(string configLocalFileName)
        {
            var fullPath = BuildConfigFullFileNameWithPathFromLocalFileName(configLocalFileName);

            return Path.Exists(fullPath);
        }


        private JsonObject CreateConfigFileHeaderJson(string configName)
        {
            var o = new JsonObject();

            // TODO: some of these property values should be localized

            o.Add(MidiConfigConstants.JsonKeys.CommonComment, JsonValue.CreateStringValue("NOTE: All json keys are case-sensitive, including GUIDs."));
            o.Add(MidiConfigConstants.JsonKeys.HeaderConfigName, JsonValue.CreateStringValue(configName));
            o.Add(MidiConfigConstants.JsonKeys.HeaderProduct, JsonValue.CreateStringValue("Windows MIDI Services"));
            o.Add(MidiConfigConstants.JsonKeys.HeaderFileVersion, JsonValue.CreateNumberValue(1.0));

            return o;
        }

        public bool CreateNewConfigFile(string configName, string configLocalFileName)
        {
            try
            {
                // cleanup the name
                configName = CleanupConfigName(configName);

                // make sure the config doesn't already exist

                if (ConfigFileExists(configLocalFileName))
                {
                    // TODO: Back up the old one and overwrite this one?

                    return false;
                }

                var outerJsonObject = new JsonObject();

                // create the config header

                var header = CreateConfigFileHeaderJson(configName);
                outerJsonObject.Add(MidiConfigConstants.JsonKeys.Header, header);

                // create section for transports

                var transportsObject = new JsonObject();
                outerJsonObject.Add(MidiConfigConstants.JsonKeys.TransportPluginSettings, transportsObject);

                var filePath = BuildConfigFullFileNameWithPathFromLocalFileName(configLocalFileName);

                if (Path.Exists(filePath))
                {
                    return false;
                }

                using (var fs = File.CreateText(filePath))
                {
                    fs.Write(outerJsonObject.Stringify());
                    fs.Close();
                }

                var config = new MidiConfigFile(configLocalFileName);
                config.LoadHeaderOnly();

                m_currentConfigFile = config;

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

    }
}
