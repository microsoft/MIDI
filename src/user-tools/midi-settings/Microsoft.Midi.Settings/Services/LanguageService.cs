// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using System.Globalization;
using Microsoft.UI.Xaml;

namespace Microsoft.Midi.Settings.Services;

public class LanguageService : ILanguageService
{
    private const string SettingsKey = "AppLanguage";
    private readonly ILocalSettingsService _localSettingsService;

    public CultureInfo CurrentCulture { get; private set; } = CultureInfo.CurrentUICulture;
    public string CurrentLanguage => CurrentCulture.Name;

    public LanguageService(ILocalSettingsService localSettingsService)
    {
        _localSettingsService = localSettingsService;
    }

    public async Task InitializeAsync()
    {
        var savedLanguage = _localSettingsService.ReadSetting<string>(SettingsKey);

        if (!string.IsNullOrEmpty(savedLanguage))
        {
            ApplyLanguage(savedLanguage);
        }
        else
        {
            var systemLanguage = CultureInfo.CurrentUICulture.Name;
            if (systemLanguage.StartsWith("zh", StringComparison.OrdinalIgnoreCase))
            {
                ApplyLanguage("zh-CN");
            }
            else
            {
                ApplyLanguage("en-US");
            }
        }

        await Task.CompletedTask;
    }

    public async Task SetLanguageAsync(string languageCode)
    {
        if (string.IsNullOrEmpty(languageCode))
        {
            return;
        }

        ApplyLanguage(languageCode);
        _localSettingsService.SaveSetting(SettingsKey, languageCode);

        await Task.CompletedTask;
    }

    public List<CultureInfo> GetSupportedLanguages()
    {
        return new List<CultureInfo>
        {
            new CultureInfo("en-US"),
            new CultureInfo("zh-CN")
        };
    }

    public void ApplyLanguage(string languageCode)
    {
        if (string.IsNullOrEmpty(languageCode))
        {
            return;
        }

        try
        {
            var culture = new CultureInfo(languageCode);
            CurrentCulture = culture;

            CultureInfo.CurrentCulture = culture;
            CultureInfo.CurrentUICulture = culture;
            CultureInfo.DefaultThreadCurrentCulture = culture;
            CultureInfo.DefaultThreadCurrentUICulture = culture;

            if (App.MainWindow?.Content is FrameworkElement rootElement)
            {
                rootElement.Language = culture.IetfLanguageTag;
            }
        }
        catch (CultureNotFoundException)
        {
            CurrentCulture = CultureInfo.GetCultureInfo("en-US");
        }
    }
}
