#pragma once

#include "Class.g.h"

namespace winrt::Microsoft_Windows_Midi::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::Microsoft_Windows_Midi::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
