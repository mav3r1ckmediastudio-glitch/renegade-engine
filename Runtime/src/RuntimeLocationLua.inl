// Included inside the governed RuntimeScriptRuntime::Impl.
bool LocationPatch(lua_State* l,int at,bridge::WorldLocation& item,std::string& error)
{
    if(!lua_istable(l,at)){error="Pass location settings as a table.";return false;}at=lua_absindex(l,at);
    auto& p=item.profile;auto& a=p.announcement;
    lua_pushnil(l);while(lua_next(l,at)){
        const auto key=NavString(l,-2);bool ok=true;
        if(key=="id"||key=="name"||key=="type"||key=="icon"||key=="animation"){
            if(lua_type(l,-1)!=LUA_TSTRING)ok=false;
            else{
                const auto v=NavString(l,-1);
                if(key=="id"){if(!item.id.empty()&&item.id!=v){error="Location ID is read-only.";ok=false;}else item.id=v;}
                else if(key=="name")item.name=v;else if(key=="type")p.type=v;
                else if(key=="icon"){if(!NavPath(v,false,error))ok=false;else p.icon=v;}
                else if(v=="none")a.effect=bridge::LocationTextEffect::None;
                else if(v=="fade")a.effect=bridge::LocationTextEffect::Fade;
                else if(v=="glitch")a.effect=bridge::LocationTextEffect::Glitch;
                else if(v=="wobble")a.effect=bridge::LocationTextEffect::Wobble;
                else ok=false;
            }
        }else if(key=="position"){
            XMFLOAT3 v;if(!ReadVector3Table(l,-1,v,error))ok=false;else item.position={v.x,v.y,v.z};
        }else if(key=="show_on_compass"||key=="after_discovery"||key=="announcement"||key=="repeat_on_return"){
            if(lua_type(l,-1)!=LUA_TBOOLEAN)ok=false;else{
                const bool value=lua_toboolean(l,-1)!=0;
                if(key=="show_on_compass")p.showOnCompass=value;else if(key=="after_discovery")p.onlyAfterDiscovery=value;
                else if(key=="announcement")a.enabled=value;else a.repeatOnReturn=value;
            }
        }else if(key=="range"||key=="duration"||key=="fade_in"||key=="fade_out"||key=="strength"||key=="speed"){
            const auto v=lua_tonumber(l,-1);if(lua_type(l,-1)!=LUA_TNUMBER||!std::isfinite(v)||std::abs(v)>1e7)ok=false;
            else if(key=="range")a.range=float(v);else if(key=="duration")a.duration=float(v);else if(key=="fade_in")a.fadeIn=float(v);
            else if(key=="fade_out")a.fadeOut=float(v);else if(key=="strength")a.strength=float(v);else a.speed=float(v);
        }else {error="Unknown location setting: "+key;ok=false;}
        lua_pop(l,1);if(!ok){lua_pop(l,1);if(error.empty())error="Invalid location setting: "+key;return false;}
    }
    return bridge::ValidateLocationProfile(p,error);
}
static int LocationPutLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self||!self->running)return PushNilError(l,"Locations require an active Level.");
    std::string error;bridge::WorldLocation item;
    if(!self->LocationPatch(l,1,item,error))return PushNilError(l,error);
    if(item.id.empty())item.id=bridge::GenerateStableId();const auto id=item.id;
    if(!self->locations.Put(std::move(item),error))return PushNilError(l,error);
    lua_pushlstring(l,id.data(),id.size());return 1;
}
static int LocationConfigureLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto* v=self?self->locations.Get(NavString(l,1)):nullptr;
    if(!v)return PushFalseError(l,"Location is no longer active.");
    auto item=v->location;std::string error;
    if(!self->LocationPatch(l,2,item,error)||!self->locations.Put(std::move(item),error))return PushFalseError(l,error);
    lua_pushboolean(l,1);return 1;
}
static int LocationGetLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto* v=self?self->locations.Get(NavString(l,1)):nullptr;
    if(!v)return PushNilError(l,"Location is no longer active.");const auto& item=v->location;const auto& p=item.profile;const auto& a=p.announcement;
    lua_newtable(l);
    const auto str=[&](const char* key,const std::string& value){lua_pushlstring(l,value.data(),value.size());lua_setfield(l,-2,key);};
    const auto num=[&](const char* key,float value){lua_pushnumber(l,value);lua_setfield(l,-2,key);};
    const auto flag=[&](const char* key,bool value){lua_pushboolean(l,value);lua_setfield(l,-2,key);};
    str("id",item.id);str("name",item.name);str("type",p.type);str("icon",p.icon);
    const char* effects[]={"none","fade","glitch","wobble"};str("animation",effects[int(a.effect)]);
    num("range",a.range);num("duration",a.duration);num("fade_in",a.fadeIn);num("fade_out",a.fadeOut);num("strength",a.strength);num("speed",a.speed);
    flag("announcement",a.enabled);flag("repeat_on_return",a.repeatOnReturn);flag("show_on_compass",p.showOnCompass);flag("after_discovery",p.onlyAfterDiscovery);
    lua_newtable(l);lua_pushnumber(l,item.position.x);lua_setfield(l,-2,"x");lua_pushnumber(l,item.position.y);lua_setfield(l,-2,"y");lua_pushnumber(l,item.position.z);lua_setfield(l,-2,"z");lua_setfield(l,-2,"position");
    return 1;
}
static int LocationActionLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self||!self->running)return PushFalseError(l,"Locations require an active Level.");
    const auto op=lua_tointeger(l,lua_upvalueindex(2));const auto id=NavString(l,1);std::string error;
    if(op==3){self->locations.HideAnnouncement();lua_pushboolean(l,1);return 1;}
    const bool ok=op==0?self->locations.Remove(id,error):op==1?self->locations.Discover(id,error):self->locations.Announce(id,error);
    if(!ok)return PushFalseError(l,error);lua_pushboolean(l,1);return 1;
}
static int LocationStateLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self)return PushNilError(l,"Locations unavailable.");
    const auto* v=self->locations.Get(NavString(l,1));if(!v)return PushNilError(l,"Location is no longer active.");
    lua_newtable(l);lua_pushboolean(l,v->withinRange);lua_setfield(l,-2,"near");lua_pushboolean(l,v->discovered);lua_setfield(l,-2,"discovered");
    lua_pushboolean(l,v->fired);lua_setfield(l,-2,"announced");return 1;
}
void RegisterLocationLua()
{
    lua_newtable(lua);lua_pushinteger(lua,1);lua_setfield(lua,-2,"contract_version");
    struct F{const char* name;lua_CFunction function;};
    const F functions[]={{"register",LocationPutLua},{"configure",LocationConfigureLua},{"get",LocationGetLua},{"state",LocationStateLua}};
    for(const auto& f:functions){lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,f.function,1);lua_setfield(lua,-2,f.name);}
    const char* names[]={"remove","discover","announce","hide_announcement"};
    for(int i=0;i<4;++i){lua_pushlightuserdata(lua,this);lua_pushinteger(lua,i);lua_pushcclosure(lua,LocationActionLua,2);lua_setfield(lua,-2,names[i]);}
    lua_setfield(lua,-2,"locations");
}
