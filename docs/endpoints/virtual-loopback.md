---
layout: page
title: Virtual Loopback
parent: Transport Types
grandparent: Windows MIDI Services
has_children: true
---

# Virtual Loopback

| Property | Value |
| -------- | ----- |
| Abstraction Id | `{942BF02D-93C0-4EA8-B03E-D51156CA75E1}` |
| Mnemonic | `LOOP` |

## Overview

A Virtual Loopback is a mechanism for two or more applications to communicate with each other over MIDI. 

## Suggested Uses

If you want to have loopback endpoints which are always available for routing between applications, but do not want them to be controlled by the applications themselves, this is the right kind of endpoint to set up.

## Configuration

As with all configuration file changes, we recommend using the Windows MIDI Services Settings application, once we make that available. For now, you may edit the JSON directly. But please note that JSON is quite unforgiving: the format is specific, and all keys (including the GUIDs and property names) are case-sensitive. In addition, there's no usable provision for comments in a JSON file, so we can't include examples in the file itself.

That out of the way, here's the configuration section for the Virtual Loopback MIDI endpoints.

```json

todo

```

# Implementation

Internally, the Virtual Loopback is implemented as two endpoints which are cross-wired, so anything sent to Loopback A arrives on the input of Loopback B, and vice versa. Each declared pair has an exclusive relationship, and there's no practical limit to the number of loopback pairs you can define.

