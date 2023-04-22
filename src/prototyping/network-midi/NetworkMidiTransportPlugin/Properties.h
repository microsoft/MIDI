#pragma once

// I don't like #defines, but this is easiest way to deal with this. No consts in IDL and no 
// messing about with the implementation type vs projected type. Could also have a dedicated
// class with these in it as const wchar_t* if that turns out to be more useful/safer
#define PKEY_Id                         L"Id"
#define PKEY_ConnectionType             L"ConnectionType"
#define PKEY_MidiEndpointName           L"MidiEndpointName"
#define PKEY_MidiProductInstanceId      L"MidiProductInstanceId"
#define PKEY_Server_Advertise           L"Server_Advertise"
#define PKEY_Server_ServiceInstanceName L"Server_ServiceInstanceName"
#define PKEY_Server_Port                L"Server_Port"
#define PKEY_Server_HostName            L"Server_HostName"


