
  static void Init@(Model.Name)Enum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("@(Model.Name)").ToLocalChecked(), enumObject);
    @foreach(var name in Enum.GetNames(Model)) {
    @:Nan::Set(enumObject, Nan::New<String>("@(TX.Uncap(name))").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(@TX.ToWinRT(Model)::@(name))));
    }
  }
