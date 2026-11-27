// Included inside the governed RuntimeScriptRuntime::Impl.
static std::string NavString(lua_State* l,int at)
{size_t n=0;const char* p=lua_type(l,at)==LUA_TSTRING?lua_tolstring(l,at,&n):nullptr;return p?std::string(p,n):std::string{};}
static bool NavColour(const std::string& text,bridge::ScreenColor& c)
{
    if(text.size()!=6&&text.size()!=8)return false;unsigned n=0;
    for(unsigned char x:text){int d=x>='0'&&x<='9'?x-'0':x>='a'&&x<='f'?x-'a'+10:x>='A'&&x<='F'?x-'A'+10:-1;if(d<0)return false;n=(n<<4)|d;}
    if(text.size()==6)n=(n<<8)|255;c={uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)};return true;
}
struct NavNumber {const char* key;float bridge::ScreenNavigation::*member;};
static const std::vector<NavNumber>& NavNumbers()
{static const std::vector<NavNumber> fields={{"span",&bridge::ScreenNavigation::fieldOfView},{"heading",&bridge::ScreenNavigation::manualHeading},{"marker_size",&bridge::ScreenNavigation::markerSize},{"marker_gap",&bridge::ScreenNavigation::markerGap},{"max_distance",&bridge::ScreenNavigation::maxDistance}};return fields;}
bool NavPath(const std::string& path,bool font,std::string& error)
{
    if(path.empty()||font&&path==bridge::BuiltinScreenFont)return true;
    std::string resolved;if(!bridge::ResolveScreenResourcePath(projectRoot,path,resolved,error))return false;
    if(!std::filesystem::is_regular_file(std::filesystem::u8path(resolved))){error="Navigation resource does not exist: "+path;return false;}
    return true;
}
bool NavTarget(lua_State* l,int at,bridge::NavigationTarget& t,std::string& error)
{
    if(!lua_istable(l,at)){error="Pass a navigation target table.";return false;}at=lua_absindex(l,at);
    lua_pushnil(l);while(lua_next(l,at))
    {
        const auto key=NavString(l,-2);bool ok=true;
        if(key=="id"||key=="label"||key=="category")
        {if(lua_type(l,-1)!=LUA_TSTRING)ok=false;else(key=="id"?t.id:key=="label"?t.label:t.category)=NavString(l,-1);}
        else if(key=="position"){XMFLOAT3 p;if(!ReadVector3Table(l,-1,p,error))ok=false;else t.position={p.x,p.y,p.z};}
        else if(key=="entity")
        {RuntimeScriptEntityReference ref;XMFLOAT3 p;if(!scene||!ReadEntityReferenceForApi(l,-1,ref,error))ok=false;
            else {RuntimeScriptEntityApi api(*scene,entitiesById,generation);if(!api.GetWorldPosition(ref,p,error))ok=false;else {t.entityId=ref.stableId;t.position={p.x,p.y,p.z};}}}
        else if(key=="priority"){const auto v=lua_tonumber(l,-1);if(lua_type(l,-1)!=LUA_TNUMBER||!std::isfinite(v)||std::floor(v)!=v||v< -1000||v>1000)ok=false;else t.priority=int(v);}
        else if(key=="visible"){if(lua_type(l,-1)!=LUA_TBOOLEAN)ok=false;else t.visible=lua_toboolean(l,-1)!=0;}
        else {error="Unknown navigation target field: "+key;ok=false;}
        lua_pop(l,1);if(!ok){lua_pop(l,1);if(error.empty())error="Invalid navigation target field: "+key;return false;}
    }
    return bridge::ValidateNavigationTarget(t,error);
}
bool NavPatch(lua_State* l,int at,bridge::ScreenWidget& w,std::string& error)
{
    if(!lua_istable(l,at)){error="Pass a navigation settings table.";return false;}at=lua_absindex(l,at);auto& c=*w.navigation;
    std::optional<bridge::ScreenColor> backingColour;
    lua_pushnil(l);while(lua_next(l,at))
    {
        const auto key=NavString(l,-2);bool ok=true;
        const auto number=std::find_if(NavNumbers().begin(),NavNumbers().end(),[&](const auto& n){return key==n.key;});
        if(key=="id"){if(NavString(l,-1)!=w.id)ok=false;}
        else if(number!=NavNumbers().end()){const auto v=lua_tonumber(l,-1);if(lua_type(l,-1)!=LUA_TNUMBER||!std::isfinite(v)||std::abs(v)>1e7)ok=false;else c.*(number->member)=float(v);}
        else if(key=="x"||key=="y"||key=="width"||key=="height"||key=="font_size"||key=="opacity"||key=="border_width")
        {const auto v=lua_tonumber(l,-1);if(lua_type(l,-1)!=LUA_TNUMBER||!std::isfinite(v)||std::abs(v)>1e7)ok=false;
            else if(key=="x")w.rect.x=float(v);else if(key=="y")w.rect.y=float(v);else if(key=="width")w.rect.width=float(v);else if(key=="height")w.rect.height=float(v);
            else if(key=="font_size")w.style.text.fontSize=float(v);else if(key=="opacity")w.style.opacity=float(v);else w.style.borderWidth=float(v);}
        else if(key=="show_names"||key=="show_distance"||key=="show_ticks"||key=="visible")
        {if(lua_type(l,-1)!=LUA_TBOOLEAN)ok=false;else {const bool v=lua_toboolean(l,-1)!=0;
            if(key=="show_names")c.showNames=v;else if(key=="show_distance")c.showDistance=v;else if(key=="show_ticks")c.showTicks=v;else w.visible=v;}}
        else if(key=="source"||key=="name"||key=="categories"||key=="strip"||key=="pointer"||key=="background"||key=="font")
        {
            if(lua_type(l,-1)!=LUA_TSTRING)ok=false;else {const auto value=NavString(l,-1);
                if(key=="source"){if(value=="camera")c.headingSource=bridge::NavigationHeadingSource::Camera;else if(value=="player")c.headingSource=bridge::NavigationHeadingSource::Player;else if(value=="manual")c.headingSource=bridge::NavigationHeadingSource::Manual;else ok=false;}
                else if(key=="name")w.name=value;else if(key=="categories")c.categoryFilter=value;
                else if(!NavPath(value,key=="font",error))ok=false;
                else if(key=="strip")c.bearingStrip=value;else if(key=="pointer")c.pointerImage=value;else if(key=="background")w.resourcePath=value;else w.style.text.fontResource=value;}
        }
        else if(key=="text_colour"||key=="background_colour"||key=="border_colour"||key=="image_tint"||key=="backing_colour")
        {bridge::ScreenColor colour;if(!NavColour(NavString(l,-1),colour))ok=false;else if(key=="border_colour")w.style.borderColor=colour;
            else if(key=="backing_colour")backingColour=colour;
            else for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})
                if(key=="text_colour")v->foreground=colour;else if(key=="image_tint")v->imageTint=colour;else v->background=colour;}
        else if(key=="icons")
        {
            if(!lua_istable(l,-1))ok=false;else {std::map<std::string,std::string> icons;const auto table=lua_absindex(l,-1);lua_pushnil(l);
                while(lua_next(l,table)){const auto name=NavString(l,-2),path=NavString(l,-1);if(lua_type(l,-2)!=LUA_TSTRING||lua_type(l,-1)!=LUA_TSTRING||name.empty()||!NavPath(path,false,error)){lua_pop(l,2);ok=false;break;}icons[name]=path;lua_pop(l,1);if(icons.size()>64){lua_pop(l,1);ok=false;break;}}
                if(ok)c.icons=std::move(icons);}
        }
        else if(key=="targets")
        {
            if(!lua_istable(l,-1)||lua_rawlen(l,-1)>64)ok=false;else {std::vector<bridge::NavigationTarget> targets;
                const auto table=lua_absindex(l,-1);const auto length=lua_rawlen(l,table);size_t count=0;lua_pushnil(l);
                while(lua_next(l,table)){const bool valid=lua_isinteger(l,-2)&&lua_tointeger(l,-2)>=1&&lua_tointeger(l,-2)<=lua_Integer(length);lua_pop(l,1);if(!valid){lua_pop(l,1);ok=false;break;}++count;}
                if(count!=length)ok=false;
                for(size_t i=1;ok&&i<=length;++i){lua_rawgeti(l,table,i);bridge::NavigationTarget t;const bool accepted=NavTarget(l,-1,t,error);lua_pop(l,1);if(!accepted){ok=false;break;}targets.push_back(std::move(t));}
                if(ok)c.targets=std::move(targets);}
        }
        else {error="Unknown navigation setting: "+key;ok=false;}
        lua_pop(l,1);if(!ok){lua_pop(l,1);if(error.empty())error="Invalid navigation setting: "+key;return false;}
    }
    // Lua table iteration order must not decide whether backing_colour targets
    // the plain panel or the image selected in this same atomic patch.
    if(backingColour)for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})
        if(w.resourcePath.empty())v->background=*backingColour;else v->imageTint=*backingColour;
    return bridge::ValidateScreenNavigation(c,error);
}
static int NavigationCreateLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self||!self->running)return PushNilError(l,"Navigation requires an active Level.");
    auto w=bridge::MakeWorldNavigationWidget();std::string error;bridge::StableId id;
    if(!self->NavPatch(l,1,w,error)||!self->navigation.Create(std::move(w),id,error))return PushNilError(l,error);
    lua_pushlstring(l,id.data(),id.size());return 1;
}
static int NavigationConfigureLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto id=NavString(l,1);const auto* view=self?self->navigation.View(id):nullptr;
    if(!view)return PushFalseError(l,"Navigation view is no longer active.");auto w=view->widget;std::string error;
    if(!self->NavPatch(l,2,w,error)||!self->navigation.Configure(id,std::move(w),error))return PushFalseError(l,error);
    lua_pushboolean(l,1);return 1;
}
static int NavigationAppearanceLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto id=NavString(l,1);const auto* view=self?self->navigation.View(id):nullptr;
    if(!view)return PushFalseError(l,"Navigation view is no longer active.");
    if(!lua_istable(l,2))return PushFalseError(l,"Pass an appearance settings table.");
    bool keep=true;
    if(!lua_isnoneornil(l,3)){if(lua_type(l,3)!=LUA_TBOOLEAN)return PushFalseError(l,"keep_icons must be a boolean.");keep=lua_toboolean(l,3)!=0;}
    const std::set<std::string> allowed={"icons","strip","pointer","background","font","font_size","text_colour","background_colour","backing_colour","image_tint","border_colour","border_width","opacity"};
    lua_pushnil(l);while(lua_next(l,2)){
        const auto key=NavString(l,-2);lua_pop(l,1);
        if(!allowed.count(key)){lua_pop(l,1);return PushFalseError(l,"Not an appearance setting: "+key);}
    }
    auto appearance=view->widget;appearance.navigation->icons.clear();std::string error;
    if(!self->NavPatch(l,2,appearance,error)||!self->navigation.ApplyAppearance(id,appearance,error,keep))return PushFalseError(l,error);
    lua_pushboolean(l,1);return 1;
}
static int NavigationRemoveLua(lua_State* l)
{
    auto* self=FromUpvalue(l);std::string error;if(!self||!self->navigation.Remove(NavString(l,1),error))return PushFalseError(l,error.empty()?"Navigation unavailable.":error);lua_pushboolean(l,1);return 1;
}
static int NavigationTargetsLua(lua_State* l)
{
    auto* self=FromUpvalue(l);std::string error;
    if(!self||!self->running)return PushFalseError(l,"Navigation requires an active Level.");
    const bool remove=lua_toboolean(l,lua_upvalueindex(2))!=0;bridge::NavigationTarget target;
    if(remove?!self->navigation.RemoveTarget(NavString(l,1),error):
        (!self->NavTarget(l,1,target,error)||!self->navigation.PutTarget(std::move(target),error)))return PushFalseError(l,error);
    lua_pushboolean(l,1);return 1;
}
static int NavigationGetLua(lua_State* l)
{
    auto* self=FromUpvalue(l);const auto* view=self?self->navigation.View(NavString(l,1)):nullptr;if(!view)return PushNilError(l,"Navigation view is no longer active.");
    const auto& w=view->widget;const auto& c=*w.navigation;lua_newtable(l);
    const auto str=[&](const char* key,const std::string& v){lua_pushlstring(l,v.data(),v.size());lua_setfield(l,-2,key);};
    const auto num=[&](const char* key,float v){lua_pushnumber(l,v);lua_setfield(l,-2,key);};
    const auto flag=[&](const char* key,bool v){lua_pushboolean(l,v);lua_setfield(l,-2,key);};
    str("id",w.id);str("name",w.name);str("source",c.headingSource==bridge::NavigationHeadingSource::Camera?"camera":c.headingSource==bridge::NavigationHeadingSource::Player?"player":"manual");
    str("categories",c.categoryFilter);str("strip",c.bearingStrip);str("pointer",c.pointerImage);str("background",w.resourcePath);str("font",w.style.text.fontResource);
    for(const auto& n:NavNumbers())num(n.key,c.*(n.member));num("x",w.rect.x);num("y",w.rect.y);num("width",w.rect.width);num("height",w.rect.height);num("font_size",w.style.text.fontSize);num("opacity",w.style.opacity);num("border_width",w.style.borderWidth);
    const auto colour=[&](const char* key,bridge::ScreenColor c){const char* digits="0123456789abcdef";std::string text;for(auto byte:{c.red,c.green,c.blue,c.alpha}){text+=digits[byte>>4];text+=digits[byte&15];}str(key,text);};
    colour("text_colour",w.style.normal.foreground);colour("background_colour",w.style.normal.background);colour("border_colour",w.style.borderColor);colour("image_tint",w.style.normal.imageTint);
    lua_newtable(l);int targetIndex=1;
    for(const auto& t:c.targets)
    {
        lua_newtable(l);const auto put=[&](const char* k,const std::string& v){lua_pushlstring(l,v.data(),v.size());lua_setfield(l,-2,k);};
        put("id",t.id);put("label",t.label);put("category",t.category);
        lua_pushboolean(l,t.visible);lua_setfield(l,-2,"visible");lua_pushinteger(l,t.priority);lua_setfield(l,-2,"priority");
        lua_newtable(l);lua_pushnumber(l,t.position.x);lua_setfield(l,-2,"x");lua_pushnumber(l,t.position.y);lua_setfield(l,-2,"y");lua_pushnumber(l,t.position.z);lua_setfield(l,-2,"z");lua_setfield(l,-2,"position");
        if(!t.entityId.empty()){self->PushEntityRef(t.entityId);lua_setfield(l,-2,"entity");}
        lua_rawseti(l,-2,targetIndex++);
    }
    lua_setfield(l,-2,"targets");
    flag("visible",w.visible);flag("show_names",c.showNames);flag("show_distance",c.showDistance);flag("show_ticks",c.showTicks);
    lua_newtable(l);for(const auto& [key,path]:c.icons){lua_pushlstring(l,path.data(),path.size());lua_setfield(l,-2,key.c_str());}lua_setfield(l,-2,"icons");
    return 1;
}
static int NavigationViewsLua(lua_State* l)
{
    auto* self=FromUpvalue(l);if(!self)return PushNilError(l,"Navigation unavailable.");lua_newtable(l);int i=1;
    for(const auto& [id,view]:self->navigation.Snapshot().views){lua_newtable(l);lua_pushlstring(l,id.data(),id.size());lua_setfield(l,-2,"id");lua_pushlstring(l,view.widget.name.data(),view.widget.name.size());lua_setfield(l,-2,"name");lua_rawseti(l,-2,i++);}return 1;
}
void RegisterNavigationLua()
{
    lua_newtable(lua);lua_pushinteger(lua,1);lua_setfield(lua,-2,"contract_version");
    struct F{const char* name;lua_CFunction function;};const F funcs[]={{"create",NavigationCreateLua},{"configure",NavigationConfigureLua},{"apply_appearance",NavigationAppearanceLua},{"get",NavigationGetLua},{"views",NavigationViewsLua},{"remove",NavigationRemoveLua}};
    for(const auto& f:funcs){lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,f.function,1);lua_setfield(lua,-2,f.name);}
    for(int i=0;i<2;++i){lua_pushlightuserdata(lua,this);lua_pushboolean(lua,i);lua_pushcclosure(lua,NavigationTargetsLua,2);lua_setfield(lua,-2,i?"remove_target":"put_target");}
    lua_setfield(lua,-2,"navigation");
}
