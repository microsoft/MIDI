@{
  @:
@:@(Model.Name) = (function () {
  		
  @:var cls = function @(Model.Name)() {

    foreach(var field in Model.GetFields()) {
      @:this.@(TX.Uncap(field.Name)) = new @(Converter.ToJsDefinitonType(field.FieldType, TX.MainModel.Types.ContainsKey(field.FieldType)))();
    }

    @:};
  
  @:return cls;

@:}) ();
@:exports.@(Model.Name) = @(Model.Name);
@:
}