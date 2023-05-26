#pragma once

#include "Class.g.h"

namespace winrt::midi_app_sdk_endpoints_contracts::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::midi_app_sdk_endpoints_contracts::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
