    static void @(Model.Name)Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!@(String.Format(Converter.TypeCheck(Model.PropertyType, TX.MainModel.Types.ContainsKey(Model.PropertyType)),"value"))) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<@(TX.ToWinRT(Model.DeclaringType,true))>(info.This())) {
        return;
      }

      @(Model.DeclaringType.Name) *wrapper = @(Model.DeclaringType.Name)::Unwrap<@(Model.DeclaringType.Name)>(info.This());

      try {@{
        var winrtConversionInfo = Converter.ToWinRT(Model.PropertyType); }

        @(winrtConversionInfo[0]) winRtValue = @(string.Format(winrtConversionInfo[1], "value"));

        wrapper->_instance->@(Model.Name) = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
