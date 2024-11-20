// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidiClientInitializerModule : public ATL::CAtlDllModuleT<CMidiClientInitializerModule>
{
public :
    HINSTANCE HModuleInstance{};

    DECLARE_LIBID(LIBID_WindowsMidiServicesClientInitializationLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDICLIENTINITIALIZER, "{abacab11-1984-5150-0812-ac8675309b04}")



};

extern class CMidiClientInitializerModule _AtlModule;
