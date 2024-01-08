@{
  Type t = Model.EventInfo.EventHandlerType;
  Type [] eventArgs = t.GetMethods().Where((methodInfo) => { return (methodInfo.Name == "Invoke"); }).First().GetParameters().Select<System.Reflection.ParameterInfo, Type>((pi) => { return pi.ParameterType; }).ToArray();

  var foreachArg = new Func<String, int, String>((format, len) => {
    return TX.ForEachType(eventArgs, format, len);
  });
}
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          @if (Model.IsStatic)
          {
          @:registrationToken = @(TX.ToWinRT(Model.EventInfo.DeclaringType, false))::@(Model.EventInfo.Name)::add(
          } else {
          @:registrationToken = wrapper->_instance->@(Model.EventInfo.Name)::add(
          }
            ref new @(TX.ToWinRT(Model.EventInfo.EventHandlerType,false))(
            [callbackObjPtr](@foreachArg("{1} arg{2}, ", 2)) {
              NodeUtils::Async::RunOnMain([callbackObjPtr @foreachArg(", arg{2}", 0)]() {
                HandleScope scope;

                @{var k = 0;}
                @foreach (var type in eventArgs)
                {
                @:Local<Value> wrappedArg@(k);
                  k++;
                }

                {
                  TryCatch tryCatch;

                  @{var j = 0;}
                  @foreach (var type in eventArgs)
                  {
                    var jsConversionInfo = Converter.ToJS(type, TX.MainModel.Types.ContainsKey(type));
                  @:wrappedArg@(j) = @(string.Format(jsConversionInfo[1], String.Format("arg{0}", j)));
                    j++;
                  }

                  @{var i = 0;}
                  @foreach (var type in eventArgs)
                  {
                  @:if (wrappedArg@(i).IsEmpty()) wrappedArg@(i) = Undefined();
                    i++;
                  }
                }

                @if (eventArgs.Length > 0) {
                @:Local<Value> args[] = { @foreachArg("wrappedArg{2}, ", 2) };
                @:Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                @:NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
                } else {
                @:Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                @:NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef,0, nullptr);
                }
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
