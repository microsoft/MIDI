  class @(Model.Name) : public WrapperBase {
    public:
      @TX.CppTemplates.InitExports(Model)

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      @(Model.Name)(@TX.ToWinRT(Model.Type) instance) {
        _instance = instance;
      }

      @TX.CppTemplates.Constructor(Model)

      @TX.CppTemplates.CastFrom(Model)

      @foreach(var methodInfo in Model.MemberAsyncMethods) {
      @TX.CppTemplates.MemberASyncMethod(methodInfo)
      }

    @foreach(var methodInfo in Model.MemberSyncMethods) {
      if (TX.IsMethodNotImplemented(methodInfo)) {
      @TX.CppTemplates.NotImplementedMethod(methodInfo)
      } else {
        if (TX.IsIClosableClose(methodInfo.Overloads[0])) {
          @TX.CppTemplates.IClosableCloseMethod(methodInfo)
        } else {
          @TX.CppTemplates.MemberSyncMethod(methodInfo)
        }
      }
    }

    @foreach(var methodInfo in Model.StaticAsyncMethods) {
      @TX.CppTemplates.StaticASyncMethod(methodInfo)
    }

    @foreach(var methodInfo in Model.StaticSyncMethods) {
      @TX.CppTemplates.StaticSyncMethod(methodInfo)
    }

    @foreach(var propertyInfo in Model.MemberProperties) {
      @TX.CppTemplates.MemberPropertyGetter(propertyInfo)

      @:
      if (propertyInfo.GetSetMethod() != null) {
        @TX.CppTemplates.MemberPropertySetter(propertyInfo)
      @:
      }
    }

    @if (Model.StaticProperties.Length > 0)
    {
      @foreach(var propertyInfo in Model.StaticProperties)
      {
      @TX.CppTemplates.StaticPropertyGetter(propertyInfo)

      @:
        if (propertyInfo.GetSetMethod() != null) {
        @TX.CppTemplates.StaticPropertySetter(propertyInfo)
      @:
        }
      }
    }

    @if(Model.Events.Length > 0)
    {
      @TX.CppTemplates.Event(Model)
    }
    private:
      @(TX.ToWinRT(Model.Type)) _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> Wrap@(Model.Name)(@(TX.ToWinRT(Model.Type)) wintRtInstance);
      friend @(TX.ToWinRT(Model.Type)) Unwrap@(Model.Name)(Local<Value> value);
  };

  Persistent<FunctionTemplate> @(Model.Name)::s_constructorTemplate;

  v8::Local<v8::Value> Wrap@(Model.Name)(@(TX.ToWinRT(Model.Type)) winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(@(Model.Name)::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  @(TX.ToWinRT(Model.Type)) Unwrap@(Model.Name)(Local<Value> value) {
     return @(Model.Name)::Unwrap<@(Model.Name)>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void Init@(Model.Name)(Local<Object> exports) {
    @(Model.Name)::Init(exports);
  }

