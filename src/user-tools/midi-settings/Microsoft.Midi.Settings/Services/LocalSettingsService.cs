// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Extensions.Options;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Helpers;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Models;
using System.Text.Json;
using System.Text.Json.Nodes;
using Windows.ApplicationModel;
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

    private async Task InitializeAsync()
    {
        if (!_isInitialized)
        {
            _settings = await Task.Run(() => _fileService.Read(_applicationDataFolder, _settingsFile)) ?? global::Windows.Data.Json.JsonObject.Parse("{}");

            _isInitialized = true;
        }
    }

    public async Task<T?> ReadSettingAsync<T>(string key)
    {
        if (RuntimeHelper.IsMSIX)
        {
            if (ApplicationData.Current.LocalSettings.Values.TryGetValue(key, out var obj))
            {
                return await Json.ToObjectAsync<T>((string)obj);
            }
        }
        else
        {
            await InitializeAsync();

            if (_settings != null && _settings.TryGetValue(key, out var obj))
            {
                //return await Json.ToObjectAsync<T>((string)obj);
                return Json.ToObject<T>(obj.Stringify());
            }
        }

        return default;
    }

    public async Task SaveSettingAsync<T>(string key, T value)
    {
        if (RuntimeHelper.IsMSIX)
        {
            ApplicationData.Current.LocalSettings.Values[key] = await Json.StringifyAsync(value);
        }
        else
        {
            await InitializeAsync();

            _settings[key] = global::Windows.Data.Json.JsonValue.Parse(await Json.StringifyAsync(value));

            await Task.Run(() => _fileService.Save(_applicationDataFolder, _settingsFile, _settings));
        }
    }
}
