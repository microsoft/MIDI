# MIDI Message Processing and Routing

MIDI messages are handled differently from service connection, control, and management messages.

## Goals

1. Meet all the functionality goals, including supporting message processors which can modify messages, remove messages, break messages out into new messages, change messages from MIDI 1.0 to MIDI 2.0, etc. and also virtual endpoints which can send messages back to other endpoints in a 1:1 or 1:n way.
2. Make it as easy and fast as possible to copy data from one location to another, without having to remove any metadata or tags
3. Minimize the number of times any block of data is copied

## Queues

Types of message queues:

MidiMessageLocalQueue
    Used for all service <-> endpoint communication. Local memory

MidiMessageSharedMemoryQueue
    Used for client <-> service communication. Shared memory (memory-mapped files)

Both implement IMidiMessageQueue. This is needed because internal routing of messages (one endpoint routing to another) doesn't require a client-side-visible queue.
TODO: What do we do if the client also opens that endpoint?

TODO: Do we even need routable queues? Maybe the endpoint (or processor) can simply be provided with the device graph and it can enqueue messages directly?

## Service-Specifics

### Client

The client puts messages in outgoing queues (one per endpoint per session) and listens for messages in incoming queues (one per endpoint per session). The queues aren't shared between sessions because each session will be at different points in the queue.

### Endpoint

There are two queues per endpoint.

1. Outgoing message queue. This is raw UMP words coming from the apps and going to the device. This is a local message queue on the service.
2. Incoming message queue. This is raw UMP words coming from the device and going to apps. This is a local message queue on the service. The router will read this and send the Words to all sessions which have the endpoint open.

The queues all handle words (value types) to avoid allocations and keep the code simple.

TODO: How does this work with multiple sessions which have the same endpoint open? The endpoint could have a list of outgoing queues per session, or all sessions could share the same outgoing queue instead of it being per-session. I think single outgoing queue is the right way to go here.

TODO: So given above, we need to think about lifetime for an endpoint. If Session1, Session2, and Session3 all have EndpointFoo open, and also EndpointVirtual has it open, we need to track that so we know when to close the endpoint for good. Need to use a ref counting or similar scheme here. Those are so easy to screw up, though, when things crash.

### Service

The service is responsible for creating and handing out message queues. It owns all the queues.

When session connects, there's no queue allocation. Only the meta channels are set up.

When session opens an endpoint, the service does the following:

1. Creates an address for the endpoint and provides it to the endpoint.
2. Creates the endpoint incoming and outgoing message queues
3. Creates the session endpoint incoming and outgoing message queues
4. Instantiates the endpoint on the device and provides it with the queues.

When an endpoint has an outgoing MIDI processing plugin added, the service does the following:

1. Creates an outgoing (from PC to device) message queue for the processing plugin.
2. Makes that outgoing queue the new destination queue for the endpoint (so plugin sits between router and )
3. Unhooks the outgoing message queue from the routing service (it does not change the address of the queue, or delete/recreate it)
4. Hooks the outgoing message queue into the processing plugin.

When an endpoint has an incoming MIDI processing plugin added, the service does the following



When session closes an endpoint, the service does the following:



## Notes

It would be better performance to simply supply the client with the endpoint queue directly. But then that limits what we can do with the messages in terms of filtering etc. It also eliminates the ability to 
