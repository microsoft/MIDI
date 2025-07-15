// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.Settings.Activation;

// Extend this class to implement new ActivationHandlers. See DefaultActivationHandler for an example.
// https://github.com/microsoft/TemplateStudio/blob/main/docs/WinUI/activation.md
public abstract class ActivationHandler<T> : IActivationHandler
    where T : class
{
    // Override this method to add the logic for whether to handle the activation.
    protected virtual bool CanHandleInternal(T args) => true;

    // Override this method to add the logic for your activation handler.
    protected abstract Task HandleInternalAsync(T args);

    public bool CanHandle(object args) => args is T && CanHandleInternal((args as T)!);

    public async Task HandleAsync(object args) => await HandleInternalAsync((args as T)!);
}
