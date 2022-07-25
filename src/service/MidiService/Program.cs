using MidiService;

using Microsoft.Extensions.Logging.EventLog;
using Microsoft.Extensions.Logging.Configuration;
using Microsoft.Extensions.DependencyInjection;
using MidiConfig;


// need to set the working directory to load the config files

//System.IO.Directory.SetCurrentDirectory(System.AppDomain.CurrentDomain.BaseDirectory);


// let's get going

IHost host = Host.CreateDefaultBuilder(args)
    .UseWindowsService(options =>
    {
        options.ServiceName = "MIDI Services";

    })
    .ConfigureServices(services =>
    {
        LoggerProviderOptions.RegisterProviderOptions<EventLogSettings, EventLogLoggerProvider>(services);

        services.AddLogging();

        services.AddSingleton<MidiServicesConfig>();

        services.AddSingleton<ServiceState>();

        services.AddSingleton<DeviceGraph>();
        services.AddSingleton<SessionGraph>();

        services.AddHostedService<ConnectionService>();
        services.AddHostedService<ManagementService>();

    })
    .Build();

host.Run();
