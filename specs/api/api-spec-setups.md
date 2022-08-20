# API Specifications

## Setups

Setups are saved JSON configurations created by the user in the settings app. They contain custom
device and endpoint names, virtual ports, routing, plugin settings and more. Creating or changing
them via code was considered, but rejected due to end-user control and potential security concerns.

We will revisit if new requirements arise.

## Config files location

The configuration file is always in a known location. This is the one file which cannot be moved to a different location. Other paths are stored in this file.

Note that any path referenced in the file needs to have full read/write/delete permissions for the Local System account (for the service) and for the user (settings apps). The installer takes care of this.

## File format
