---
layout: doc
title: Windows MIDI Services App SDK Overview
---

# SDK Types

The [SDK namespaces and types in the metadata are documented here]({{"/sdk-reference/" | relative_url}}).

## Get started

To get started, you will need the compiler of your choice, until the types are in the official Windows SDK, the NuGet package for `Windows.Devices.Midi2` and if you are using C++, the `C++/WinRT` 3.x package. The latter is used to ingest the metadata and generate the necessary header files. C# developers will need the `C#/WinRT` package. Developers in other languages will need to speak to their compiler vendor to understand what tools they provide for generating WinRT projections.

Once set up, I recommend going to the samples area of the repo and learning how SDK and Service initialization happen, and then how to use the basic features of the SDK. The samples are all available [here](https://aka.ms/midisamples). The `cpp-winrt` folder contains the majority of the C++ code.

Before you get far into your own implementation, read [Best practices and performance optimizations]({{"/kb/best-practices/" | relative_url}}). It covers the things which are easy to get wrong early and expensive to change later, including how to send and receive messages efficiently, how many connections and sessions to open, and how to present endpoints, groups and function blocks to your users.

## Porting from WinMM or WinRT MIDI 1.0

If you have an existing MIDI 1.0 codebase, start with the article which matches what you are building.

| You are building | Start here |
| ---------------- | ---------- |
| An application | [Moving from WinMM to Windows MIDI Services]({{"/kb/moving-from-winmm-to-wms/" | relative_url}}) |
| A library, language binding or app framework that other people build on | [Porting a MIDI Library or Framework to Windows MIDI Services]({{"/kb/porting-midi-libraries/" | relative_url}}) ([aka.ms/MidiLibraryPorting](https://aka.ms/MidiLibraryPorting)) |

The application article explains how WinMM and WinRT MIDI 1.0 concepts map onto UMP endpoints, groups and channels. The library article assumes you have read it, and then covers what is different when you have a port-based public API you cannot break, including the WinMM habits which are now defects.

In both cases, read [Windows MIDI Services identifiers]({{"/kb/identifiers/" | relative_url}}) as well. Names are no longer durable identifiers, because customers can rename endpoints and MIDI 1.0 ports, so anything you persist needs to key off something else.

More developer articles, including transport plugin development, configuration files and troubleshooting, are in the [Knowledge Base]({{"/kb/" | relative_url}}).

## Using an AI coding assistant

If you are working with a coding agent, point it at the repository's own guidance rather than letting it work from what it already knows about MIDI on Windows. Most models have read a great deal of code which predates Windows MIDI Services, and a good deal of that code is wrong in ways this documentation exists to correct.

Agent guidance is not always discovered automatically, so it is worth naming these files explicitly in your prompt or your workspace configuration:

| File | What it covers |
| ---- | -------------- |
| [`AGENTS.md`](https://github.com/microsoft/MIDI/blob/main/AGENTS.md) | Entry point. Points at everything below. |
| [`.github/skills/midi-contributing/SKILL.md`](https://github.com/microsoft/MIDI/blob/main/.github/skills/midi-contributing/SKILL.md) | Writing or reviewing a code change to this repository, including when a change needs a servicing gate |
| [`.github/skills/midi-bug-reports/SKILL.md`](https://github.com/microsoft/MIDI/blob/main/.github/skills/midi-bug-reports/SKILL.md) | Scoping a defect, writing repro steps, and designing a test plan |
| [`.github/instructions/en-us-spelling.instructions.md`](https://github.com/microsoft/MIDI/blob/main/.github/instructions/en-us-spelling.instructions.md) | This project ships in en-US; en-GB spellings are defects |

If you are building an application or a library rather than contributing here, the two porting articles above are written to be read by an agent as well as by a person. [aka.ms/MidiLibraryPorting](https://aka.ms/MidiLibraryPorting) is a stable link you can hand to a coding assistant directly.
