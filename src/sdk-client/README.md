# Client SDK

Rev 1 of this client-side SDK is written in C#/WinRT. It's likely we will change to C++/WinRT or C++/COM as we look at specific implementation use-cases and the results of the MIDI questionnaire sent out in August. This is only for the client side and shared libraties to eliminate any extra overhead in apps, and compatibility issues with other CLR versions. The service and other code will not be impacted.

Note that because this version is in C#/.NET, any .NET app using this SDK must use the same CLR version that we are using. You cannot have multiple CLR versions loaded into the same process.
