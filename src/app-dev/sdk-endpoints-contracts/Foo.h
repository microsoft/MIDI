#pragma once
#include "Foo.g.h"


namespace winrt::Microsoft::Devices::Midi2::ClientEndpointContracts::implementation
{
    struct Foo : FooT<Foo>
    {
        Foo() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::ClientEndpointContracts::factory_implementation
{
    struct Foo : FooT<Foo, implementation::Foo>
    {
    };
}
