---
layout: tools_page
title: MIDI Console Service Command
tool: console
description: Use the MIDI Console check service status, restart the service, etc.
---

The `midi service` command provides information about the status of the MIDI Service. This can be useful when troubleshooting.

```
midi service --help
```
![MIDI Service Command](/assets/images/console-midi-service.png)


## Check to see if the MIDI Service is running

If you want to see if the MIDI Service is running, you can check via the console.

```
midi service status
```

![MIDI Service Status Command](/assets/images/console-midi-service-status.png)

## Perform a ping test of the MIDI Service

Another diagnostic feature is the ping test. This calls the app SDK functions to send a proprietary ping through the Ping endpoint in the service. It then returns back information about the round-trip speed characteristics of the ping series.

```
midi service ping --verbose
```

![MIDI Service Status Command](/assets/images/console-midi-service-ping-verbose.png)

## Start and stop the MIDI Service

When run in an Administrator command prompt, the MIDI Console can be used to start and stop the service.

```
midi service restart
```

an alternative is to type

```
midi service stop
```

followed by 

```
midi service start
```

In current versions, the `midi service start` command will inform you that the service is already running, because just using the MIDI console will trigger the service to start.

## Set the service to auto-start

One of the requirements for shipping in Windows is to make the MIDI Service demand-start. However, that results in a multi-second delay when the first call is made to the service.

One way to avoid this, is to change the service's configuration so that it auto-starts with Windows. This causes a few seconds of delay on Windows startup, but then reduces the delay when applications first call into the service.

This must be run from an Administrator command prompt.

```
midi service set-auto-start
```

![MIDI Service Set-Auto-Start Command](/assets/images/console-midi-service-auto-start.png)

That changes the service startup from "Manual (Trigger Start)" to "Automatic". You can also do the same thing using the Services applet, or other command-line approaches (PowerShell, etc.). This is in the MIDI console for convenience.
