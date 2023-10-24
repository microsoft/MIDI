using Spectre.Console.Cli;
using Spectre.Console;
using System.Runtime.Versioning;

using Microsoft.Devices.Midi2.ConsoleApp;
using Microsoft.Devices.Midi2.ConsoleApp.Resources;
using Windows.Gaming.Input.ForceFeedback;
using Windows.Devices.Midi2;



Console.InputEncoding = System.Text.Encoding.UTF8;
Console.OutputEncoding = System.Text.Encoding.UTF8;

var app = new CommandApp();

app.Configure(config =>
{
    config.SetApplicationName("midi");
    config.ValidateExamples();
    config.CaseSensitivity(CaseSensitivity.None);


    config.AddBranch("enumerate", enumerate =>
    {
        enumerate.SetDescription(Strings.CommandEnumerateDescription);

        enumerate.AddCommand<EnumEndpointsCommand>("ump-endpoints")
            .WithAlias("ump")
            .WithAlias("endpoints")
            .WithDescription(Strings.CommandEnumerateEndpointsDescription)
            .WithExample("enumerate", "ump-endpoints")
            ;

        enumerate.AddCommand<EnumLegacyEndpointsCommand>("bytestream-endpoints")
            .WithAlias("legacy-endpoints")
            .WithAlias("legacy")
            .WithDescription(Strings.CommandEnumerateLegacyEndpointsDescription)
            .WithExample("enumerate", "bytestream-endpoints", "--direction", "all")
            ;


        // TODO: may want to change this to just "plugins" and offer a switch
        // for the type of plugins to show
        /*
        enumerate.AddCommand<EnumTransportsCommand>("transport-plugins")
            .WithAlias("transports")
            .WithDescription(Strings.CommandEnumerateTransportPluginsDescription)
            .WithExample("enumerate", "transport-plugins")
            ; */
    }).WithAlias("list")
    .WithAlias("enum");


    
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
    }).WithAlias("svc");



    config.AddBranch<EndpointCommandSettings>("endpoint", endpoint =>
    {
        endpoint.SetDescription(Strings.CommandEndpointDescription);

        endpoint.AddCommand<MonitorEndpointCommand>("monitor")
            .WithAlias("monitor")
            .WithAlias("listen")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV#MIDIU_DEFAULT_LOOPBACK_BIDI_A#{e7cce071-3c03-423f-88d3-f1045d02552b}", "monitor")
            .WithDescription(Strings.CommandMonitorEndpointDescription)
            ;

        endpoint.AddCommand<SendMessageCommand>("send-message")
            .WithAlias("send-ump")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV...}", "send-message", "0x405F3AB7", "0x12345789", "--count", "10", "--delay", "20")
            .WithDescription(Strings.CommandSendMessageDescription)
            ;

        endpoint.AddCommand<SendMessagesFileCommand>("send-message-file")
            .WithAlias("send-ump-file")
            .WithAlias("send-file")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV...}", "send-message-file", "%USERPROFILE%\\Documents\\messages.txt")
            .WithDescription(Strings.CommandSendMessagesFileDescription)
            ;

        endpoint.AddCommand<CaptureMessagesCommand>("capture")
            .WithAlias("record")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV...}", "capture", "%USERPROFILE%\\Documents\\capture.txt", "--echo", "--annotate")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV...}", "capture", "capture.csv", "--delimiter", "Comma")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV...}", "capture", "capture.txt", "--format", "HexWithPrefix", "--filter", "SysEx8")
            .WithDescription(Strings.CommandMonitorEndpointDescription)
            ;

        endpoint.AddCommand<EndpointPropertiesCommand>("properties")
            .WithAlias("props")
            .WithExample("endpoint", "\\\\?\\SWD#MIDISRV...}", "properties")
            .WithDescription(Strings.CommandEndpointPropertiesDescription)
            ;



    }).WithAlias("ep");


    /*
    config.AddBranch<CacheCommandSettings>("cache", cache =>
    {
        // todo: commands to work with the global cache and the endpoint cache
        // including ways to flush all cache items for a single endpoint, 
        // way to set the value of the data for an endpoint property
        // and then the same for global
        // cache endpoint <endpoint id> flush
        // cache endpoint <endpoint id> list|enumerate
        // cache endpoint <endpoint id> set <property> <value>
        // cache endpoint <endpoint id> set-from-file <property> <filename>
        // TBD: provide a way to do the same for *all* endpoints in the cache?
        // cache global flush
        // cache global list|enumerate
        // cache global set <property> <value>
        // cache global set-from-file <property> <filename>
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
AnsiConsole.WriteLine();

if (args.Length == 0)
{
    // show app description only when no arguments supplied

    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatAppDescription(Strings.AppDescription));
    AnsiConsole.WriteLine();
}

return app.Run(args);
