
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      @TX.ToWinRT(Model.Type) winRtInstance;
      @{
        int c = 0;
        var constructors = Model.Type.GetConstructors();
      }

      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<@TX.ToWinRT(Model.Type)>(info[0])) {
        try {
          winRtInstance = (@(TX.ToWinRT(Model.Type))) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      @foreach(var overload in constructors) {
        int i = 1;
      @:else if (info.Length() == @(overload.GetParameters().Length)@{if (overload.GetParameters().Length==0)@(")")}

        foreach (var paramInfo in overload.GetParameters()) {

        @:&& @(String.Format(Converter.TypeCheck(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType)), "info[" + (i-1).ToString() + "]"))@{if (overload.GetParameters().Length==i) @(")")}
          i++;
        }
      @:{
        @:try {
          int parameterCounter = 0;
          foreach (var paramInfo in overload.GetParameters()) {
          var winrtConversionInfo = Converter.ToWinRT(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType));

          @:@(winrtConversionInfo[0]) arg@(parameterCounter) = @(string.Format(winrtConversionInfo[1], "info[" +parameterCounter + "]" ));
          parameterCounter++;
          }

          if (overload.GetParameters().Length > 0) {
          @:
          }
          @:winRtInstance = ref new @(TX.ToWinRT(Model.Type, false))(@{int j=0;foreach(var paramInfo in overload.GetParameters()){if(j>0)@(","); @("arg" + j.ToString()); j++;}});
        @:} catch (Platform::Exception ^exception) {
          @:NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          @:return;
        @:}
      @:}
      } else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      @(Model.Name) *wrapperInstance = new @(Model.Name)(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }
