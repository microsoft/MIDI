
    static void @(Model.Name)(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0 || !info[info.Length() -1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: No callback was given")));
        return;
      }

      @TX.ToWinRT(Model.Overloads[0].ReturnType) op;

      @{int c = 0;}
      @foreach(var overload in Model.Overloads) {
        int i = 0;
        var elseString = "";
        if (c > 0) {
          elseString = "else ";
        }
      @:@(elseString)if (info.Length() == @(overload.GetParameters().Length+1)@{if (overload.GetParameters().Length==0)@(")")}

        foreach (var paramInfo in overload.GetParameters()) {
          @:&& @(String.Format(Converter.TypeCheck(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType)), "info[" + i.ToString() + "]"))@{if (overload.GetParameters().Length==(i+1)) @(")")}
          i++;
        }
      @:{
        @:try
        @:{
          int parameterCounter = 0;
          foreach (var paramInfo in overload.GetParameters())
          {
          var winrtConversionInfo = Converter.ToWinRT(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType));

          @:@(winrtConversionInfo[0]) arg@(parameterCounter) = @(string.Format(winrtConversionInfo[1], "info[" +parameterCounter + "]" ));
          parameterCounter++;
          }

          if (overload.GetParameters().Length > 0) {
            @:
          }
          @:op = @TX.ToWinRT(Model.Overloads[0].DeclaringType, false)::@(overload.Name)(@{int j=0;foreach(var paramInfo in overload.GetParameters()){if(j>0)@(","); @("arg" + j.ToString()); j++;}});
        @:} catch (Platform::Exception ^exception) {
            @:NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
            @:return;
        @:}
      @:}
        c++;
      } else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }

      auto opTask = create_task(op);
      uv_async_t* asyncToken = NodeUtils::Async::GetAsyncToken(info[info.Length() -1].As<Function>());
      @{
        System.Reflection.MethodInfo[] returnTypeMethods = Model.Overloads[0].ReturnType.GetMethods();
        Type taskReturnType = returnTypeMethods.Where((methodInfo) => { return (methodInfo.Name == "GetResults"); }).First().ReturnType;
        string taskGenericType = TX.ToWinRT(taskReturnType);
      }
      opTask.then( [asyncToken] (task<@(taskGenericType)> t)
      {
        try {
          @{
            if (taskReturnType != typeof(void))
            {
          @:auto result = t.get();
          @:NodeUtils::Async::RunCallbackOnMain(asyncToken, [result](NodeUtils::InvokeCallbackDelegate invokeCallback) {
            }
            else
            {
          @:t.get();
          @:NodeUtils::Async::RunCallbackOnMain(asyncToken, [](NodeUtils::InvokeCallbackDelegate invokeCallback) {
            }
          }

            @{
              if (taskReturnType == typeof(void)) {
                @:Local<Value> args[] = {Undefined()};
              } else {
                var jsConversionInfo = Converter.ToJS(taskReturnType, TX.MainModel.Types.ContainsKey(taskReturnType));

            @:Local<Value> error;
            @:Local<Value> arg1;

            @:{
              @:TryCatch tryCatch;
              @:arg1 = @string.Format(jsConversionInfo[1], "result");

              @:if (tryCatch.HasCaught())
              @:{
              @:  error = Nan::To<Object>(tryCatch.Exception()).ToLocalChecked();
              @:}
              @:else
              @:{
              @:  error = Undefined();
              @:}

              @:if (arg1.IsEmpty()) arg1 = Undefined();
            @:}

            @:Local<Value> args[] = {error, arg1};
              }
            }

            invokeCallback(_countof(args), args);
          });
        }
        catch (Platform::Exception^ exception)
        {
          NodeUtils::Async::RunCallbackOnMain(asyncToken, [exception](NodeUtils::InvokeCallbackDelegate invokeCallback) {

            Local<Value> error = NodeRT::Utils::WinRtExceptionToJsError(exception);

            Local<Value> args[] = {error};
            invokeCallback(_countof(args), args);
          });
        }
      });
    }
