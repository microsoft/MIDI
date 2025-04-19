---
layout: tools_page
title: Enumerate Endpoints with the MIDI Console
tool: console
description: How to use the MIDI Console to list the available Windows MIDI Services endpoints
---

## Enumerate MIDI UMP Endpoints

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

![MIDI enum endpoints]({{ site.baseurl }}/assets/images/console-midi-enum-endpoints.png)

## Enumerate Classic Byte-format (MIDI 1.0) Endpoints

This uses the old WinRT API. Its primary reason for existance is so you can see what's shown to older APIs vs what is shown for the new Windows MIDI Services API. As with the UMP endpoints, the commands have aliases, so the following are all equivalent

```
midi enumerate bytestream-endpoints
midi enumerate legacy-endpoints
midi enum legacy-endpoints
midi list legacy
```

## Watch UMP Endpoints for Changes

Enumerating endpoints gives you a snapshot of the list at a moment in time. Watching the endpoints will give you a constantly updating list, which reflects device add/remove as well as property updates. This is useful more for developers, or those who are using tools to modify endpoints and want to verify that the changes were reported.

The `watch-endpoints` command has the alias `watch`, so these are equivalent:

```
midi watch-endpoints
midi watch
```

Note that only UMP endpoints (or bytestream endpoints converted to UMP by the new USB driver and service) are watched for changes. The older MIDI API is not used here. When you want to stop watching the endpoints for changes, hit the `escape` key.

