#include "pch.h"
#include "Class.h"
#include "Class.g.cpp"

namespace winrt::midi_app_sdk_endpoints_contracts::implementation
{
    int32_t Class::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Class::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
