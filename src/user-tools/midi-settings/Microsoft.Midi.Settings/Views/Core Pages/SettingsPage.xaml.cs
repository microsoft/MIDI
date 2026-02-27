// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Views;

// TODO: Set the URL for your privacy policy by updating SettingsPage_PrivacyTermsLink.NavigateUri in Resources.resw.
public sealed partial class SettingsPage : Page
{
    private ILoggingService _loggingService;

    public SettingsViewModel ViewModel
    {
        get;
    }

    private bool _isInitializing = false;

    public SettingsPage()
    {
        _loggingService = App.GetService<ILoggingService>();

        ViewModel = App.GetService<SettingsViewModel>();

        InitializeComponent();

        // 初始化语言选择器
        _isInitializing = true;
        LanguageComboBox.SelectedValue = ViewModel.SelectedLanguage;
        _isInitializing = false;

        AboutCard_Title.Text = ResourceExtensions.GetLocalized("AppDisplayName");
        AboutCard_Description.Text = ResourceExtensions.GetLocalized("AppDescription");

    }

    private void WindowsDeveloperSettingsHyperlinkButton_Click(object sender, UI.Xaml.RoutedEventArgs e)
    {
        var psi = new System.Diagnostics.ProcessStartInfo();

        psi.FileName = "ms-settings:developers";
        psi.UseShellExecute = true;

        System.Diagnostics.Process.Start(psi);
    }

    private async void LanguageComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        // 如果是初始化阶段，不触发重启
        if (_isInitializing)
        {
            return;
        }

        if (LanguageComboBox.SelectedValue is string selectedLanguage)
        {
            var languageService = App.GetService<ILanguageService>();
            await languageService.SetLanguageAsync(selectedLanguage);

            // 延迟一点显示对话框，确保 XamlRoot 已准备好
            await Task.Delay(100);

            if (this.XamlRoot == null)
            {
                // 如果 XamlRoot 为空，直接重启
                RestartApp();
                return;
            }

            // 使用本地化资源
            var title = ResourceExtensions.GetLocalized("LanguageChangeDialog_Title");
            var content = ResourceExtensions.GetLocalized("LanguageChangeDialog_Content");
            var primaryButton = ResourceExtensions.GetLocalized("LanguageChangeDialog_PrimaryButton");
            var secondaryButton = ResourceExtensions.GetLocalized("LanguageChangeDialog_SecondaryButton");

            var dialog = new ContentDialog
            {
                Title = title,
                Content = content,
                PrimaryButtonText = primaryButton,
                CloseButtonText = secondaryButton,
                XamlRoot = this.XamlRoot
            };

            try
            {
                var result = await dialog.ShowAsync();
                if (result == ContentDialogResult.Primary)
                {
                    RestartApp();
                }
            }
            catch (Exception ex)
            {
                // 如果显示对话框失败，直接重启
                _loggingService.LogError("Failed to show language change dialog", ex);
                RestartApp();
            }
        }
    }

    private void RestartApp()
    {
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = System.Reflection.Assembly.GetExecutingAssembly().Location,
                WorkingDirectory = Environment.CurrentDirectory
            });
            Environment.Exit(0);
        }
        catch (Exception ex)
        {
            _loggingService.LogError("Failed to restart app", ex);
        }
    }
}
