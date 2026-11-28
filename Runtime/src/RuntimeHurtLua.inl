// Included inside the governed RuntimeScriptRuntime::Impl.
bool HurtPatch(lua_State* l,int at,bridge::ScreenHurtFeedback& config,std::string& error,bridge::ScreenWidget* widget=nullptr)
{
    if(!lua_istable(l,at)){error="Pass a hurt feedback settings table.";return false;}at=lua_absindex(l,at);
    lua_pushnil(l);while(lua_next(l,at)){
        const auto key=NavString(l,-2);bool found=false,ok=false;
        for(const auto& f:bridge::HurtNumbers())if(key==f.key){found=true;const auto n=lua_tonumber(l,-1);ok=lua_type(l,-1)==LUA_TNUMBER&&std::isfinite(n)&&n>=f.minimum&&n<=f.maximum;if(ok)config.*(f.member)=float(n);break;}
        if(!found)for(const auto& f:bridge::HurtFlags())if(key==f.key){found=true;ok=lua_type(l,-1)==LUA_TBOOLEAN;if(ok)config.*(f.member)=lua_toboolean(l,-1)!=0;break;}
        if(!found)for(const auto& f:bridge::HurtResources())if(key==f.key){found=true;const auto path=NavString(l,-1);ok=lua_type(l,-1)==LUA_TSTRING&&bridge::HurtPath(path)&&NavPath(path,false,error);if(ok)config.*(f.member)=path;break;}
        if(!found&&(key=="colour"||key=="low_colour")){found=true;bridge::ScreenColor c;ok=lua_type(l,-1)==LUA_TSTRING&&NavColour(NavString(l,-1),c);if(ok)(key=="colour"?config.colour:config.lowColour)=(std::uint32_t(c.red)<<24)|(std::uint32_t(c.green)<<16)|(std::uint32_t(c.blue)<<8)|c.alpha;}
        if(!found&&widget&&(key=="x"||key=="y"||key=="width"||key=="height"||key=="opacity")){
            found=true;const auto n=lua_tonumber(l,-1);ok=lua_type(l,-1)==LUA_TNUMBER&&std::isfinite(n)&&std::abs(n)<=1e7;
            if(ok){if(key=="x")widget->rect.x=float(n);else if(key=="y")widget->rect.y=float(n);else if(key=="width")widget->rect.width=float(n);else if(key=="height")widget->rect.height=float(n);else widget->style.opacity=float(n);}
        }
        lua_pop(l,1);if(!found||!ok){lua_pop(l,1);if(error.empty())error="Invalid hurt feedback field: "+key;return false;}
    }
    return bridge::ValidateHurtFeedback(config,error);
}
static int HurtCreateLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self||!self->running)return PushNilError(l,"Hurt feedback requires an active Level.");
    auto w=bridge::MakePlayerHurtWidget();std::string error;bridge::StableId id;
    if(!self->HurtPatch(l,1,*w.hurt,error,&w)||!self->hurtFeedback.Create(std::move(w),id,error))return PushNilError(l,error);
    lua_pushlstring(l,id.data(),id.size());return 1;
}
static int HurtConfigureLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto id=NavString(l,1);const auto* view=self?self->hurtFeedback.View(id):nullptr;
    if(!view)return PushFalseError(l,"Unknown hurt feedback view.");auto w=view->widget;std::string error;
    if(!self->HurtPatch(l,2,*w.hurt,error,&w)||!self->hurtFeedback.ConfigureWidget(id,std::move(w),error))return PushFalseError(l,error);
    lua_pushboolean(l,1);return 1;
}
static int HurtGetLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto* view=self?self->hurtFeedback.View(NavString(l,1)):nullptr;
    if(!view)return PushNilError(l,"Unknown hurt feedback view.");const auto& c=*view->widget.hurt;lua_newtable(l);
    for(const auto& p:std::vector<std::pair<const char*,float>>{{"x",view->widget.rect.x},{"y",view->widget.rect.y},{"width",view->widget.rect.width},{"height",view->widget.rect.height},{"opacity",view->widget.style.opacity}}){lua_pushnumber(l,p.second);lua_setfield(l,-2,p.first);}
    for(const auto& f:bridge::HurtNumbers()){lua_pushnumber(l,c.*(f.member));lua_setfield(l,-2,f.key);}
    for(const auto& f:bridge::HurtFlags()){lua_pushboolean(l,c.*(f.member));lua_setfield(l,-2,f.key);}
    for(const auto& f:bridge::HurtResources()){const auto& path=c.*(f.member);lua_pushlstring(l,path.data(),path.size());lua_setfield(l,-2,f.key);}
    for(const auto& f:std::vector<std::pair<const char*,std::uint32_t>>{{"colour",c.colour},{"low_colour",c.lowColour}}){const char* digits="0123456789abcdef";std::string value;for(int i=7;i>=0;--i)value+=digits[(f.second>>(i*4))&15];lua_pushlstring(l,value.data(),value.size());lua_setfield(l,-2,f.first);}
    return 1;
}
static int HurtViewsLua(lua_State* l)
{
    auto* self=FromUpvalue(l);lua_newtable(l);int i=1;if(self)for(const auto& [id,v]:self->hurtFeedback.Snapshot().views){lua_pushlstring(l,id.data(),id.size());lua_rawseti(l,-2,i++);}return 1;
}
static int HurtRemoveLua(lua_State* l)
{
    auto* self=FromUpvalue(l);std::string error;if(!self||!self->hurtFeedback.Remove(NavString(l,1),error))return PushFalseError(l,error);lua_pushboolean(l,1);return 1;
}
static int HurtStateLua(lua_State* l)
{
    auto* self=FromUpvalue(l);lua_newtable(l);if(!self)return 1;const auto& f=self->hurtFeedback.Snapshot().frame;
    lua_pushboolean(l,f.available);lua_setfield(l,-2,"available");lua_pushboolean(l,f.alive);lua_setfield(l,-2,"alive");
    lua_pushnumber(l,f.healthFraction);lua_setfield(l,-2,"health_fraction");lua_pushinteger(l,f.sequence);lua_setfield(l,-2,"hit_sequence");
    lua_pushnumber(l,f.seconds);lua_setfield(l,-2,"seconds");lua_pushnumber(l,f.hits.empty()?-1:f.hits.back().age);lua_setfield(l,-2,"last_hit_age");return 1;
}
static int HurtHitLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self||!self->running||!self->hurtPulse)return PushFalseError(l,"Hurt feedback host unavailable.");
    const auto amount=lua_tonumber(l,1);if(lua_type(l,1)!=LUA_TNUMBER||!std::isfinite(amount)||amount<=0||amount>1e6||lua_gettop(l)>2)return PushFalseError(l,"Pass positive damage and optional world bearing.");
    std::optional<float> bearing;if(!lua_isnoneornil(l,2)){const auto angle=lua_tonumber(l,2);if(lua_type(l,2)!=LUA_TNUMBER||!std::isfinite(angle)||std::abs(angle)>360000)return PushFalseError(l,"Invalid world bearing.");bearing=float(angle);}
    std::string error;if(!self->hurtPulse(float(amount),bearing,error))return PushFalseError(l,error);lua_pushboolean(l,1);return 1;
}
static int HurtClearLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self||!self->running||!self->hurtClear||lua_gettop(l))return PushFalseError(l,"Hurt feedback host unavailable, or unexpected arguments.");
    self->hurtClear();lua_pushboolean(l,1);return 1;
}
void RegisterHurtLua()
{
    lua_newtable(lua);lua_pushinteger(lua,1);lua_setfield(lua,-2,"contract_version");
    struct F{const char* name;lua_CFunction fn;};const F functions[]={{"create",HurtCreateLua},{"configure",HurtConfigureLua},{"get",HurtGetLua},{"views",HurtViewsLua},{"remove",HurtRemoveLua},{"state",HurtStateLua},{"hit",HurtHitLua},{"clear",HurtClearLua}};
    for(const auto& f:functions){lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,f.fn,1);lua_setfield(lua,-2,f.name);}lua_setfield(lua,-2,"hurt");
}
