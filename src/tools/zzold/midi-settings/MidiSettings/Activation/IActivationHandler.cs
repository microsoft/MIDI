using System.Threading.Tasks;

namespace MidiSettings.Activation;

public interface IActivationHandler
{
    bool CanHandle(object args);

    Task HandleAsync(object args);
}
