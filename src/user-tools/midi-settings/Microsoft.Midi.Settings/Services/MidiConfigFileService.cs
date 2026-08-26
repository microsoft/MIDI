// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;

using Windows.Data.Json;
using Microsoft.Midi.Settings.Config;
using Windows.ApplicationModel.Contacts;
using Windows.Devices.PointOfService;

namespace Microsoft.Midi.Settings.Services;

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

        public const string Match = "match";
        public const string CustomProperties = "customProperties";


        public const string Header = "header";
        public const string HeaderConfigName = "configName";
        public const string HeaderProduct = "product";
        public const string HeaderFileVersion = "fileVersion";

        public const string TransportPluginSettings = "endpointTransportPluginSettings";

        public const string NetworkClients = "clients";
        public const string NetworkHosts = "hosts";

        public const string Muted = "muted";
        public const string Endpoint = "endpoint";
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

    private readonly ILoggingService _loggingService;

    public MidiConfigFile(string localFileName, ILoggingService loggingService)
    {
        FileName = localFileName;
        _loggingService = loggingService;
    }


    internal bool LoadHeaderOnly()
    {
        _loggingService.LogInfo("Enter");

        if (m_fullFileName == string.Empty) return false;

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
                    _loggingService.LogError("Missing config file header");

                    return false;
                }

                var headerObject = obj[MidiConfigConstants.JsonKeys.Header].GetObject();

                if (!headerObject.Keys.Contains(MidiConfigConstants.JsonKeys.HeaderConfigName))
                {
                    _loggingService.LogError("Missing config file header config name");
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
        catch (Exception ex)
        {
            _loggingService.LogError("Error loading config file header", ex);

            return false;
        }
    }

    public bool Load()
    {
        _loggingService.LogInfo("Enter");

        if (m_fullFileName == string.Empty) return false;

        try
        {
            if (!File.Exists(m_fullFileName))
            {
                _loggingService.LogError($"Config file '{m_fullFileName}' does not exist");

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
                _loggingService.LogError($"Unable to parse contents of config file");

                return false;
            }

        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error loading config file", ex);

            return false;
        }
    }



    // Methods to add common functions to the config, like a new endpoint name, or new loopbacks
    // expose strongly typed stuff here, and let this class take care of the details within.
    // Each discrete function should result in a commit to the file.

    public bool StoreEndpointCustomization(MidiServiceEndpointCustomizationConfig updateConfig)
    {
        try
        {
            if (updateConfig == null) return false;

            // The SDK owns the file: it re-reads and merges under its own write lock, keeps a
            // daily backup, and verifies what it wrote. This app no longer edits the json itself.
            var response = MidiServiceTransportPluginConfigManager.SaveUpdate(updateConfig);

            if (!response.Success)
            {
                _loggingService.LogError(
                    $"Could not save endpoint customization. {response.ErrorMessage} Transport Id: {updateConfig.TransportId}");

                return false;
            }

            // the file on disk has changed underneath the parse this object is holding
            Load();

            return true;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception storing endpoint customization. Transport Id: {updateConfig.TransportId}", ex);

            return false;
        }

    }

}




class MidiConfigFileService : IMidiConfigFileService
{
    public event EventHandler ActiveConfigFileChanged;


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

    private readonly IMidiServiceRegistrySettingsService _registryService;
    private readonly ILoggingService _loggingService;
    public MidiConfigFileService(
        IMidiServiceRegistrySettingsService registryService,
        ILoggingService loggingService)
    {
        _registryService = registryService;
        _loggingService = loggingService;

        string localFileName = _registryService.GetCurrentConfigFileName();

        if (!string.IsNullOrEmpty(localFileName))
        {
            CurrentConfig = LoadConfigFile(localFileName);
        }

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
                var configFile = new MidiConfigFile(localFileName, _loggingService);

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
        _loggingService.LogInfo("Enter");

        // early Canary builds went out with a file of this name, and
        // it's protected in a way that we can't write to it without
        // some futzing around, so we just pretend it doesn't exist.
        if (localFileName != null &&
            localFileName != string.Empty &&
            localFileName.ToLower() != "default.midiconfig.json")
        {
            var config = new MidiConfigFile(localFileName, _loggingService);

            if (config.Load())
            {
                if (ActiveConfigFileChanged != null)
                {
                    ActiveConfigFileChanged(this, new EventArgs());
                }

                return config;
            }
        }

        return null;
    }


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
        _loggingService.LogInfo("Enter");

        try
        {
            // cleanup the name
            configName = CleanupConfigName(configName);

            // make sure the config doesn't already exist

            if (ConfigFileExists(configLocalFileName))
            {
                // TODO: Back up the old one and overwrite this one?
                _loggingService.LogError($"Config file '{configLocalFileName}' already exists");

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
                _loggingService.LogError($"Config file '{filePath}' already exists");
                return false;
            }

            using (var fs = File.CreateText(filePath))
            {
                fs.Write(outerJsonObject.Stringify());
                fs.Close();
            }

            var config = new MidiConfigFile(configLocalFileName, _loggingService);
            config.LoadHeaderOnly();

            m_currentConfigFile = config;

            if (ActiveConfigFileChanged != null)
            {
                ActiveConfigFileChanged(this, new EventArgs());
            }

            _loggingService.LogInfo($"Config file '{config.FileName}' created");

            return true;
        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error creating new config file", ex);

            return false;
        }
    }

}
