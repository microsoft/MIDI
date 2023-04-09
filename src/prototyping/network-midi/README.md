# Network MIDI Prototype

This is a prototype, not production code. The Protocol is not yet completed so please do not implement any application or device code based on what you see here.

The NuGet package generated from this code is also used directly by the settings app, currently, to demonstrate Network MIDI through the MIDI Monitor. The production version of this transport will be loaded into the service process, not any client process.

## Intent

The intent of Network MIDI 2.0 as a transport on Windows is to make it as transparent as possible to apps and users. Apps and users will be able to identify a device and endpoint as a Network MIDI 2.0 device or endpoint, but otherwise, they are treated like any other UMP Endpoint on Windows. That includes being multi-client, participating in filtering and routing in the service, and more.

On Windows, we strongly prefer apps not create their own ad-hoc network MIDI connections using UDP directly, but instead go through our SDK and API. The resulting user experience will be much better this way.

## Production aproach learned from the prototype

The prototype implements a very limited Network MIDI Host, but through discussions with other prototypers and network connections with other prototypes, it has been valuable to ferret out some of the considerations for the production implementation.

### Windows Network MIDI Host

By design, multiple remote network clients can connect to a single network MIDI host. By consensus in the MIDI Association, those remote clients need to be individually addressable by apps.

The MIDI Device is the entity tied to the host service and port. The MIDI Device is what gets advertised on mDNS and is what remote clients are actually connecting to. The user (or app) supplies the name of the device when creating it. Default name is the PC name and port. Product Instance Id is also provided by the user or app and does not have a default value.

For each remote client connection session that connects to that network port, the MIDI Device will create a new bidirectional UMP Endpoint that is 1:1 with that address & port pair. This is necessary for Type F stream messages to work and be meaningful and for clients to be individually addressable.

By default, the UMP Endpoint will be named with the address information.

The created MIDI Devices and UMP Endpoints are multi-client like any other device and UMP Endpoint on Windows.

Flow:

Please refer to protocol specification for overall workflow details. These notes here are just to clarify roles and overall approach.

* User or app creates the Windows Network MIDI host
* Windows Network MIDI Host advertises on mDNS
* Remote network MIDI client discovers service and connects to the host on this PC
* Remote network MIDI client sends an invitation message to the host
  * Host accepts the invitation (note that authentication negotiation is part of this process and not mentioned here)
  * If successful, host creates a new UMP Endpoint for this
  * Service updates PnP to add the new UMP Endpoint, or otherwise notifies apps that the new endpoint is available.

* Remote network MIDI client disconnects, fails to respond to a ping, or sends a Bye message
  * Host sends required bye reply as needed
  * Host destroys the UMP Endpoint
  * Service notifies connected apps (through PNP or other) that the UMP Endpoint is no longer present

### Windows Network MIDI Client

Apps and APIs may create any number of network MIDI clients. The resulting UMP Endpoints on Windows are multi-client, so any number of apps may connect to them just like USB or other MIDI Devices and UMP Endpoints.

Flow:

Please refer to protocol specification for overall workflow details. These notes here are just to clarify roles and overall approach.

* User or app discovers remote network MIDI host via mDNS or by directly entering an address and port
* Windows Service creates a new Network MIDI Client Device (MIDI Device)
* MIDI Device connects to the remote host
* MIDI Device sends invitation and handles negotiating any authentication based on user/API input
* If remote network MIDI host accepts connection
  * MIDI Device creates new UMP Endpoint
  * Windows Service notifies apps / pnp of new device and endpoint
* If remote network MIDI host rejects connection
  * Windows Services destroys the newly created MIDI Device

## Device and UMP Endpoint Persistence

UMP Endpoints are only valid when there is a connected remote network MIDI client or host.

Network MIDI Host MIDI Devices created by the user through the settings app are persistent across reboots. They will be stored in configuration and restarted when Windows MIDI Services restarts and the setup loaded. In this way, they become a reliable way for remote apps, computers, devices to connect to this PC.

Network MIDI Client Connection Devices go away when the remote host device disconnects\*, or the local service/PC is restarted.

\* We may want to revisit this once the use-cases are better understood. It may be desirable to keep the MIDI Device around and enumerated, but in an inactive state so the user can easily reconnect to the remote device when it is next advertised or otherwise made available. This may not be possible with ones based on an IP rather than mDNS (so no unique Service Instance Name), or in the case where multiple network MIDI hosts advertise with the same Service Instance Name information (two of the same hardware device, for example). The other challenge is there's no requirement to keep port numbers the same across restarts. So this is all under investigation. The values we should use are the TXT records (Endpoint Name and Product Instance ID), but currently, those are optional. I am proposing making them mandatory or very highly recommended, with details on why.

## Creating Network MIDI Connections through Apps

Applications may wish to create network MIDI client connections while staying inside the app. We are investigating the possibility of a "common dialog" type of approach for all runtime-creatable MIDI devices and UMP Endpoints, and then making those dialogs available to apps through a SDK call.
