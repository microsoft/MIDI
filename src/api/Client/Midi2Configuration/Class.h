#pragma once

#include "Class.g.h"

namespace winrt::midi_configuration_api::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::midi_configuration_api::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
