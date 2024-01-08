@{
  for(var i = 0; i < Model.Events.Length; i++) {
    @:addListener(type: "@(Model.Events[i].EventInfo.Name)", listener: (ev: Event) => void): void ;
    @:removeListener(type: "@(Model.Events[i].EventInfo.Name)", listener: (ev: Event) => void): void ;
    @:on(type: "@(Model.Events[i].EventInfo.Name)", listener: (ev: Event) => void): void ;
    @:off(type: "@(Model.Events[i].EventInfo.Name)", listener: (ev: Event) => void): void ;
    @:
  }   
    @:addListener(type: string, listener: (ev: Event) => void): void ;
    @:removeListener(type: string, listener: (ev: Event) => void): void ;
    @:on(type: string, listener: (ev: Event) => void): void ;
    @:off(type: string, listener: (ev: Event) => void): void ;
    @:
}
