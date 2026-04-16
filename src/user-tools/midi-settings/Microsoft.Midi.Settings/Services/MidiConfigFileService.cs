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
using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
using Windows.ApplicationModel.Contacts;
using Windows.Devices.PointOfService;
using Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback;    // we use the WinRT JSON libraries to be consistent with the service code

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

    private bool Save()
    {
        _loggingService.LogInfo("Enter");

        if (m_config == null) return false;
        if (m_fullFileName == string.Empty) return false;

        try
        {
            // todo: Would be nice to format the json when writing, instead of writing it in compact mode
            
            // CreateText opens and overwrites if already there
            using (var fs = File.CreateText(m_fullFileName))
            {
                fs.Write(m_config.Stringify());
                fs.Close();
            }

            return true;
        }
        catch (Exception ex)
        {
            _loggingService.LogError("Error saving config file", ex);

        }

        return false;
    }

    private JsonObject? FindExistingTransportSection(JsonObject mainConfigObject, Guid transportId)
    {
        _loggingService.LogInfo($"Enter. Looking for transport '{transportId}'");

        string transportIdKey = transportId.ToString("B").ToUpper();

        System.Diagnostics.Debug.WriteLine($"Looking for section with this transport: {transportIdKey}");

        if (mainConfigObject.Keys.Contains(MidiConfigConstants.JsonKeys.TransportPluginSettings))
        {
            var transportPluginsSettings = mainConfigObject.GetNamedObject(MidiConfigConstants.JsonKeys.TransportPluginSettings);

            if (transportPluginsSettings.Keys.Contains(transportIdKey))
            {
                return transportPluginsSettings.GetNamedObject(transportIdKey, null);
            }
        }

        _loggingService.LogInfo($"Unable to find transport section");

        return null;
    }



    private JsonObject FindOrCreateTransportSection(JsonObject mainConfigObject, Guid transportId)
    {
        _loggingService.LogInfo($"Enter. Looking for transport '{transportId.ToString()}'");

        string transportIdKey = transportId.ToString("B").ToUpper();

        if (!mainConfigObject.Keys.Contains(MidiConfigConstants.JsonKeys.TransportPluginSettings))
        {
            _loggingService.LogInfo($"Adding missing transport plugin settings section");

            // need to add transport plugin settings to the main config
            JsonObject section = new JsonObject();
            mainConfigObject.SetNamedValue(MidiConfigConstants.JsonKeys.TransportPluginSettings, section);
        }

        var transportPluginsSettings = mainConfigObject.GetNamedObject(MidiConfigConstants.JsonKeys.TransportPluginSettings);

        if (!transportPluginsSettings.Keys.Contains(transportIdKey))
        {
            _loggingService.LogInfo($"Adding transport key '{transportId}'");

            // need to add section for this transport
            JsonObject section = new JsonObject();
            transportPluginsSettings.SetNamedValue(transportIdKey, section);
        }

        var thisTransport = transportPluginsSettings.GetNamedObject(transportIdKey, null);

        return thisTransport;
    }



    private JsonArray? FindExistingUpdateArray(JsonObject parentTransportObject)
    {
        if (parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonUpdate))
        {
            return parentTransportObject.GetNamedArray(MidiConfigConstants.JsonKeys.CommonUpdate);
        }

        return null;
    }

    private JsonArray FindOrCreateTransportUpdateArray(JsonObject parentTransportObject)
    {
        if (FindExistingUpdateArray(parentTransportObject) == null)
        {
            // need to add create array

            JsonArray array = new JsonArray();
            parentTransportObject.SetNamedValue(MidiConfigConstants.JsonKeys.CommonUpdate, array);
        }
        
        var updateArray = FindExistingUpdateArray(parentTransportObject)!;


        return updateArray;
    }

    private JsonArray FindOrCreateTransportRemoveArray(JsonObject parentTransportObject)
    {
        if (!parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonRemove))
        {
            // need to add create array

            JsonArray array = new JsonArray();
            parentTransportObject.SetNamedValue(MidiConfigConstants.JsonKeys.CommonRemove, array);
        }

        var removeArray = parentTransportObject.GetNamedArray(MidiConfigConstants.JsonKeys.CommonRemove);


        return removeArray;
    }

    private JsonObject? FindExistingTransportCreateObject(Guid transportId, Guid associationId)
    {
        if (m_config == null) return null;

        var transportSection = FindExistingTransportSection(m_config, transportId);
        if (transportSection == null) return null;

        if (transportSection.Keys.Contains(MidiConfigConstants.JsonKeys.CommonCreate))
        {
            var createObject = transportSection.GetNamedObject(MidiConfigConstants.JsonKeys.CommonCreate);
            if (createObject == null) return null;

            if (createObject.Keys.Contains(associationId.ToString("B").ToUpper()))
            {
                var loopback = createObject.GetNamedObject(associationId.ToString("B").ToUpper());

                if (loopback == null) return null;

                return loopback;
            }
        }

        return null;
    }

    private JsonObject? FindExistingTransportCreateObject(JsonObject parentTransportObject)
    {
        if (parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonCreate))
        {
            return parentTransportObject.GetNamedObject(MidiConfigConstants.JsonKeys.CommonCreate);
        }

        return null;
    }

    private JsonArray? FindExistingTransportCreateArray(JsonObject parentTransportObject)
    {
        if (parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonCreate))
        {
            return parentTransportObject.GetNamedArray(MidiConfigConstants.JsonKeys.CommonCreate);
        }

        return null;
    }

    private JsonArray? FindExistingTransportUpdateArray(JsonObject parentTransportObject)
    {
        if (parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonUpdate))
        {
            return parentTransportObject.GetNamedArray(MidiConfigConstants.JsonKeys.CommonUpdate);
        }

        return null;
    }


    private JsonArray FindOrCreateTransportCreateArray(JsonObject parentTransportObject)
    {
        if (!parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonCreate))
        {
            // need to add create array

            JsonArray array = new JsonArray();
            parentTransportObject.SetNamedValue(MidiConfigConstants.JsonKeys.CommonCreate, array);
        }

        var createArray = parentTransportObject.GetNamedArray(MidiConfigConstants.JsonKeys.CommonCreate);


        return createArray;
    }

    // some transports use a create object, not a create array
    private JsonObject FindOrCreateTransportCreateObject(JsonObject parentTransportObject)
    {
        if (!parentTransportObject.Keys.Contains(MidiConfigConstants.JsonKeys.CommonCreate))
        {
            // need to add create object

            JsonObject obj = new JsonObject();
            parentTransportObject.SetNamedValue(MidiConfigConstants.JsonKeys.CommonCreate, obj);
        }

        var createObject = parentTransportObject.GetNamedObject(MidiConfigConstants.JsonKeys.CommonCreate);


        return createObject;
    }


    private bool MergeEndpointTransportSectionIntoJsonObject(JsonObject mainConfigObject, JsonObject objectToMergeIn)
    {
        try
        {
            _loggingService.LogInfo($"Enter");

            if (mainConfigObject == null) return false;
            if (objectToMergeIn == null) return false;

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
                _loggingService.LogInfo($"Incoming object not found");

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

            _loggingService.LogError($"Finished without merging object");

            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception merging data into config", ex);

            return false;
        }
    }


    // Methods to add common functions to the config, like a new endpoint name, or new loopbacks
    // expose strongly typed stuff here, and let this class take care of the details within.
    // Each discrete function should result in a commit to the file.



    public bool RemoveBasicLoopbackEndpoint(Guid associationId)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Association Id: {associationId}");

            if (m_config == null) return false;

            var transportSection = FindExistingTransportSection(m_config, Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback.MidiBasicLoopbackEndpointManager.TransportId);
            if (transportSection == null) return false;

            var createSection = FindExistingTransportCreateObject(transportSection);
            if (createSection == null) return false;

            string endpointKey = associationId.ToString("B").ToUpper();

            if (createSection.ContainsKey(endpointKey))
            {
                createSection.Remove(endpointKey);

                _loggingService.LogInfo($"Saving: {associationId}");

                return Save();
            }

            _loggingService.LogError($"Unable to find or save loopback {associationId}");

            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception removing basic loopback endpoint. Association Id: {associationId}", ex);

            return false;
        }

    }



    // this assumes the config has already been run against the service to create the pair. We're just storing them here.
    public bool StoreBasicLoopbackEndpoint(Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback.MidiBasicLoopbackEndpointCreationConfig creationConfig)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Association Id {creationConfig.AssociationId}");

            if (m_config == null) return false;
            if (creationConfig == null) return false;

            // get the latest from disk
            if (!Load())
            {
                _loggingService.LogError($"Unable to load config");
                return false;
            }

            if (MergeEndpointTransportSectionIntoJsonObject(m_config, creationConfig.GetConfigJson()))
            {
                // write the property
                _loggingService.LogInfo($"Saving. Association Id {creationConfig.AssociationId}");

                return Save();
            }

            _loggingService.LogError($"Failed to store basic loopback endpoint {creationConfig.AssociationId}");

            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception storing basic loopback endpoint. Association Id: {creationConfig.AssociationId}", ex);

            return false;
        }
    }


    public bool StoreBasicLoopbackMutedProperty(Guid associationId, bool isMuted)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Association Id: {associationId}, isMuted: {isMuted} ");

            if (m_config == null) return false;

            // get the latest from disk
            if (!Load())
            {
                _loggingService.LogError($"Unable to load config");
                return false;
            }

            // there's never an update object for loopbacks, only create objects.
            var createObject = FindExistingTransportCreateObject(MidiBasicLoopbackEndpointManager.TransportId, associationId);
            if (createObject == null) return false;

            var endpointObject = createObject.GetNamedObject(MidiConfigConstants.JsonKeys.Endpoint, null);
            if (endpointObject == null) return false;

            endpointObject.SetNamedValue(MidiConfigConstants.JsonKeys.Muted, JsonValue.CreateBooleanValue(isMuted));

            _loggingService.LogInfo($"Saving. Association Id: {associationId}, isMuted: {isMuted} ");

            return Save();

        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception storing muted property Association Id: {associationId}, isMuted: {isMuted}", ex);

            return false;
        }
    }

    public bool RemoveLoopbackEndpointPair(Guid associationId)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Association Id: {associationId}");

            if (m_config == null) return false;

            var transportSection = FindExistingTransportSection(m_config, Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointManager.TransportId);
            if (transportSection == null) return false;

            var createSection = FindExistingTransportCreateObject(transportSection);
            if (createSection == null) return false;

            string endpointKey = associationId.ToString("B").ToUpper();

            if (createSection.ContainsKey(endpointKey))
            {
                createSection.Remove(endpointKey);

                _loggingService.LogInfo($"Saving. Association Id: {associationId}");
                return Save();
            }

            _loggingService.LogError($"Unable to remove loopback endpoint pair");
            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception removing loopback pair Association Id: {associationId}", ex);

            return false;
        }
    }



    // this assumes the config has already been run against the service to create the pair. We're just storing them here.
    public bool StoreLoopbackEndpointPair(Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.MidiLoopbackEndpointCreationConfig creationConfig)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Association Id: {creationConfig.AssociationId}");

            if (m_config == null) return false;
            if (creationConfig == null) return false;

            // get the latest from disk
            if (!Load())
            {
                _loggingService.LogError($"Unable to load config");
                return false;
            }

            if (MergeEndpointTransportSectionIntoJsonObject(m_config, creationConfig.GetConfigJson()))
            {
                // write the property

                _loggingService.LogInfo($"Saving. Association Id: {{creationConfig.AssociationId}}\"");
                return Save();
            }

            _loggingService.LogError($"Unable to remove loopback endpoint pair");
            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception storing loopback pair Association Id: {creationConfig.AssociationId}", ex);

            return false;
        }
    }


    public bool StoreNetworkHost(Microsoft.Windows.Devices.Midi2.Endpoints.Network.MidiNetworkHostCreationConfig creationConfig)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Id: {creationConfig.Id}");

            if (m_config == null) return false;
            if (creationConfig == null) return false;

            // get the latest from disk
            if (!Load())
            {
                _loggingService.LogError($"Unable to load config");
                return false;
            }

            if (MergeEndpointTransportSectionIntoJsonObject(m_config, creationConfig.GetConfigJson()))
            {
                // write the property

                _loggingService.LogInfo($"Saving. Id: {creationConfig.Id}");
                return Save();
            }

            _loggingService.LogError($"Unable to store network host");
            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception storing network host. Id: {creationConfig.Id}", ex);

            return false;
        }
    }

    public bool RemoveNetworkHost(string hostEntryId)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Id: {hostEntryId}");

            if (m_config == null) return false;
            if (string.IsNullOrEmpty(hostEntryId)) return false;

            // get the latest from disk
            if (!Load())
            {
                _loggingService.LogError($"Unable to load config");
                return false;
            }

            var transportObject = FindExistingTransportSection(m_config, MidiNetworkTransportManager.TransportId);
            if (transportObject == null) return false;

            var createObject = FindExistingTransportCreateObject(transportObject);
            if (createObject == null) return false;

            var hostsObject = createObject.GetNamedObject(MidiConfigConstants.JsonKeys.NetworkHosts);
            if (hostsObject == null) return false;

            if (hostsObject.ContainsKey(hostEntryId))
            {
                hostsObject.Remove(hostEntryId);

                _loggingService.LogInfo($"Saving. Id: {hostEntryId}");
                return Save();
            }
            else
            {
                return false;
            }
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception removing network host. Id: {hostEntryId}", ex);

            return false;
        }

    }



    public bool StoreNetworkClient(Microsoft.Windows.Devices.Midi2.Endpoints.Network.MidiNetworkClientConnectConfig creationConfig)
    {
        try
        {
            _loggingService.LogInfo($"Enter. Id: '{creationConfig.Id}'");

            if (m_config == null) return false;
            if (creationConfig == null) return false;

            // get the latest from disk
            if (!Load())
            {
                _loggingService.LogError($"Unable to load config");
                return false;
            }

            if (MergeEndpointTransportSectionIntoJsonObject(m_config, creationConfig.GetConfigJson()))
            {
                // write the property

                _loggingService.LogInfo($"Saving. Id: '{creationConfig.Id}'");
                return Save();
            }

            _loggingService.LogError("Unable to store network client in config file.");

            return false;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception storing network client. Id: {creationConfig.Id}", ex);

            return false;
        }

    }


    public bool StoreEndpointCustomization(Microsoft.Windows.Devices.Midi2.ServiceConfig.MidiServiceEndpointCustomizationConfig updateConfig)
    {
        try
        {
            if (m_config == null) return false;
            if (updateConfig == null) return false;

            // get the latest from disk
            if (!Load())
            {
                return false;
            }

            var existingTransportSettings = FindOrCreateTransportSection(m_config, updateConfig.TransportId);
            if (existingTransportSettings == null) return false;

            var existingUpdateArray = FindOrCreateTransportUpdateArray(existingTransportSettings);
            if (existingUpdateArray == null) return false;

            // now, look to see if there are any matching objects in here already

            foreach (var existingUpdate in existingUpdateArray)
            {
                var existingUpdateObject = existingUpdate.GetObject();
                if (existingUpdateObject == null) continue;

                if (existingUpdateObject.ContainsKey(MidiConfigConstants.JsonKeys.Match))
                {
                    var existingMatchJson = existingUpdateObject.GetNamedObject(MidiConfigConstants.JsonKeys.Match);

                    var existingMatchObject = Microsoft.Windows.Devices.Midi2.ServiceConfig.MidiServiceConfigEndpointMatchCriteria.FromJson(existingMatchJson.Stringify());

                    if (updateConfig.MatchCriteria.Matches(existingMatchObject))
                    {
                        System.Diagnostics.Debug.WriteLine("We match");

                        // this object has the full structure including endpointTransportPluginSettings and the transportId
                        var newTransportSection = FindExistingTransportSection(updateConfig.GetConfigJson(), updateConfig.TransportId);

                        if (newTransportSection != null)
                        {
                            var newConfigUpdateArray = FindExistingUpdateArray(newTransportSection);

                            if (newConfigUpdateArray != null)
                            {
                                var newConfigObject = newConfigUpdateArray.First().GetObject();

                                if (newConfigObject != null)
                                {
                                    System.Diagnostics.Debug.WriteLine("Adding new config object to array");

                                    // remove this update entry (match and custom props) in the config
                                    existingUpdateArray.Remove(existingUpdate);

                                    existingUpdateArray.Add(newConfigObject);

                                    return Save();
                                }
                            }
                        }

                        // matched but we fell through due to other issues. Stop processing
                        return false;
                    }
                }
            }

            // no matches found, so add as new

            var newTransportObject = FindExistingTransportSection(updateConfig.GetConfigJson(), updateConfig.TransportId);
            if (newTransportObject == null) return false;

            var newUpdateArray = FindExistingTransportUpdateArray(newTransportObject);
            if (newUpdateArray == null) return false;

            var obj = newUpdateArray.First();
            if (obj == null) return false;

            existingUpdateArray.Add(obj);

            return Save();
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
