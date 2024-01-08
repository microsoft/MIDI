
    static void @(Model.Name)Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {@{
          var jsConversionInfo = Converter.ToJS(Model.PropertyType, TX.MainModel.Types.ContainsKey(Model.PropertyType));
          var winrtConversionInfo = Converter.ToWinRT(Model.PropertyType);
        }
        @winrtConversionInfo[0] result = @(TX.ToWinRT(Model.DeclaringType, false))::@(Model.Name);
        info.GetReturnValue().Set(@string.Format(jsConversionInfo[1], "result"));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
