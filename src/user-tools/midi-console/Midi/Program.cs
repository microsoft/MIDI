using Spectre.Console.Cli;
using Microsoft.Devices.Midi2.ConsoleApp;

var app = new CommandApp();

app.Configure(config =>
{
    config.AddCommand<EnumDevicesCommand>("enum-devices")
        .WithAlias("enumerate-devices")
        .WithAlias("list-devices")
        .WithDescription("List MIDI devices")
        /*.WithExample("enum", "foo", "--sort", ...)*/
        ;
});



return app.Run(args);
