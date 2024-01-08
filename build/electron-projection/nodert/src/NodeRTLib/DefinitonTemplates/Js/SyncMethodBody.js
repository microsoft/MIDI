@{
@:{
    @:/// <signature>
    @:/// <summary>Function summary.</summary>
    foreach (var paramInfo in Model.GetParameters()) {
    @:/// <param name="@(paramInfo.Name)" type="@(Converter.ToJsDefinitonType(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType)))">A param.</param>
    }
    if (Model.ReturnType != typeof(void)) 
    {
    @:/// <returns type="@(Converter.ToJsDefinitonType(Model.ReturnType, TX.MainModel.Types.ContainsKey(Model.ReturnType)))" />
    }
    @:/// </signature>

    if (Model.ReturnType != typeof(void))
    {
    @:return new @(Converter.ToJsDefinitonType(Model.ReturnType, TX.MainModel.Types.ContainsKey(Model.ReturnType)))();
    }
  @:}
}