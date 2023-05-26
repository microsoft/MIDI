#pragma once

#include "Class.g.h"

namespace winrt::midi_app_sdk_network_endpoints::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::midi_app_sdk_network_endpoints::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
