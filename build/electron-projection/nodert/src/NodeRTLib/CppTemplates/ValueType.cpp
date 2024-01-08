  static bool Is@(Model.Name)JsObject(Local<Value> value) {
    if (!value->IsObject()) {
      return false;
    }

    Local<String> symbol;
    Local<Object> obj = Nan::To<Object>(value).ToLocalChecked();

    @foreach (var field in Model.GetFields())
    {
    @:symbol = Nan::New<String>("@TX.Uncap(field.Name)").ToLocalChecked();
    @:if (Nan::Has(obj, symbol).FromMaybe(false)) {
    @:  if (!@(String.Format(Converter.TypeCheck(field.FieldType, TX.MainModel.Types.ContainsKey(field.FieldType)), "Nan::Get(obj,symbol).ToLocalChecked()"))) {
    @:    return false;
    @:  }
    @:}
    @:
    }
    return true;
  }

  @TX.ToWinRT(Model) @(Model.Name)FromJsObject(Local<Value> value) {
    HandleScope scope;
    @TX.ToWinRT(Model) returnValue;

    if (!value->IsObject()) {
      Nan::ThrowError(Nan::TypeError(NodeRT::Utils::NewString(L"Unexpected type, expected an object")));
      return returnValue;
    }

    Local<Object> obj = Nan::To<Object>(value).ToLocalChecked();
    Local<String> symbol;

    @foreach (var field in Model.GetFields())
    {
    @:symbol = Nan::New<String>("@TX.Uncap(field.Name)").ToLocalChecked();
    @:if (Nan::Has(obj, symbol).FromMaybe(false)) {
      var winRtConversionInfo = Converter.ToWinRT(field.FieldType);
    @:  returnValue.@(field.Name) = @(String.Format(winRtConversionInfo[1],"Nan::Get(obj,symbol).ToLocalChecked()"));
    @:}
    @:
    }
    return returnValue;
  }

  Local<Value> @(Model.Name)ToJsObject(@TX.ToWinRT(Model) value) {
    EscapableHandleScope scope;

    Local<Object> obj = Nan::New<Object>();

    @foreach (var field in Model.GetFields())
    {
    var jsConversionInfo = Converter.ToJS(field.FieldType, TX.MainModel.Types.ContainsKey(field.FieldType));
    @:Nan::Set(obj, Nan::New<String>("@TX.Uncap(field.Name)").ToLocalChecked(), @string.Format(jsConversionInfo[1], "value." + field.Name));
    }

    return scope.Escape(obj);
  }
