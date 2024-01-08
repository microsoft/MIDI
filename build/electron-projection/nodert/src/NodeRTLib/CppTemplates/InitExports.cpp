
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("@(Model.Name)").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);

        @if(Model.MemberASyncMethods.Length > 0 || Model.StaticASyncMethods.Length > 0) {
        @:Local<Function> func;
        @:Local<FunctionTemplate> funcTemplate;
        }

        @if(Model.MemberSyncMethods.Length > 0) {
          @:
          foreach(var method in Model.MemberSyncMethods) {
            @:Nan::SetPrototypeMethod(localRef, "@(TX.Uncap(TX.CSharpMethodToCppMethod(method.Name)))", @(TX.CSharpMethodToCppMethod(method.Name)));
          }
          @:
        }

        @if(Model.MemberASyncMethods.Length > 0) {
          @:
          foreach(var method in Model.MemberASyncMethods) {
            @:Nan::SetPrototypeMethod(localRef, "@(TX.Uncap(TX.CSharpMethodToCppMethod(method.Name)))", @TX.CSharpMethodToCppMethod(method.Name));
          }
          @:
        }

        @if(Model.HasMemberEvents) {
          @:
          @:Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          @:Nan::SetPrototypeMethod(localRef,"on", AddListener);
          @:Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          @:Nan::SetPrototypeMethod(localRef, "off", RemoveListener);
        }

        @if(Model.MemberProperties.Length > 0) {
          @:
          foreach(var prop in Model.MemberProperties) {
            var propName = TX.Uncap(prop.Name);
            if (prop.GetSetMethod() != null) {
            @:Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("@(propName)").ToLocalChecked(), @(prop.Name)Getter, @(prop.Name)Setter);
            } else {
            @:Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("@(propName)").ToLocalChecked(), @(prop.Name)Getter);
            }
          }
        }

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        @{
          if(Model.StaticSyncMethods.Length > 0) {
          foreach(var method in Model.StaticSyncMethods) {
        @:Nan::SetMethod(constructor, "@(TX.Uncap(TX.CSharpMethodToCppMethod(method.Name)))", @TX.CSharpMethodToCppMethod(method.Name));
            }
          }
          if (Model.StaticASyncMethods.Length > 0) {
            foreach(var method in Model.StaticASyncMethods) {
        @:func = Nan::GetFunction(Nan::New<FunctionTemplate>(@TX.CSharpMethodToCppMethod(method.Name))).ToLocalChecked();
        @:Nan::Set(constructor, Nan::New<String>("@(TX.Uncap(TX.CSharpMethodToCppMethod(method.Name)))").ToLocalChecked(), func);
            }
          }

          if(Model.StaticProperties.Length > 0) {
            foreach(var prop in Model.StaticProperties) {
              var propName = TX.Uncap(prop.Name);
              if (prop.GetSetMethod() != null) {
        @:Nan::SetAccessor(constructor, Nan::New<String>("@(propName)").ToLocalChecked(), @(prop.Name)Getter, @(prop.Name)Setter);
              } else {
        @:Nan::SetAccessor(constructor, Nan::New<String>("@(propName)").ToLocalChecked(), @(prop.Name)Getter);
              }
            }
          }

          if (Model.HasStaticEvents) {
        @:
            if (!Model.HasMemberEvents) {
              @:Local<Function> addListenerFunc = Nan::GetFunction(Nan::New<FunctionTemplate>(AddListener)).ToLocalChecked();
              @:Nan::Set(constructor, Nan::New<String>("addListener").ToLocalChecked(), addListenerFunc);
              @:Nan::Set(constructor, Nan::New<String>("on").ToLocalChecked(), addListenerFunc);
            }

            if (!Model.HasMemberEvents) {
              @:Local<Function> removeListenerFunc = Nan::GetFunction(Nan::New<FunctionTemplate>(RemoveListener)).ToLocalChecked();
              @:Nan::Set(constructor, Nan::New<String>("removeListener").ToLocalChecked(), removeListenerFunc);
              @:Nan::Set(constructor, Nan::New<String>("off").ToLocalChecked(), removeListenerFunc);
            }
          }
        }

        Nan::Set(exports, Nan::New<String>("@(Model.Name)").ToLocalChecked(), constructor);
      }