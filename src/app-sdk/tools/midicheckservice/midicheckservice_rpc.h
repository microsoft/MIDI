// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDICHECKSERVICE_RPC_H
#define MIDICHECKSERVICE_RPC_H

// this RPC code is used by this tool, but is not supported for use by applications 
// directly. It could change in the future, as an internal implementation detail of 
// Windows MIDI Services.

_Use_decl_annotations_
void __RPC_FAR* __RPC_USER midl_user_allocate(size_t byteCount);

_Use_decl_annotations_
void __RPC_USER midl_user_free(void __RPC_FAR* pointer);

bool VerifyMidiSrvConnectivity();


#endif

