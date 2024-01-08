@foreach(var t in Model.Types.Values) {
  @:v8::Local<v8::Value> Wrap@(t.Name)(@(TX.ToWinRT(t.Type)) wintRtInstance);
  @:@TX.ToWinRT(t.Type) Unwrap@(t.Name)(Local<Value> value);
  @:
}