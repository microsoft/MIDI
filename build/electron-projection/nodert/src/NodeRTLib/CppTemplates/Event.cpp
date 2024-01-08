    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"@TX.Uncap(Model.Events[0].EventInfo.Name)", str))
      {
        @if (!Model.Events[0].IsStatic)
        {
        @:if (!NodeRT::Utils::IsWinRtWrapperOf<@(TX.ToWinRT(Model.Type,true))>(info.This()))
        @:{
        @:  Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
    @:  return;
        @:}
        @:@(Model.Name) *wrapper = @(Model.Name)::Unwrap<@(Model.Name)>(info.This());
        }
      @TX.CppTemplates.RegisterEventWithWinRT(Model.Events[0])
      }
      @for(var i=1; i<Model.Events.Length; i++) {
      @:else if (NodeRT::Utils::CaseInsenstiveEquals(L"@TX.Uncap(Model.Events[i].EventInfo.Name)", str))
      @:{
        @if (!Model.Events[i].IsStatic)
        {
        @:if (!NodeRT::Utils::IsWinRtWrapperOf<@(TX.ToWinRT(Model.Type,true))>(info.This()))
        @:{
        @:  Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
    @:  return;
        @:}
        @:@(Model.Name) *wrapper = @(Model.Name)::Unwrap<@(Model.Name)>(info.This());
        }
      @:@TX.CppTemplates.RegisterEventWithWinRT(Model.Events[i])
      @:}
      } else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if (@TX.ForEachEvent(Model.Events ,"(!NodeRT::Utils::CaseInsenstiveEquals(L\"{1}\", str)) &&", 3)) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"@TX.Uncap(Model.Events[0].EventInfo.Name)", str)) {
          @if (!Model.Events[0].IsStatic)
          {
          @:if (!NodeRT::Utils::IsWinRtWrapperOf<@(TX.ToWinRT(Model.Type,true))>(info.This()))
          @:{
          @:  Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
          @:  return;
          @:}
          @:@(Model.Name) *wrapper = @(Model.Name)::Unwrap<@(Model.Name)>(info.This());
          @:wrapper->_instance->@(Model.Events[0].EventInfo.Name)::remove(registrationToken);
          }
          else
          {
          @:@(TX.ToWinRT(Model.Events[0].EventInfo.DeclaringType, false))::@(Model.Events[0].EventInfo.Name)::remove(registrationToken);
          }
        }
        @for(var i=1; i<Model.Events.Length; i++) {
        @:else if (NodeRT::Utils::CaseInsenstiveEquals(L"@TX.Uncap(Model.Events[i].EventInfo.Name)", str))
        @:{
          @if (!Model.Events[i].IsStatic)
          {
          @:if (!NodeRT::Utils::IsWinRtWrapperOf<@(TX.ToWinRT(Model.Type,true))>(info.This()))
          @:{
          @:  Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
          @:  return;
          @:}
          @:@(Model.Name) *wrapper = @(Model.Name)::Unwrap<@(Model.Name)>(info.This());
          @:wrapper->_instance->@(Model.Events[i].EventInfo.Name)::remove(registrationToken);
          }
          else
          {
          @:@(TX.ToWinRT(Model.Events[i].EventInfo.DeclaringType, false))::@(Model.Events[i].EventInfo.Name)::remove(registrationToken);
          }
        @:}
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
