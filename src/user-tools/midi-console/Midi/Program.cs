// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Runtime.Versioning;

using Microsoft.Midi.ConsoleApp;

using sdkInit = Microsoft.Windows.Devices.Midi2.Initialization;




Console.InputEncoding = System.Text.Encoding.UTF8;
Console.OutputEncoding = System.Text.Encoding.UTF8;

AppDomain.CurrentDomain.UnhandledException += (s, e) =>
{
    LoggingService.Current.LogError(e.ToString()!);
};


var app = new CommandApp();

app.Configure(config =>
{
    config.SetApplicationName("midi");
    config.ValidateExamples();
    config.CaseSensitivity(CaseSensitivity.None);


    config.AddBranch("create", create =>
    {
        create.AddCommand<CreateLoopbackCommand>("loopback")
        ;
    });


    config.AddBranch("enumerate", enumerate =>
    {
        enumerate.SetDescription(Strings.CommandEnumerateDescription);

        enumerate.AddCommand<EnumEndpointsCommand>("midi-services-endpoints")
            .WithAlias("endpoints")
            .WithAlias("ump-endpoints")
            .WithAlias("ep")
            .WithDescription(Strings.CommandEnumerateEndpointsDescription)
            .WithExample("enumerate", "midi-services-endpoints", "--include-diagnostic-loopback")
            .WithExample("enumerate", "endpoints")
            .WithExample("list", "endpoints")
            .WithExample("enum", "ep")
            ;

        enumerate.AddCommand<EnumLegacyEndpointsCommand>("legacy-winrt-api-endpoints")
            .WithAlias("legacy-endpoints")
            .WithAlias("bytestream-endpoints")
            .WithAlias("legacy")
            .WithDescription(Strings.CommandEnumerateLegacyEndpointsDescription)
            .WithExample("enumerate", "legacy-winrt-api-endpoints", "--direction", "all")
            .WithExample("enumerate", "legacy", "--direction", "all")
            .WithExample("list", "legacy-endpoints")
            ;

        enumerate.AddCommand<EnumActiveSessionsCommand>("active-sessions")
            .WithAlias("sessions")
            .WithDescription(Strings.CommandEnumerateActiveSessionsDescription)
            .WithExample("enumerate", "active-sessions")
            .WithExample("enumerate", "sessions")
            ;

        enumerate.AddCommand<EnumTransportsCommand>("transport-plugins")
            .WithAlias("transports")
            .WithDescription(Strings.CommandEnumerateTransportPluginsDescription)
            .WithExample("enumerate", "transport-plugins")
            .WithExample("enumerate", "transports")
            ;

        enumerate.AddCommand<EnumEndpointPropertyKeysCommand>("endpoint-property-keys")
            .WithAlias("property-keys")
            .WithDescription(Strings.CommandEnumerateEndpointPropertyKeysDescription)
            .WithExample("enumerate", "endpoint-property-keys")
            .WithExample("enumerate", "property-keys")
            ;

        //enumerate.AddCommand<EnumMdnsAdvertisementsCommand>("mdns-advertisements")
        //    .WithAlias("mdns")
        //    .WithAlias("network-midi")
        //    .WithDescription(Strings.CommandEnumerateMdnsAdvertisementsDescription)
        //    .WithExample("enumerate", "mdns-advertisements")
        //    .WithExample("enumerate", "mdns")
        //    .WithExample("enumerate", "network-midi")
        //    ;


    }).WithAlias("list")
    .WithAlias("enum");



    config.AddBranch<EndpointCommandSettings>("endpoint", endpoint =>
    {
        //endpoint.AddExample("endpoint", "properties", "--verbose");
        endpoint.AddExample("endpoint", "properties");
        endpoint.AddExample("endpoint", "send-message", "0x21234567");

        endpoint.SetDescription(Strings.CommandEndpointDescription);

        endpoint.AddCommand<EndpointMonitorCommand>("monitor")
            .WithAlias("monitor")
            .WithAlias("listen")
            .WithExample("endpoint", "\\\\?\\swd#midisrv#midiu_diag_loopback_b#{e7cce071-3c03-423f-88d3-f1045d02552b}", "monitor")
            .WithExample("endpoint", "monitor", "--capture-to-file", "%USERPROFILE%\\Documents\\MyMidiCapture.midi2", "--annotate-capture", "--capture-field-delimiter", "Pipe")
            .WithExample("endpoint", "monitor", "--verbose")
            .WithDescription(Strings.CommandMonitorEndpointDescription)
            ;

        endpoint.AddCommand<EndpointSendMessageCommand>("send-message")
            .WithAlias("send-ump")
            .WithAlias("send")
            .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "send-message", "0x405F3AB7", "0x12345789", "--count", "10", "--delay", "20")
            .WithExample("endpoint", "send-message", "0x21234567", "--count", "10", "--offset-microseconds", "2000000")
            .WithDescription(Strings.CommandSendMessageDescription)
            ;

        endpoint.AddCommand<EndpointSendMessagesFileCommand>("send-message-file")
            .WithAlias("send-ump-file")
            .WithAlias("send-file")
            .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "send-message-file", "%USERPROFILE%\\Documents\\messages.midi2")
            .WithDescription(Strings.CommandSendMessagesFileDescription)
            ;

        endpoint.AddCommand<EndpointSendSysExFileCommand>("send-sysex-file")
            .WithAlias("send-sysex")
            .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "send-sysex-file", "%USERPROFILE%\\Documents\\patch_dump.syx")
            .WithExample("endpoint", "send-sysex", "patch_dump.syx")
            .WithDescription(Strings.CommandSendSysExFileDescription)
            ;

        endpoint.AddCommand<EndpointPlayNotesCommand>("play-notes")
            .WithAlias("play")
            .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "play-notes", "127 32 56 98", "--length", "500", "--velocity", "50", "--forever", "--midi2")
            .WithDescription(Strings.CommandPlayNotesDescription)
            ;


        endpoint.AddCommand<EndpointPropertiesCommand>("properties")
            .WithAlias("props")
            .WithAlias("information")
            .WithAlias("info")
            .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "properties")
            .WithDescription(Strings.CommandEndpointPropertiesDescription)
            ;

        endpoint.AddCommand<EndpointSendClockCommand>("send-beat-clock")
            .WithAlias("send-clock")
            .WithAlias("clock")
            .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "send-clock", "--tempo", "120", "--ppqn", "24", "--group", "8")
            .WithExample("endpoint", "send-clock", "--tempo", "120", "--group", "1", "--group", "2", "--group", "16", "--send-start", "--send-stop")
            .WithDescription(Strings.CommandSendClockDescription)
            ;





        endpoint.AddBranch<EndpointRequestCommandSettings>("request", request =>
        {
            request.SetDescription(Strings.CommandEndpointRequestDescription);

            request.AddCommand<EndpointRequestFunctionBlocksCommand>("function-blocks")
                .WithAlias("function-block")
                .WithAlias("fb")
                .WithAlias("function")
                .WithAlias("functions")
                .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "request", "function-blocks", "--all")
                .WithExample("endpoint", "request", "function-block", "--number", "2")
                .WithDescription(Strings.CommandEndpointRequestFunctionBlocksDescription)
                ;

            request.AddCommand<EndpointRequestEndpointMetadataCommand>("endpoint-info")
                .WithAlias("endpoint-metadata")
                .WithAlias("endpoint-data")
                .WithAlias("em")
                .WithAlias("metadata")
                .WithExample("endpoint", "\\\\?\\swd#midisrv...}", "request", "endpoint-info", "--all")
                .WithExample("endpoint", "request", "endpoint-info", "--device-identity", "--product-instance-id")
                .WithDescription(Strings.CommandEndpointRequestEndpointMetadataDescription)
                ;
        }).WithAlias("req");


    }).WithAlias("ep");
     
    // if any alias for "service" is added, it must be included in the startup code below
    config.AddBranch("service", service =>
    {
        service.SetDescription(Strings.CommandServiceDescription);

        service.AddCommand<ServiceStatusCommand>("status")
            .WithDescription(Strings.CommandServiceStatusDescription)
            .WithExample("service", "status")
            ;

        service.AddCommand<ServicePingCommand>("ping")
            .WithDescription(Strings.CommandServicePingDescription)
            .WithExample("service", "ping", "--count", "10", "--timeout", "5000", "--verbose")
            ;

        service.AddCommand<ServiceRestartCommand>("restart")
            .WithDescription(Strings.CommandServiceRestartDescription)
            .WithExample("service", "restart")
            ;

        service.AddCommand<ServiceStartCommand>("start")
            .WithDescription(Strings.CommandServiceStartDescription)
            .WithExample("service", "start")
            ;

        service.AddCommand<ServiceStopCommand>("stop")
            .WithDescription(Strings.CommandServiceStopDescription)
            .WithExample("service", "stop")
            ;

        service.AddCommand<ServiceSetAutoStart>("set-auto-start")
            .WithDescription(Strings.ServiceSetAutoDelayedStartDescription)
            .WithExample("service", "set-auto-start", "--restart")
            ;


    }).WithAlias("svc");


    config.AddCommand<TimeCommand>("time")
        .WithDescription(Strings.CommandTimeDescription)
        .WithExample("time")
        .WithAlias("clock")
        ;

    config.AddCommand<WatchEndpointsCommand>("watch-endpoints")
        .WithDescription(Strings.CommandWatchEndpointsDescription)
        .WithExample("watch-endpoints")
        ;


    //config.AddBranch("measure", measure =>
    //{
    //    measure.SetDescription(Strings.CommandMeasureDescription);


    //    measure.AddCommand<MeasureLoopbackJitterCommand>("loopback-jitter")
    //        .WithAlias("jitter")
    //        .WithDescription(Strings.CommandMeasureLoopbackPerformanceDescription)
    //        .WithExample("measure", "loopback-jitter")
    //        ;

    //});


    /*
    config.AddBranch<SimulateCommandSettings>("simulate", cache =>
    {
        // TODO: endpoint and function block responders, including static, randomly moving at some interval, etc.
        // include an interactive mode that has menus for changing the endpoint name etc.

    });
    */

    /*
    config.AddBranch<SimulateCommandSettings>("simulate", cache =>
    {
        // TODO: endpoint and function block responders, including static, randomly moving at some interval, etc.
        // include an interactive mode that has menus for changing the endpoint name etc.

    });
    */

    /*
    config.AddCommand<DiagnosticsReportCommand>("diagnostics-report")
        .WithAlias("report")
        .WithDescription(Strings.CommandDiagnosticsReportDescription)
        .WithExample("diagnostics-report", "%USERPROFILE%\\Documents\\report.txt")
        ; */
});

