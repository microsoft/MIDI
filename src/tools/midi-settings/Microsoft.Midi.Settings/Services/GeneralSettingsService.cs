using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls.Primitives;

namespace Microsoft.Midi.Settings.Services
{
    class GeneralSettingsService : IGeneralSettingsService
    {
        private readonly ILocalSettingsService _localSettingsService;


        private const string ShowDeveloperOptions_SettingsKey = "ShowDeveloperOptions";
        private bool _showDeveloperOptions;

        public event EventHandler SettingsChanged;

        public GeneralSettingsService(ILocalSettingsService localSettingsService)
        {
            _localSettingsService = localSettingsService;
        }


        public bool ShowDeveloperOptions
        {
            get => _showDeveloperOptions;
            set
            {
                if (value)
                {
                    // only allow setting this to true if dev mode is enabled
                    value = WindowsDeveloperModeHelper.IsDeveloperModeEnabled;
                }

                SaveShowDeveloperOptionsFromSettingsAsync(value);
                _showDeveloperOptions = value;

                SettingsChanged?.Invoke(this, EventArgs.Empty);
            }
        }

        public async Task InitializeAsync()
        {
            _showDeveloperOptions = await LoadShowDeveloperOptionsFromSettingsAsync();

            await Task.CompletedTask;
        }


        private async Task<bool> LoadShowDeveloperOptionsFromSettingsAsync()
        {
            WindowsDeveloperModeHelper.Refresh();

            if (WindowsDeveloperModeHelper.IsDeveloperModeEnabled)
            {
                var settingsValue = await _localSettingsService.ReadSettingAsync<string>(ShowDeveloperOptions_SettingsKey);
                bool result;

                if (settingsValue != null && bool.TryParse(settingsValue, out result))
                {
                    return result;
                }
            }

            return false;
        }

        private async Task SaveShowDeveloperOptionsFromSettingsAsync(bool showDeveloperOptions)
        {
            await _localSettingsService.SaveSettingAsync(ShowDeveloperOptions_SettingsKey, showDeveloperOptions.ToString());
        }
    }
}
