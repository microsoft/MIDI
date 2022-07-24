# MIDI Setup

Originally called MIDI Profile (and later MIDI Configuration), but name changed to avoid confusion with 
MIDI 2.0 Profiles and configuration.

## Why

Musicians can have different MIDI setups for any number of reasons, including a setup for a specific
performance, an "away" and a "home" setup, and more. In those setups, different devices can be attached,
different routing parameters set, different virtual devices setup, and much more.

Additionally, these setups could be largely portable between PCs, and so need to be easily copied
and applied elsewhere, whether it is a replacement PC, a companion PC, or just sharing with a friend.

## Description

Infrastructure to enable loading, saving, configuring the graph of MIDI devices and endpoints. This is not part of the app-facing UI, as it shall only be used by the tools and the API itself.

## Approach

### Two levels of setup

* Master PC setup file for global settings, such as allowed plug-ins, transports, etc. Having this separate
prevents having to redo these settings for each possible setup
* Individual setup file, with a single default file, but the ability to create as many as needed

### What needs to be saved/loaded (high-level view)

Master PC Setup File

* Transport plugins and their main settings
* All other allowed and enabled plugins.

Individual Setup Files

* Name of the setup file
* Optional description of the file
* Optional icon path for the file
* Device Transport plugin parameters and data
  * Virtual Devices and streams
  * Enabled / Disabled settings for each enumerated device (including USB, BLE, Network ...)
  * Any custom device / stream names
  * Additional user metadata visible to apps (like whether or not to send clock to this device, or information about the device physical setup)
  * Potentially: End-user supplied "spoofed" MIDI CI data for non-CI-aware devices (this could get complex to manage, 
  however due to channels/groups)
* Any rules or plug-ins applied to devices and streams
* Any mapping applied to devices and streams
* Other future items, like any automation setups (which may point to other files) including commands to send to endpoints when the setup is loaded

### Format and Locations

Files will be human-readible and editable JSON files

The location is likely to be in the normal local app settings folders or a sub-folder of the documents folder. The tools will provide an easy way to open
that folder to manage the files.

Additionally, when feasible, the tools should provide a way to copy a section (for example, the RTP MIDI section)
from an existing setup file, without requiring manual copy/paste in a text editor.

### How data is loaded / saved

Two approaches under consideration: 

* Centralized code which creates the graph of objects, and fills them with data from the setup files
* Each object hydrates itself with information from the setup files, with a section passed in as part of building out the graph

No decision has been made yet.