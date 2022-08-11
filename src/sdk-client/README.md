# Client SDK

The prototype and initial rev of this client-side SDK is written in C#/WinRT. We will change to
C++/WinRT or C++/COM as we look at specific implementation use-cases and the results of the MIDI
questionnaire sent out in August. This is only for the client side and shared libraties to eliminate
any extra overhead in apps, and compatibility issues with other CLR versions, to ensure no CLR
loading in the app processes, and to allow for Arm64EC support. The service, tools, and other code 
will not be impacted.

**Note that because this version is written in C#/.NET, any .NET app using this SDK must use the same
CLR version that we are using.** You cannot have multiple CLR versions loaded into the same process.

C#/.NET is fast enough for what we're doing. It's really only the CLR load complexity and lack of
Arm64EC support which will drive this change.
