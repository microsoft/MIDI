// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;


namespace Microsoft.Midi.Settings.Services;

public class SynchronizationContextService : ISynchronizationContextService
{
    private SynchronizationContext _uiContext;
    public SynchronizationContext GetUIContext()
    {
        return _uiContext;
    }

    public SynchronizationContextService(SynchronizationContext uiContext)
    {
        _uiContext = uiContext;

        System.Diagnostics.Debug.WriteLine($"SynchronizationContextService: UI Context captured.");
    }


}
