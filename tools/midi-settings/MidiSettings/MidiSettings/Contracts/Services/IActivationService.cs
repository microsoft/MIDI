using System.Threading.Tasks;

namespace MidiSettings.Contracts.Services;

public interface IActivationService
{
    Task ActivateAsync(object activationArgs);
}
