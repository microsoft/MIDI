// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <roapi.h>
#include <wrl.h>
#include <Windows.Foundation.h>
#include <wrl/wrappers/corewrappers.h>

using namespace Microsoft::WRL::Wrappers;
using namespace Microsoft::WRL;
using namespace ABI::Windows::Foundation;




void MidiRegistrationTests::TestRegistration()
{
    std::cout << "** This test functions on the installed registrations. It does not use the project SDK files, so run only after manifest is registered" << std::endl;

    // loop through list of registered types and verify the Ro APIs can find them

    ComPtr<IUriRuntimeClassFactory> uriFactory;
    HRESULT hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
        IID_PPV_ARGS(&uriFactory)
    );

    if (SUCCEEDED(hr)) {
        // Use uriFactory to create instances of Uri
    }

}

