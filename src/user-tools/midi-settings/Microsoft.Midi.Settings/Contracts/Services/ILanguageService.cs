// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Globalization;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface ILanguageService
{
    CultureInfo CurrentCulture { get; }
    string CurrentLanguage { get; }
    Task InitializeAsync();
    Task SetLanguageAsync(string languageCode);
    List<CultureInfo> GetSupportedLanguages();
    void ApplyLanguage(string languageCode);
}