// app title
AnsiConsole.WriteLine();
AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatAppTitle(Strings.AppTitle));
AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatAppVersionInformation($"{Microsoft.Windows.Devices.Midi2.Common.MidiNuGetBuildInformation.Name} ({Microsoft.Windows.Devices.Midi2.Common.MidiNuGetBuildInformation.Source})"));
AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatAppVersionInformation(Microsoft.Windows.Devices.Midi2.Common.MidiNuGetBuildInformation.BuildFullVersion));
AnsiConsole.WriteLine();



sdkInit.MidiDesktopAppSdkInitializer? initializer = null;

try
{
    initializer = sdkInit.MidiDesktopAppSdkInitializer.Create();
}
catch (Exception)
{
    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorSdkInitializerInitializationFailedExceptionInCreate));
    return (int)MidiConsoleReturnCode.ErrorMidiServicesSdkNotInstalled;
}


if (initializer == null)
{
    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorSdkInitializerInitializationFailed));

    return (int)MidiConsoleReturnCode.ErrorMidiServicesSdkNotInstalled;
}

using (initializer)
{
    // initialize SDK runtime
    if (!initializer.InitializeSdkRuntime())
    {
        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorSdkInitializationFailed));

        return (int)MidiConsoleReturnCode.ErrorMidiServicesSdkNotInstalled;
    }


    // this is super brittle
    if (args.Length > 1 && (args[0].ToLower() == "service" || args[0].ToLower() == "svc"))
    {
        // skip service initialization because user is perfoming a service command
    }
    else
    {
        // is the service running? If not, show a message so the user knows what is happening
        using (var controller = MidiServiceHelper.GetServiceController())
        {
            if (!MidiServiceHelper.ServiceIsReallyRunning(controller))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(Strings.StartingMidiService));
            }
        }

        // start the service
        if (!initializer.EnsureServiceAvailable())
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorMidiServiceNotAvailable));

            return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
        }
    }

    if (args.Length == 0)
    {
        // show app description only when no arguments supplied

        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatAppDescription(Strings.AppDescription));
        AnsiConsole.WriteLine();

        AnsiConsole.MarkupLine("Installed MIDI SDK: " + AnsiMarkupFormatter.FormatSdkVersionInformation(initializer.GetInstalledSdkDescription(true, true, true)));
        AnsiConsole.WriteLine();
    }


    MidiClock.BeginLowLatencySystemTimerPeriod();

    var result = app.Run(args);

    MidiClock.EndLowLatencySystemTimerPeriod();

    return result;
}
