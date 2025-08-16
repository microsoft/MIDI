// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IPageService
{
    Type GetPageType(string key);

    IList<Type> GetAllSearchEnabledViewModels();
}
