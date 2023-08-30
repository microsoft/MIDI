using Spectre.Console.Cli;
using Spectre.Console;
using System.Runtime.Versioning;

using Microsoft.Devices.Midi2.ConsoleApp;
using Microsoft.Devices.Midi2.ConsoleApp.Resources;


var app = new CommandApp();

app.Configure(config =>
{
    config.SetApplicationName("midi");

    config.AddCommand<EnumEndpointsCommand>("enumerate-endpoints")
        .WithAlias("enum-endpoints")
        .WithAlias("list-endpoints")
        .WithDescription(Strings.CommandEnumerateEndpointsDescription)
        .WithExample("enum-endpoints", "--direction all")
        ;

    config.AddCommand<EnumLegacyEndpointsCommand>("enumerate-legacy-endpoints")
        .WithAlias("enum-legacy-endpoints")
        .WithAlias("list-legacy-endpoints")
        .WithDescription(Strings.CommandEnumerateLegacyEndpointsDescription)
        .WithExample("enum-legacy-endpoints", "--direction all")
        ;

    config.AddCommand<CheckHealthCommand>("check-health")
        .WithAlias("test")
        .WithDescription(Strings.CommandCheckHealthDescription)
        .WithExample("test", "--loopback")
        ;

    config.AddCommand<SendMessageCommand>("send-message")
        .WithAlias("send")
        .WithExample("send-message", "-word 0x405F3AB7 -word 0x12345789")
        .WithDescription(Strings.CommandSendMessageDescription)
        ;

    config.AddCommand<SendMessagesFileCommand>("send-message-file")
        .WithAlias("send-file")
        .WithExample("send-message-file", "%USERPROFILE%\\Documents\\messages.txt")
        .WithDescription(Strings.CommandSendMessagesFileDescription)
        ;

    config.AddCommand<MonitorEndpointCommand>("monitor-endpoint")
        .WithAlias("monitor")
        .WithAlias("listen")
        .WithExample("monitor-endpoint", "--instance-id \\\\?\\SWD#MIDISRV#MIDIU_DEFAULT_LOOPBACK_IN#{ae174174-6396-4dee-ac9e-1e9c6f403230}")
        .WithDescription(Strings.CommandMonitorEndpointDescription)
        ;

    config.AddCommand<DiagnosticsReportCommand>("diagnostics-report")
        .WithAlias("report")
        .WithDescription(Strings.CommandDiagnosticsReportDescription)
        .WithExample("diagnostics-report", "--output %USERPROFILE%\\Documents\\report.txt")
        ;
});

if (args.Length == 0)
{
    //AnsiConsole.Write(new FigletText("MIDI Console")
    //    .LeftJustified()
    //    .Color(Color.Purple));

}

AnsiConsole.WriteLine();
AnsiConsole.MarkupLine(Strings.AppTitle);
AnsiConsole.WriteLine();
AnsiConsole.MarkupLine(Strings.AppDescription);
AnsiConsole.WriteLine();

return app.Run(args);
