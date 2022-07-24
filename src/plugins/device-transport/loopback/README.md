# Loopback Plugin

This is a testing plugin that an end-user can use to set up a virtual
loopback device for testing functionality. By default, MIDI services will have
one loopback device set up which can be used for performance and function
testing. 

Function: Anything sent through the MIDI output comes right back through the 
MIDI input on this device.

## Implementation Notes

This plugin is being used for initial plugin interface prototoyping. Right now,
there's tight coupling with the rest of the system. Intent is to take what is 
learned here and then abstract out an interface we can use for plugins written
in C#, C++, etc.

IOW, don't make any assumptions based on this interface as it is implemented 
right now :)

Notes from the code regarding the use of streams in the interface

```csharp
// TODO: These need to use a type that C++ plugin devs will have access
// to, like am IBuffer or similar. .NET Streams aren't likely the
// best choice here. Also need to eval how the USB driver will work before
// settling on this interface. Don't want to use C++/CLI, but WinRT types
// and other COM-known types are an option.
```

Another option is that we could simply require using .NET for the plugins.
That's viable for this specific scenario as long as performance is what we 
need. That may be what we do with v 1.0, but it depends on the requirements
for the USB driver interface.
