    static void @(Model.Name)Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<@(TX.ToWinRT(Model.DeclaringType,true))>(info.This())) {
        return;
      }

      @(Model.DeclaringType.Name) *wrapper = @(Model.DeclaringType.Name)::Unwrap<@(Model.DeclaringType.Name)>(info.This());

      try  {@{
        var jsConversionInfo = Converter.ToJS(Model.PropertyType, TX.MainModel.Types.ContainsKey(Model.PropertyType));
        var winrtConversionInfo = Converter.ToWinRT(Model.PropertyType);}
        @winrtConversionInfo[0] result = wrapper->_instance->@(Model.Name);
        info.GetReturnValue().Set(@string.Format(jsConversionInfo[1], "result"));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
