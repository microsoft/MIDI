---
layout: kb
title: How to Create Loopback Endpoint Pairs from Code
audience: developers
description: How to create loopback endpoint pairs from code
---

# How to create simple Loopback Endpoint Pairs at runtime

We'll assume here you've already initialized Windows MIDI Services and created a session.

First, you define the two sides of the loopback. Because UMP endpoints are bidirectional, the loopback works from either direction: Messages set out on A arrive in on B, and those sent out on B arrive in on A.

```cpp
MidiLoopbackEndpointDefinition definitionA;
MidiLoopbackEndpointDefinition definitionB;

// A-side of the loopback
definitionA.Name = L"Sample App Loopback A";
definitionA.Description = L"The first description is optional, but is displayed to users. This becomes the transport-defined description.";
definitionA.UniqueId = L"8675309-OU812-5150";

// B-side of the loopback
definitionB.Name = L"Sample App Loopback B";
definitionB.Description = L"The second description is optional, but is displayed to users. This becomes the transport-defined description.";
definitionB.UniqueId = L"3263827-OU812-5150"; // can be the same as the first one, but doesn't need to be.
```

Next, create the transient (meaning they are not in the config file and therefore are not recreated after a reboot) loopback endpoint pair using the above definitions

```cpp
MidiLoopbackEndpointCreationConfig creationConfig(m_associationId, definitionA, definitionB);

auto response = MidiLoopbackEndpointManager::CreateTransientLoopbackEndpoints(creationConfig);

if (response.Success)
{
    std::cout << "Endpoints created successfully" << std::endl << std::endl;

    std::cout
        << "Loopback Endpoint A: " << std::endl 
        << " - " << winrt::to_string(definitionA.Name) << std::endl
        << " - " << winrt::to_string(response.EndpointDeviceIdA) << std::endl << std::endl;

    std::cout 
        << "Loopback Endpoint B: "  << std::endl
        << " - " << winrt::to_string(definitionB.Name) << std::endl
        << " - " << winrt::to_string(response.EndpointDeviceIdB) << std::endl << std::endl;

    m_endpointAId = response.EndpointDeviceIdA;
    m_endpointBId = response.EndpointDeviceIdB;
}
else
{
    // failed to create the loopback pair. It may be that the unique
    // Ids are already in use.
}
```

One thing you may have noticed in the listing above is the use of an **association Id**. This identifier is a GUID you generate and then use to associate the endpoint pairs together. This is what establishes the relationship between the two endpoints.

```cpp
winrt::guid m_associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();
```

That's all that's needed. You can connect to and open either endpoint and use it as you would any other.

## Sample Code

* [C++ / WinRT Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/loopback-endpoints/main.cpp)
