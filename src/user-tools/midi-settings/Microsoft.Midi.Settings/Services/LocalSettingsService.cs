// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Helpers;

using Windows.Storage;

namespace Microsoft.Midi.Settings.Services;

public class LocalSettingsService : ILocalSettingsService
{
    private readonly IFileService _fileService;
    //private readonly LocalSettingsOptions _options;

    private readonly string _programDataRoot;
    private readonly string _applicationDataFolder;
    private readonly string _settingsFile;

    //private IDictionary<string, object> _settings;

    private global::Windows.Data.Json.JsonObject _settings;

    private bool _isInitialized;

    public LocalSettingsService(IFileService fileService)
    {
        _programDataRoot = Environment.ExpandEnvironmentVariables("%ProgramData%");
        _applicationDataFolder = Path.Combine(_programDataRoot, @"Microsoft\MIDI\");
        _settingsFile = "SettingsApp.appconfig.json";

        _fileService = fileService;

        //_settings = new Dictionary<string, object>();

    }

    public void Initialize()
    {
        if (!_isInitialized)
        {
            _settings = _fileService.Read(_applicationDataFolder, _settingsFile) ?? global::Windows.Data.Json.JsonObject.Parse("{}");

            _isInitialized = true;
        }
    }

    public T? ReadSetting<T>(string key)
    {
        App.GetService<ILoggingService>().LogInfo("Enter");

        Initialize();

        if (RuntimeHelper.IsMSIX)
        {
            if (ApplicationData.Current.LocalSettings.Values.TryGetValue(key, out var obj))
            {
                return Json.ToObject<T>((string)obj);
            }
        }
        else
        {
            if (_settings != null && _settings.TryGetValue(key, out var obj))
            {
                //return await Json.ToObjectAsync<T>((string)obj);
                return Json.ToObject<T>(obj.Stringify());
            }
        }

        return default;
    }

    public T? ReadSetting<T>(string key, T defaultIfNotSet)
    {
        App.GetService<ILoggingService>().LogInfo("Enter");

        Initialize();

        if (RuntimeHelper.IsMSIX)
        {
            if (ApplicationData.Current.LocalSettings.Values.TryGetValue(key, out var obj))
            {
                return Json.ToObject<T>((string)obj);
            }
        }
        else
        {
            if (_settings != null && _settings.TryGetValue(key, out var obj))
            {
                //return await Json.ToObjectAsync<T>((string)obj);
                return Json.ToObject<T>(obj.Stringify());
            }
        }

        return defaultIfNotSet;
    }

    public void SaveSetting<T>(string key, T value)
    {
        App.GetService<ILoggingService>().LogInfo("Enter");

        Initialize();


        if (RuntimeHelper.IsMSIX)
        {
            ApplicationData.Current.LocalSettings.Values[key] = Json.Stringify(value);
        }
        else
        {
            _settings[key] = global::Windows.Data.Json.JsonValue.Parse(Json.Stringify(value));

            _fileService.Save(_applicationDataFolder, _settingsFile, _settings);
        }
    }
}
