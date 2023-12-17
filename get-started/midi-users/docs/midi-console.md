# Windows MIDI Services Console

If you have the midi console installed, you can invoke it from any command prompt using `midi`. We recommend using [Windows Terminal](https://aka.ms/terminal) for the best experience.

## General Information

### Commands vs Options

MIDI Console commands are words with no symbol prefix. For example `endpoint` or `send-message-file`. Options are prefixed with two dashes if you use the full word, or a single dash if you use the single-letter abbreviation. For example `--help` or `-h`. There is no statement completion built in to the console, but there are some supported abbreviations for commands. These are not yet fully documented but are present in the Program.cs in the console source code.

### "Ports" vs "Streams"

In MIDI 1.0, specifically USB MIDI 1.0, a connected device would have a single input and single output stream. Inside that stream are packets of data with virtual cable numbers. Those numbers (16 total at most) identify the "port" the data is going to. Operating systems would then translate those into input and output ports. Those cable numbers were hidden from users.

MIDI 2.0 does not have a concept of a port. Instead, you always work with the stream itself. The group number, which is in the MIDI message now, is the moral equivalent of that cable number.

So where you may have seen a device with 5 input and 5 output ports in the past, you will now see a **single bidirectional UMP Endpoint stream** with 5 input groups and 5 output groups. We know this can take some getting used to, but it enables us to use MIDI 1.0 devices as though they are MIDI 2.0 devices, and provide a unified API.

### Help

Add the option `--help` or its short version `-h` to any command to get information and examples for that command.

```
midi --help
midi service --help
midi enumerate --help
midi enumerate endpoints --help
```

The `--help` option will always provide the most up-to-date list of commands and options supported by the MIDI Services Console.

## Check the MIDI Service Health

The heart of Windows MIDI Services is the Windows Service which processes and routes messages, creates endpoints, and more. The MIDI Services Console app includes a few commands to check the status and health of the service.

### Check MIDI Service Status

If you want to verify that the MIDI Service is running, you can check its status using the Service Control Manager, or through the MIDI Console. 

```
midi service status
midi svc status
```

If you uset the `--verbose` or `-v` option, the console will display more information about the service.

```
midi service status
midi svc status
```

### Ping the Service

If you want to verify that the service is transmitting and receiving messages, you can use the ping command, much like you would 

```
midi service ping
midi svc ping
```

This command also supports the `--verbose` or `-v` option to display the full results of the ping. It also supports a `--count` or `-c` parameter for the number of messages you want to send. Finally, the call supports a `--timeout` or `-t` parameter to set the timeout in milliseconds before the ping is considered to have failed.

Here are examples of the command with various parameters.

```
midi service ping --verbose
midi service ping --verbose --count 20 --timeout 20000
```

## See the Current Timestamp and Frequency

If you want to see the MIDI clock we're using for timestamps and message scheduling, you can use the `time` command. It will display the current timestamp in ticks, and the number of ticks per second (the resolution)

```
midi time
midi clock
```

## Enumerate (List) MIDI Entities

A basic operation you may do with the tool is list the major entities (Endpoints and Plugins) in the system.

The enumerate command has the aliases `enum` and `list` which may be used instead of the full `enumerate` command.

### Enumerate MIDI UMP Endpoints

The `ump-endpoints` parameter has the alias `endpoints` and the alias `ump` so either may be used with the same results. These commands are all equivalent:

```
midi enumerate ump-endpoints
midi enumerate endpoints
midi enum endpoints
midi list endpoints
midi list ump
```

All of the above statements will return a list of all the user-focused UMP endpoints on the system.

> **Note:** There are loopback endpoints A and B that are always available and are built into the service. They are crosswired to each other so that any message sent to A is received on B, and vice versa. They cannot be removed or disabled. Because these are more for support, testing, and developer scenarios, they are not returned from enumeration calls by default. Instead, you would supply the `--include-loopback` option for the enumeration commands.

### Enumerate Classic Byte-stream (MIDI 1.0) Endpoints

This uses the old WinRT API. Its primary reason for existance is so you can see what's shown to older APIs vs what is shown for the new Windows MIDI Services API. As with the UMP endpoints, the commands have aliases, so the following are all equivalent

```
midi enumerate bytestream-endpoints
midi enumerate legacy-endpoints
midi enum legacy-endpoints
midi list legacy
```

### Enumerate Transport Plugins

TODO: This feature is actively in development.

### Enumerate Message Processing Plugins

TODO: This feature is actively in development.

## Watch UMP Endpoints for Changes

Enumerating endpoints gives you a snapshot of the list at a moment in time. Watching the endpoints will give you a constantly updating list, which reflects device add/remove as well as property updates. This is useful more for developers, or those who are using tools to modify endpoints and want to verify that the changes were reported.

The `watch-endpoints` command has the alias `watch`, so these are equivalent:

```
midi watch-endpoints
midi watch
```

Note that only UMP endpoints (or bytestream endpoints converted to UMP by the new USB driver and service) are watched for changes. The older MIDI API is not used here. When you want to stop watching the endpoints for changes, hit the `escape` key.

## Single-Endpoint Commands

There are a number of commands, including those for monitoring and sending messages, which operate on a single endpoint.

In most any command which takes an Endpoint Device Id as a parameter, that parameter is optional. If you leave it out, and the command operates on a single endpoint, you will be prompted with a menu of available endpoints to work with.

If you want to script the commands without requiring any user interaction, provide the endpoint device ID as the first parameter after the `endpoint` command. For example:

```
midi endpoint \\?\SWD#MIDISRV#MIDIU_DIAG_LOOPBACK_B#{e7cce071-3c03-423f-88d3-f1045d02552b} properties --verbose
```

### Get Detailed Endpoint Properties

In the Device Manager in Windows, you can only see a subset of properties for a device. The same goes with the `pnputil` utility. It can be useful to see all of the key properties of a MIDI Endpoint. Therefore, we've baked property reporting right into the MIDI Services Console.

```
midi endpoint properties
```



By default, only key properties are displayed. If you want to see the complete list of all properties for the endpoint device, its container, and its parent device, add the `--verbose` parameter.

```
midi endpoint properties --verbose
```

As with other endpoint commands, if you provide the endpoint device Id, it will be used. Otherwise, you will be prompted to select an endpoint.

### Monitor an Endpoint for Incoming Messages

By default, every UMP Endpoint in Windows MIDI Services is multi-client. That means that more than one application can open a connection to the endpoint and send and/or receive messages. This also makes it possible to monitor all the incoming messages on an endpoint, even when that endpoint is in use by another application.

When run in `verbose` mode, the monitor will display each message as it arrives. It also displays helpful information about the type of the message, the group and channel when appropriate, the timestap offset (from the previous message if it was received recently), and more. This requires a fairly wide console window to allow formatting each message to take up only a single line. In a narrow window the format will be a bit ugly. We recommend using the [Windows Terminal application](https://apps.microsoft.com/detail/9N0DX20HK701), which has support for zooming in and out using the mouse wheel, different fonts, and more.

When run without the `--verbose` option, the monitor displays a count of the number of messages received.

Default mode:

```
midi endpoint monitor
```

Verbose mode:

```
midi endpoint monitor --verbose
```

When you have completed monitoring an endpoint, hit the `escape` key to close the connection and the app.

### Send a Message from the Command Line

Sending a message to an endpoint is very helpful for testing, but can also be used in automation to, for example, change the current program, or set a MIDI CC value. It would be very easy for a person to build a batch file or PowerShell script which used midi.exe to synchronize different devices, or reset devices to a known state in preparation for a performance.

The message data beyond the message type (first 4 bits) is not pre-validated, so the data can be anything. However, the number of 32 bit words must match the message type per the MIDI 2.0 specification.

Send a single UMP32 message immediately

```
midi endpoint send-message 0x21234567
```

Send a single UMP64 message ten times

```
midi endpoint send-message 0x41234567 0xDEADBEEF --count 10
```

Schedule a single UMP64 message 2 seconds from now (2 million microseconds). Offsets are in microseconds to provide more precise control.

```
midi endpoint send-message 0x41234567 0xFEEDF00D --offset-microseconds 2000000
```

Send a single UMP64 message fifteen times, but with a delay of two seconds (2000 milliseconds) in between each message. Delays are in milliseconds because they are there primarily to prevent flooding with older devices.

```
midi endpoint send-message 0x41234567 0xDEADBEEF --count 15 --pause 2000
```

In general, we recommend sending messages in hexadecimal format (prefix `0x` followed by 8 hexadecimal digits)as it is easier to visually inspect the information being sent. The 1-4 MIDI words are in order from left to right, from 1 to 4. 

### Send a File full of Messages

If you want to send a file full of messages, for SysEx or testing, for example, the console has provision for this.

The file needs to have one message per line, with 1-4 32 bit words as appropriate. There are options for delimeter (auto, space, comma, pipe, tab), word format (binary, hex, or decimal) as well as an option to change the group index. The latter is especially important when you have a SysEx file saved from one group and you want to send it on another group. The file name can include system variables which require expansion.

```
midi endpoint send-message-file %userprofile%\Documents\SysExBank12.txt --new-group-index 5
```

There are a number of options for this command both for the format it is reading, but also for the delay between messages (for older devices) and more. To get an explanation for each, type:

```
midi endpoint send-message-file --help
```

Here is one of the test files we use. It demonstrates comments, multiple representations for numbers, different delimeters, and more.

```
# This is a test file for sending UMPs through Windows MIDI Services
# It uses auto for the field delimiter so we can have different 
# delimiters on each line. Numeric format for this file is always hex.

# The line above was empty. The next data line is a UMP32

0x22345678

# The messages aren't valid beyond their message type matching the number of words

0xF1345678 0x12345678 0x03263827 0x86753099
0xF2345678,0x12345678,0x86754321, 0x86753099
0xF3345678|0x12345678|       0x86754321|0x86753099

0x21345678
0x42345678 0x12341234
0x43345677 0x12341235
0x44345676 0x12341236
0x45345675 0x12341237
0x26989898

# The next two lines have different hex formatting

41345678h 12341234h
22989898h
F3345678h 12345678h 86754321h 86753099h


# The next lines have no hex formatting

41345678 12341234
22989898

# The next lines have inconsistent hex formatting

41345678 12341234
0xF2345678 12345678h 86754321 0x86753099




# bunch of empty lines above. And the file ends with a comment
```

### Receive Message and Save to a File

TODO: This capability is a work-in-progress and is not yet functional.

## Technical Information

The Windows MIDI Services Console app has been developed using C#, .NET 8, the MIT-licensed open source [Spectre.Console](https://spectreconsole.net/) library, and the Microsoft-developed open source [C#/WinRT](https://learn.microsoft.com/windows/apps/develop/platform/csharp-winrt/) toolkit.

The console uses the same Windows MIDI Services WinRT APIs available to other desktop applications. Its full source code is available [on our Github repo](https://aka.ms/midirepo). Pull-requests, feature requests, and bug reports welcome. The project is open source, but we request that instead of forking it to create your own version, you consider contributing to the project.
