// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Core.Helpers;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls.Primitives;
using Newtonsoft.Json.Linq;

namespace Microsoft.Midi.Settings.Services
{
    class GeneralSettingsService : IGeneralSettingsService
    {
        private readonly ILocalSettingsService _localSettingsService;


        //private const string ShowDeveloperOptions_SettingsKey = "ShowDeveloperOptions";
        private const string MainWindowPositionAndSize_SettingsKey = "MainWindowPositionAndSize";
        //private bool _showDeveloperOptions;

        public event EventHandler? SettingsChanged;

        public GeneralSettingsService(ILocalSettingsService localSettingsService)
        {
            _localSettingsService = localSettingsService;
        }

        public async void SetMainWindowPositionAndSize(WindowRect value)
        {
            await _localSettingsService.SaveSettingAsync<WindowRect>(MainWindowPositionAndSize_SettingsKey, value);
        }

        public WindowRect? GetMainWindowPositionAndSize()
        {
            try
            {
                return _localSettingsService.ReadSettingAsync<WindowRect>(MainWindowPositionAndSize_SettingsKey).GetAwaiter().GetResult();

            }
            catch (Exception)
            {
            }

            // default
            return null;
        }

        //public bool ShowDeveloperOptions
        //{
        //    get => _showDeveloperOptions;
        //    set
        //    {
        //        if (value != _showDeveloperOptions)
        //        {
        //            if (value)
        //            {
        //                // only allow setting this to true if dev mode is enabled
        //                value = WindowsDeveloperModeHelper.IsDeveloperModeEnabled;
        //            }

        //            SaveShowDeveloperOptionsFromSettingsAsync(value).Wait();
        //            _showDeveloperOptions = value;

        //            SettingsChanged?.Invoke(this, EventArgs.Empty);
        //        }
        //    }
        //}

        public async Task InitializeAsync()
        {
         //   _showDeveloperOptions = await LoadShowDeveloperOptionsFromSettingsAsync();

            await Task.CompletedTask;
        }


        //private async Task<bool> LoadShowDeveloperOptionsFromSettingsAsync()
        //{
        //    WindowsDeveloperModeHelper.Refresh();

        //    if (WindowsDeveloperModeHelper.IsDeveloperModeEnabled)
        //    {
        //        var settingsValue = await _localSettingsService.ReadSettingAsync<string>(ShowDeveloperOptions_SettingsKey);
        //        bool result;

        //        if (settingsValue != null && bool.TryParse(settingsValue, out result))
        //        {
        //            return result;
        //        }
        //    }

        //    return false;
        //}

        //private async Task SaveShowDeveloperOptionsFromSettingsAsync(bool showDeveloperOptions)
        //{
        //    await _localSettingsService.SaveSettingAsync<bool>(ShowDeveloperOptions_SettingsKey, showDeveloperOptions);
        //}
    }
}
