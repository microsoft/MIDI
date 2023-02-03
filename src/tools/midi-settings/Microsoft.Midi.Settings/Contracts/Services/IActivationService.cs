namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IActivationService
{
    Task ActivateAsync(object activationArgs);
}
