---
layout: tools_page
title: MIDI Console
tool: console
description: Complete command and option reference for the Windows MIDI Services Console
---

The Windows MIDI Services Console is a command-line tool for enumerating, inspecting, monitoring, and testing MIDI endpoints. If you have it installed, you can invoke it from any command prompt using `midi`. We recommend using [Windows Terminal](https://aka.ms/terminal) for the best experience.

This single page documents every command and option. To get the same information at the command line, add `--help` to any command.

## Where to Get it

The Windows MIDI Services console is delivered as part of the SDK Runtime and Tools installer available via the links at the top of this page.

## Usage: Commands vs Options

MIDI Console commands are words with no symbol prefix, for example `endpoint` or `send-message-file`. Options are prefixed with two dashes for the full word, or a single dash for the single-letter abbreviation, for example `--help` or `-h`.

Most commands and many options have shorter aliases. All of them are listed in the tables on this page. Commands are not case-sensitive.

![MIDI endpoint properties]({{ site.baseurl }}/assets/images/console-midi-endpoint-properties.png)

## Getting Help

Add `--help` or `-h` to any command or branch to get its description, examples, arguments, and options.

```
midi --help
midi endpoint --help
midi endpoint monitor --help
midi enumerate endpoints --help
```

The `--help` option always reflects the exact version you have installed, so it is the definitive reference if this page and the tool ever disagree.

![MIDI Services Console Help]({{ site.baseurl }}/assets/images/console-help.png)

## "Ports" vs "Endpoints"

In MIDI 1.0, specifically USB MIDI 1.0, a connected device would have a single input and single output stream. Inside that stream are packets of data with virtual cable numbers. Those numbers (16 total at most in typical implementations) identify the "port" the data is going to. Operating systems would then translate those into input and output ports. Those cable numbers were hidden from users by the concept of a port.

MIDI 2.0 does not have the concept of a port. Instead, you always work with the stream itself. The group number, which is in the MIDI message now, is the moral equivalent of that cable number. Like the virtual cable, it helps address where the message is being sent.

So where you may have seen a device with 5 input and 5 output ports in the past show up as 10 discrete ports, you will now see a **single bidirectional UMP Endpoint stream** with 5 input groups and 5 output groups. We know this can take some getting used to, but it enables us to use MIDI 1.0 devices as though they are MIDI 2.0 devices, and provide a unified API.

## Specifying an Endpoint

Every command under `midi endpoint` takes an optional Endpoint Device Id, supplied immediately after the `endpoint` keyword and before the sub-command.

If you leave it out, you will be prompted with a menu of the available endpoints.

![midi endpoint prompt]({{ site.baseurl }}/assets/images/console-midi-endpoint-prompt.png)

If you want to script a command so that it requires no user interaction, provide the endpoint device Id explicitly.

```
midi endpoint \\?\SWD#MIDISRV#MIDIU_DIAG_LOOPBACK_B#{e7cce071-3c03-423f-88d3-f1045d02552b} properties --verbose
```

The general form is:

```
midi endpoint [Endpoint Device Id] <command> [OPTIONS]
```

## Command Summary

| Command | Aliases | Purpose |
| ----- | ----- | ----- |
| `enumerate` | `list`, `enum` | List endpoints, sessions, transports, and property keys |
| `endpoint` | `ep` | Monitor, send to, and inspect a single UMP endpoint |
| `endpoint request` | `req` | Send MIDI 2.0 stream request messages |
| `loopback` | `midi2-loopback`, `bidirectional-loopback` | Create and remove MIDI 2.0 loopback endpoint pairs |
| `basic-loopback` | `midi1-loopback`, `simple-loopback` | Create and remove single MIDI 1.0 loopback endpoints |
| `bridge` | `connect` | Bridge a Bluetooth LE MIDI 1.0 port to a UMP endpoint |
| `service` | `svc` | Check MIDI service status and ping the service |
| `watch-endpoints` | `watch-ump` | Watch UMP endpoints for add/remove/property changes |
| `watch-ports` | `watch-legacy` | Watch MIDI 1.0 ports for add/remove/property changes |
| `time` | `clock` | Show the current MIDI clock timestamp and resolution |

---

# Enumerate

Lists what is present on the system. The `enumerate` command has the aliases `enum` and `list`, so `midi enumerate endpoints`, `midi enum endpoints`, and `midi list endpoints` are all equivalent.

## enumerate midi-services-endpoints

*Aliases: `endpoints`, `ump-endpoints`, `ep`*

List MIDI UMP endpoints visible to Windows MIDI Services-aware applications.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--show-endpoint-id` | `-i` | | Include the UMP Endpoint Id in the output |
| `--include-diagnostic-loopback` | `-l` | | Include the diagnostic loopback endpoints |
| `--all` | `-a` | | List all recognized endpoints, including internal ones and ones an application may be unable to connect to directly |
| `--verbose` | `-v` | | Include more details for each endpoint |

```
midi enumerate midi-services-endpoints --include-diagnostic-loopback
midi enumerate endpoints
midi list endpoints
midi enum ep
```

> **Note:** There are diagnostic loopback endpoints A and B that are always available and are built into the service. They are crosswired to each other so that any message sent to A is received on B, and vice versa. They cannot be removed or disabled. Because these are more for support, testing, and developer scenarios, they are not returned from enumeration calls by default. Supply `--include-diagnostic-loopback` to see them.

![MIDI enum endpoints]({{ site.baseurl }}/assets/images/console-midi-enum-endpoints.png)

## enumerate legacy-winrt-api-endpoints

*Aliases: `legacy-endpoints`, `bytestream-endpoints`, `legacy`, `winrt1`*

List MIDI 1.0 endpoints as seen in apps using older MIDI APIs. Its primary reason for existing is so you can compare what is shown to older APIs against what is shown for the Windows MIDI Services API.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--direction` | `-d` | `All` | The direction of data flow for the endpoints to list |
| `--include-endpoint-id` | `-i` | `True` | Include the MIDI 1.0 Endpoint Id (the Port Id) in the output |

```
midi enumerate legacy-winrt-api-endpoints --direction all
midi enumerate legacy --direction all
midi list legacy-endpoints
```

## enumerate active-sessions

*Alias: `sessions`*

List all currently active Windows MIDI Services sessions on this PC.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--all` | `-a` | | Include all sessions, rather than only those relevant to the current user |

```
midi enumerate active-sessions
midi enumerate sessions
```

## enumerate transport-plugins

*Alias: `transports`*

List all MIDI transport plugins installed on this PC. This makes it easy to see which transports are currently enabled in Windows MIDI Services.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--verbose` | `-v` | | Include additional detail for each transport |

```
midi enumerate transport-plugins
midi enumerate transports
```

![Enumerate Transport Plugins]({{ site.baseurl }}/assets/images/console-enum-transports.png)

## enumerate endpoint-property-keys

*Alias: `property-keys`*

List the Windows MIDI Services-specific property keys. This command has no options beyond `--help`.

```
midi enumerate endpoint-property-keys
midi enumerate property-keys
```

---

# Endpoint

Commands which operate on a single UMP endpoint. The `endpoint` branch has the alias `ep`. All of these accept an optional Endpoint Device Id as described in [Specifying an Endpoint](#specifying-an-endpoint).

![midi endpoint help]({{ site.baseurl }}/assets/images/console-midi-endpoint-help.png)

## endpoint monitor

*Alias: `listen`*

Monitors a UMP endpoint for incoming messages and optionally displays them as they arrive.

By default, every UMP Endpoint in Windows MIDI Services is multi-client. That means more than one application can open a connection to the endpoint and send and/or receive messages. This also makes it possible to monitor all the incoming messages on an endpoint, even when that endpoint is in use by another application.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--single-message` | `-s` | | Wait for a single incoming message only |
| `--verbose` | `-v` | | Provide additional columns of information for each message |
| `--capture-to-file` | `-c` | | Capture messages to the specified file. If the file exists, it will be appended to |
| `--annotate-capture` | `-n` | | True to annotate messages written to the file. Annotations begin with the `#` sign and are written on the line before the UMP data line. The annotation includes timestamp information as well as the specific message type |
| `--capture-field-delimiter` | `-l` | `Space` | Delimiter to separate fields. Valid values include Space, Comma, Pipe, Tab |
| `--debug-warn-skipped-increment` | `-w` | | For incoming streams where the last word is incremented by one each time (a developer debugging approach) warn when there are gaps |
| `--auto-reconnect` | `-a` | `True` | Continue monitoring and automatically reconnect after device disconnection, if the device becomes available (unplug/replug) |
| `--jitter-statistics` | `-j` | | Display jitter statistics for the incoming stream |
| `--include-timestamp` | `-t` | | Include the timestamp in the displayed output |
| `--decode-messages` | `-d` | `True` | Show decoded information about the message |
| `--include-real-time-messages` | `-r` | | Include frequent/noisy messages like Clock pulse and Active Sense |

When run in verbose mode, the monitor displays each message as it arrives, along with the type of the message, the group and channel when appropriate, the timestamp offset, and more. This requires a fairly wide console window to allow each message to take up only a single line.

```
midi endpoint monitor
midi endpoint monitor --verbose
```

![midi endpoint monitor verbose]({{ site.baseurl }}/assets/images/console-midi-endpoint-monitor-verbose.png)

### Saving messages to a file

When monitoring, you can save the messages to a file. This can be used to capture test data which you will later send using `send-message-file`, or for storing something like a System Exclusive dump.

```
midi endpoint monitor --capture-to-file %USERPROFILE%\Documents\MyMidiCapture.midi2 --annotate-capture --capture-field-delimiter Pipe
```

If no file extension is specified, the extension `.midi2` is automatically added.

> The file you choose to write to will be appended to if it already exists. Use caution when specifying the file name, so that you don't corrupt an unrelated file with this MIDI data.

When you have finished monitoring an endpoint, hit the `escape` key to close the connection and the app.

## endpoint send-message

*Aliases: `send-ump`, `send`*

Send a single message to a UMP endpoint as a list of up to four 32-bit MIDI words.

**Argument:** `<MIDI Words>` — 32-bit MIDI words, typically in hexadecimal `0x00000000` format, same as the MIDI protocol wire format.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--pause` | `-p` | `2` | Pause (delay), in milliseconds, between messages. Specify 0 for no delay |
| `--word-format` | `-w` | `Hex` | Data format for the individual words. Valid values include Binary, Decimal, Hex |
| `--no-wait` | `-n` | | Do not prompt the user to hit any key to close the connection |
| `--count` | `-c` | `1` | Number of times to send this message |
| `--offset-microseconds` | `-o` | `0` | Timestamp offset in microseconds to use when scheduling this message. A new timestamp with this offset is calculated for each sent message as it is sent |
| `--timestamp` | `-t` | | Absolute timestamp value to use for all messages. Use 0 to bypass scheduling and send immediately |
| `--debug-auto-increment` | `-i` | | Auto-increment the last word you have specified by 1 for each message sent. Requires a message of two or more words so that the message type nibble isn't impacted |

The message data beyond the message type (first 4 bits) is not pre-validated, so the data can be anything. However, the number of 32-bit words must match the message type per the MIDI 2.0 specification.

```
midi endpoint send-message 0x21234567
midi endpoint send-message 0x41234567 0xDEADBEEF --count 10
midi endpoint send-message 0x41234567 0xDEADBEEF --count 15 --pause 2000
```

In general, we recommend sending messages in hexadecimal format as it is easier to visually inspect the information being sent. The 1-4 MIDI words are in order from left to right.

### Special debug messages

Sending otherwise valid UMP messages where the last word is incremented by 1 for each sent message helps validate that all messages were received by your application, and in the correct order. This requires a message type of at least two words. We don't recommend sending Type F stream messages as those have the potential to corrupt data; a Type 4 MIDI 2.0 channel voice message is usually safer.

```
midi endpoint send-message 0x41234567 0x00000000 --count 10000 --pause 2 --debug-auto-increment
```

You should see the second word update from `0x00000000` through `0x00002710` (decimal 10000). We recommend the pause when sending large numbers of messages because a pause of 0 can flood the buffers with more data than the client may be able to retrieve in time, and may result in dropped messages.

### Scheduling messages

`--offset-microseconds` adds a fixed time to each outgoing message so that it is scheduled that far into the future. Offsets are in microseconds to provide more precise control compared to milliseconds.

```
midi endpoint send-message 0x41234567 0xFEEDF00D --offset-microseconds 2000000
```

You can also specify an absolute timestamp. Typically this is used to specify a timestamp of 0, which bypasses any scheduling and sends immediately.

```
midi endpoint send-message 0x41234567 0xFEEDF00D --timestamp 0
```

You can use the `midi time` command to see the current timestamp, and then use that to pick a future timestamp. If you do not specify a timestamp, the current time is used.

## endpoint send-message-file

*Aliases: `send-ump-file`, `send-file`*

Sends a text file of UMP MIDI words to the specified endpoint.

**Argument:** `<Input File>` — Path and filename of the text file to send. Lines beginning with `#` are comments, and empty lines are allowed for spacing. Remaining lines must be valid delimited (big-endian) hexadecimal UMP words. The file name can include environment variables which require expansion.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--pause` | `-p` | `2` | Pause (delay), in milliseconds, between messages. Specify 0 for no delay |
| `--word-format` | `-w` | `Hex` | Data format for the individual words. Valid values include Binary, Decimal, Hex |
| `--no-wait` | `-n` | | Do not prompt the user to hit any key to close the connection |
| `--delimiter` | `-d` | `Auto` | Delimiter between fields. Set to Auto to have this evaluated for each line. Valid values include Auto, Space, Comma, Pipe, Tab |
| `--verbose` | `-v` | | Verbose output as the messages are sent |
| `--new-group-index` | `-g` | | For non-stream messages, replace the second nibble with the specified new group index. Helpful when sending previously-recorded SysEx to a new group |

```
midi endpoint send-message-file %USERPROFILE%\Documents\SysExBank12.txt --new-group-index 5
```

Here is one of the test files we use. It demonstrates comments, multiple representations for numbers, different delimiters, and more.

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
0x26989898

# The next two lines have different hex formatting

41345678h 12341234h
22989898h

# The next lines have no hex formatting

41345678 12341234
22989898

# And the file ends with a comment
```

## endpoint send-sysex-file

*Alias: `send-sysex`*

Send a file of MIDI 1.0 binary SysEx 7-bit messages to a compatible endpoint. These are first translated to UMP SysEx 7 for transmission.

**Argument:** the binary SysEx 7 file to send.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--group-index` | `-g` | | Set the group index with this new value |
| `--pause` | `-p` | `500` | Milliseconds to pause between transfers (groups) of messages |
| `--message-transfer-count` | `-m` | `64` | Number of outgoing UMP message packets (typically 6 data bytes each) between pauses |

```
midi endpoint send-sysex-file %USERPROFILE%\Documents\patch_dump.syx
midi endpoint send-sysex patch_dump.syx --group-index 2
```

## endpoint play-notes

*Alias: `play`*

Send MIDI 1.0 or 2.0 note on and off messages to the endpoint. This is not meant to be a sequencer with the timing accuracy you would have in a DAW, but is instead a simple way to play notes on an endpoint.

**Argument:** `<indexes>` — List of space-separated MIDI 1.0 note indexes (0-127 decimal).

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--length` | `-l` | `250` | Length / duration of the note in whole decimal milliseconds |
| `--rest` | `-r` | `250` | Duration of wait time between notes, in whole decimal milliseconds |
| `--group` | `-g` | `1` | Number (1-16 decimal) for the group to send the messages to |
| `--channel` | `-c` | `1` | Number (1-16 decimal) of the channel to send the messages to |
| `--velocity` | `-v` | `75` | Note velocity (1.0 - 100.0) as a fractional decimal percentage of the maximum value for the protocol |
| `--forever` | `-f` | | Continue to loop through the notes until you press the escape key |
| `--midi2` | `-m` | | Use MIDI 2.0 protocol messages (type 4) instead of MIDI 1.0 protocol (type 2) |
| `--auto-reconnect` | `-a` | `True` | Automatically reconnect after device disconnection, if the device becomes available |

```
midi endpoint play-notes 50 55 52 60 72 90 --group 1 --channel 10 --velocity 100 --length 250 --rest 500 --forever
```

![Play Notes]({{ site.baseurl }}/assets/images/console-midi-endpoint-play-notes.png)

## endpoint properties

*Aliases: `props`, `information`, `info`*

List out system-captured metadata properties for the specified endpoint.

In Device Manager, and with the `pnputil` utility, you can only see a subset of properties for a device. It can be useful to see all of the key properties of a MIDI Endpoint, so property reporting is baked right into the console.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--verbose` | `-v` | | Include additional detailed information about the endpoint |
| `--include-raw-properties` | `-r` | | Include the full list of raw property keys and values for the endpoint. Typically useful only for debugging purposes |
| `--include-name-table` | `-n` | | Include the list of possible MIDI 1.0 port names generated on endpoint creation or customization |

```
midi endpoint properties
midi endpoint properties --verbose
```

---

# Endpoint Request

Send MIDI 2.0 stream request messages without having to remember their exact format. These are primarily a convenience for developers and for debugging MIDI 2.0 hardware. The `request` branch has the alias `req`.

Before sending a request, you may want to open another console window with `midi watch-endpoints` active, so you can see when the stored properties change. You may also want a verbose `midi endpoint monitor` window open so you can see the response messages come back.

## endpoint request function-blocks

*Aliases: `function-block`, `fb`, `function`, `functions`*

Send a function block request message to the endpoint. The singular aliases are available to make the command read better when requesting a single block's data.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--all` | `-a` | | Request all function blocks for this endpoint |
| `--function-block-number` | `-n` | `0` | Request just the specified function block number |
| `--request-info` | `-i` | `True` | Request the general information for a function block |
| `--request-name` | `-f` | `True` | Request the name for a function block |

By default you request both the info notification and the name notification. To request only one of them, turn the other off. You must request at least one of the two.

```
midi endpoint request function-blocks --all
midi endpoint request function-blocks --function-block-number 3
midi endpoint request function-blocks --all --request-name false
midi endpoint request function-blocks --all --request-info false
```

## endpoint request endpoint-info

*Aliases: `endpoint-metadata`, `endpoint-data`, `em`, `metadata`*

Send an endpoint discovery message to the endpoint.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--all` | `-a` | | Request all endpoint information |
| `--endpoint-info` | `-i` | `True` | Request the endpoint information notification |
| `--device-identity` | `-d` | | Request a device identity notification |
| `--name` | `-n` | | Request an endpoint name notification. May result in multiple response messages |
| `--product-instance-id` | `-p` | | Request a product instance id notification. May result in multiple response messages |
| `--stream-configuration` | `-s` | | Request a stream configuration notification |
| `--ump-version-major` | `-j` | `1` | The UMP specification major version. The default is usually sufficient |
| `--ump-version-minor` | `-m` | `1` | The UMP specification minor version. The default is usually sufficient |

```
midi endpoint request endpoint-info --all
midi endpoint request endpoint-info --device-identity --product-instance-id
midi endpoint request metadata --name --endpoint-info false
```

---

# Loopback Endpoints

Loopback endpoints are useful for testing, and for connecting two applications together on the same PC. These endpoints are temporary: they exist until the console session which created them ends.

## loopback create

*Branch aliases: `midi2-loopback`, `bidirectional-loopback`*

Create a temporary set of MIDI 2.0 (and MIDI 1.0) loopback endpoints, connected to each other. Messages sent to the A loopback will be received by B, and messages sent to B will be received by A.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--name-a` | `-a` | | Unique name for the A-side of the loopback pair |
| `--name-b` | `-b` | | Unique name for the B-side of the loopback pair |
| `--root-name` | `-r` | | Provide a root name instead of individual endpoint names. "(A)" and "(B)" will be appended to this name to create the two endpoints |
| `--unique-identifier` | `-u` | | Unique identifier for the loopback pair |
| `--association-id` | `-i` | | Id (must be a valid GUID) used to associate the two endpoints. Also used when removing the loopback pair |

```
midi loopback create --name-a "My Loopback A" --name-b "My Loopback B"
midi loopback create --root-name "My Loopback"
```

## loopback remove

*Alias: `delete`*

Remove a pair of loopback endpoints by providing the association id.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--association-id` | `-i` | | The Guid which uniquely identifies the loopback endpoint pair |

```
midi loopback remove --association-id {bb872b25-bc38-4009-a85a-559824398a13}
```

## basic-loopback create

*Branch aliases: `midi1-loopback`, `simple-loopback`*

Create a temporary basic MIDI 1.0 loopback endpoint for connecting two MIDI 1.0 applications together.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--name` | `-n` | | The name of the basic MIDI 1.0 loopback endpoint |
| `--unique-identifier` | `-u` | | Unique identifier for the MIDI 1.0 loopback endpoint |
| `--association-id` | `-i` | | Id (must be a valid GUID) used to identify the loopback before and after it has been created. Used when removing the loopback |

```
midi basic-loopback create --name "My Loopback"
```

## basic-loopback remove

*Alias: `delete`*

Remove a basic MIDI 1.0 loopback endpoint by providing the association id.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--association-id` | `-i` | | The Guid which uniquely identifies the loopback endpoint pair |

```
midi basic-loopback remove --association-id {bb872b25-bc38-4009-a85a-559824398a13}
```

---

# Bridge

*Branch alias: `connect`*

## bridge ble

*Alias: `bluetooth`*

Create a new-API and WinMM-visible BLE endpoint by bridging to the WinRT MIDI 1.0 API and a new UMP loopback endpoint.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--input` | `-i` | | The WinRT MIDI 1.0 BLE input port Id to bridge from |
| `--output` | `-o` | | The WinRT MIDI 1.0 BLE output port Id to bridge from |
| `--name` | `-n` | | Name to use for the newly created endpoint |
| `--quiet` | `-q` | | Suppress detailed output |

```
midi bridge ble
midi connect bluetooth --name "My BLE Keyboard"
```

---

# Service

*Branch alias: `svc`*

The `midi service` command provides information about the status and health of the MIDI Service. This can be useful when troubleshooting.

> **Note:** Starting, stopping, restarting, and changing the start type of the MIDI service are standard Windows service management operations, and are no longer part of the MIDI Console. Use the built-in Windows tooling for those tasks: the Services applet (`services.msc`), `sc.exe`, or the PowerShell `Start-Service`, `Stop-Service`, `Restart-Service`, and `Set-Service` cmdlets. For example, `sudo pwsh -c "Restart-Service MidiSrv"`.

![MIDI Service Command]({{ site.baseurl }}/assets/images/console-midi-service.png)

## service status

Check to see if the Windows Service is running.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--verbose` | `-v` | | Report additional details about the service |

```
midi service status
midi svc status --verbose
```

![MIDI Service Status Command]({{ site.baseurl }}/assets/images/console-midi-service-status.png)

## service ping

Ping the MIDI Windows Service. This calls the app SDK functions to send a proprietary ping through the Ping endpoint in the service, and returns information about the round-trip speed characteristics of the ping series.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--count` | `-c` | `20` | Number of times to ping the MIDI Windows Service |
| `--timeout` | `-t` | `10000` | Total ping timeout in milliseconds. Does not include the session and connection creation and teardown time |
| `--verbose` | `-v` | | Show details from each ping request and response |

```
midi service ping --verbose
midi service ping --verbose --count 20 --timeout 20000
```

![MIDI Service Ping Command]({{ site.baseurl }}/assets/images/console-midi-service-ping-verbose.png)

---

# Watching for Changes

Enumerating gives you a snapshot of the list at a moment in time. Watching gives you a constantly updating list which reflects device add/remove as well as property updates. This is useful for developers, or for those using tools to modify endpoints who want to verify that the changes were reported.

To stop watching, hit the `escape` key.

## watch-endpoints

*Alias: `watch-ump`*

Watch UMP endpoints for add/remove and PnP property change notifications.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--include-loopback` | `-l` | | Include the diagnostic loopback endpoints |
| `--verbose` | `-v` | | Include more details for each endpoint |

```
midi watch-endpoints
midi watch-ump --include-loopback
```

## watch-ports

*Alias: `watch-legacy`*

Watch MIDI 1.0 ports for add/remove and PnP property change notifications.

| Option | Short | Default | Description |
| ----- | ----- | ----- | ----- |
| `--verbose` | `-v` | | Set for detailed output |

```
midi watch-ports
midi watch-legacy --verbose
```

---

# Time

*Alias: `clock`*

Get the current MIDI clock timestamp value and information about the clock resolution. It displays the current timestamp in ticks, and the number of ticks per second. This command has no options beyond `--help`.

```
midi time
midi clock
```

![MIDI Clock Command]({{ site.baseurl }}/assets/images/console-midi-clock.png)

---

## Technical Information

The Windows MIDI Services Console app has been developed using C#, .NET 10, the MIT-licensed open source [Spectre.Console](https://spectreconsole.net/) library, and the Microsoft-developed open source [C#/WinRT](https://learn.microsoft.com/windows/apps/develop/platform/csharp-winrt/) toolkit.

The console uses the same Windows MIDI Services WinRT APIs available to other desktop applications. Its full source code is available [on our Github repo](https://aka.ms/midirepo). Pull-requests, feature requests, and bug reports welcome. The project is open source, but we request that instead of forking it to create your own version, you consider contributing to the project.
