# API Specifications

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

## Session Management

Sessions are the entrypoint into the API. In addition to this, they provide the required user-facing diagnostic data of which apps have which sessions and endpoints open.

```cpp
// MidiSessionSettings is where we can set flags and other session-global
// settings. For example, we may decide to have flags to control the way
// messages are received (polling, events, etc.)
MidiSessionSettings settings

// session name can be a project in the DAW, a tab in a browser, etc.
// The process name / id etc. is also captured behind the scenes
_session = MidiSession.Create("My song", settings)

// do stuff with session in here
...

// closing the session closes all the open endpoints/devices
// Good practice, but should also happen automatically if session goes out of scope
_session.Close()
```

TODO: We want the user to be in control. So we may allow them the ability to terminate a session using the settings tool (with appropriate warnings). If we do that, there will be a session terminated notification sent to the app which owns the session.

For regular applications, only the app owning the session can terminate it. We will not have a public API endpoint for apps to terminate other sessions.

## Session Settings

TODO
