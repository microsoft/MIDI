// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Service;
using Microsoft.Windows.Midi.Internal.Service.Services;

using Microsoft.Extensions.Logging.EventLog;
using Microsoft.Extensions.Logging.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Windows.Midi.Internal.Service.Model;
using Microsoft.Windows.Midi.Internal.Config;


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

        services.AddSingleton<MidiDeviceGraph>();
        services.AddSingleton<SessionGraph>();


        services.AddHostedService<PingService>();
        services.AddHostedService<ConnectionService>();
        services.AddHostedService<ManagementService>();

    })
    .Build();

host.Run();
