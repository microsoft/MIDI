// Copyright (c) The NodeRT Contributors
// All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the ""License""); you may
// not use this file except in compliance with the License. You may obtain a
// copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
// OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
// IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
//
// See the Apache Version 2.0 License for specific language governing permissions
// and limitations under the License.

// TODO: Verify that this is is still needed..
#define NTDDI_VERSION 0x06010000

#include <v8.h>
#include "nan.h"
#include <string>
#include <ppltasks.h>
#include "CollectionsConverter.h"
#include "CollectionsWrap.h"
#include "node-async.h"
#include "NodeRtUtils.h"
#include "OpaqueWrapper.h"
#include "WrapperBase.h"

#using <@(Model.Assembly.GetName().Name).WinMD>

// this undefs fixes the issues of compiling Windows.Data.Json, Windows.Storag.FileProperties, and Windows.Stroage.Search
// Some of the node header files brings windows definitions with the same names as some of the WinRT methods
#undef DocumentProperties
#undef GetObject
#undef CreateEvent
#undef FindText
#undef SendMessage

const char* REGISTRATION_TOKEN_MAP_PROPERTY_NAME = "__registrationTokenMap__";

using v8::Array;
using v8::String;
using v8::Value;
using v8::Boolean;
using v8::Integer;
using v8::FunctionTemplate;
using v8::Object;
using v8::Local;
using v8::Function;
using v8::Date;
using v8::Number;
using v8::PropertyAttribute;
using v8::Primitive;
using Nan::HandleScope;
using Nan::Persistent;
using Nan::Undefined;
using Nan::True;
using Nan::False;
using Nan::Null;
using Nan::MaybeLocal;
using Nan::EscapableHandleScope;
using Nan::HandleScope;
using Nan::TryCatch;
using namespace concurrency;

@foreach(var name in Model.Namespaces) @("namespace " + name + " { ")

@TX.CppTemplates.TypeWrapperForwardDeclaration(Model)

@foreach(var en in Model.Enums) {
  @TX.CppTemplates.Enum(en);
}

@foreach(var vt in Model.ValueTypes) {
  @TX.CppTemplates.ValueType(vt);
}

@foreach(var vt in Model.ExternalReferencedValueTypes) {
  @TX.CppTemplates.ValueType(vt);
}

@foreach(var t in Model.Types.Values) {
  @TX.CppTemplates.Type(t);
}

@foreach(var name in Model.Namespaces) @("} ")


NAN_MODULE_INIT(init) {
  // We ignore failures for now since it probably means that
  // the initialization already happened for STA, and that's cool

  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  /*
  if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
    Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"error in CoInitializeEx()")));
    return;
  }
  */

  @{
    var namespacePrefix = "";
    foreach(var name in Model.Namespaces) {
      namespacePrefix += name + "::";
    }

    foreach(var en in Model.Enums) {
      @:@(namespacePrefix)Init@(en.Name)Enum(target);
    }

    foreach(var t in Model.Types.Values) {
      @:@(namespacePrefix)Init@(t.Name)(target);
    }
  }

  NodeRT::Utils::RegisterNameSpace("@(Model.WinRTNamespace)", target);
}

@{
  var moduleName = Model.Namespaces[0];
  var counter = 0;

  for (var i=1; i< Model.Namespaces.Count; i++) {
    moduleName += "_" + Model.Namespaces[i];
  }
}

NODE_MODULE(binding, init)
