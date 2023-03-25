namespace Microsoft.Midi.SettingsApp.Contracts.Services;

public interface IActivationService
{
    Task ActivateAsync(object activationArgs);
}
