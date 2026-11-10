// Included inside RuntimeScriptRuntime::Impl: governed Lua closures share S3 EntityRef ownership.
struct ParticleLayerValue {
 const char* name;double(*get)(const bridge::ParticleEffectLayer&);void(*set)(bridge::ParticleEffectLayer&,double);int kind;double low,high;
};
static const std::vector<ParticleLayerValue>& ParticleLayerValues(){
#define PN(n,m,k,lo,hi) {n,[](const bridge::ParticleEffectLayer& l)->double{return double(l.m);},[](bridge::ParticleEffectLayer& l,double v){l.m=static_cast<std::remove_reference_t<decltype(l.m)>>(v);},k,lo,hi}
 static const std::vector<ParticleLayerValue> values={
 PN("enabled",enabled,1,0,1),PN("playback",playback,2,0,1),
 PN("shape_type",shape.type,2,0,6),PN("shape_radius",shape.radius,0,0,10000),PN("shape_height",shape.height,0,0,10000),PN("shape_angle",shape.angle,0,0,90),PN("shape_thickness",shape.thickness,0,0,10000),PN("shape_surface",shape.surface,1,0,1),
 PN("emit_rate",emitter.emitCount,0,0,1000000),PN("size",emitter.size,0,.000001,10000),
 PN("lifetime",emitter.life,0,.001,10000),PN("life_variation",emitter.randomLife,0,0,10000),
 PN("randomness",emitter.randomFactor,0,0,10000),PN("normal_factor",emitter.normalFactor,0,-10000,10000),
 PN("velocity_x",emitter.velocity.x,0,-10000,10000),PN("velocity_y",emitter.velocity.y,0,-10000,10000),PN("velocity_z",emitter.velocity.z,0,-10000,10000),
 PN("gravity_x",emitter.gravity.x,0,-10000,10000),PN("gravity_y",emitter.gravity.y,0,-10000,10000),PN("gravity_z",emitter.gravity.z,0,-10000,10000),
 PN("drag",emitter.drag,0,0,10000),PN("rotation",emitter.rotation,0,-10000,10000),PN("motion_blur",emitter.motionBlurAmount,0,0,10000),
 PN("mass",emitter.mass,0,.0001,10000),PN("restitution",emitter.restitution,0,0,1),PN("colour_variation",emitter.randomColor,0,0,1),
 PN("red",emitter.color.x,0,0,1),PN("green",emitter.color.y,0,0,1),PN("blue",emitter.color.z,0,0,1),PN("opacity",emitter.color.w,0,0,1),
 PN("emission",emitter.emissiveStrength,0,0,100000),PN("emissive_red",emitter.emissiveColor.x,0,0,10000),PN("emissive_green",emitter.emissiveColor.y,0,0,10000),PN("emissive_blue",emitter.emissiveColor.z,0,0,10000),
 PN("particle_scale_x",emitter.scaleX,0,.001,10000),PN("particle_scale_y",emitter.scaleY,0,.001,10000),
 PN("fade_start",emitter.opacityPeakStart,0,0,1),PN("fade_end",emitter.opacityPeakEnd,0,0,1),
 PN("columns",emitter.framesX,2,1,32),PN("rows",emitter.framesY,2,1,32),PN("frames",emitter.frameCount,2,1,1024),PN("start_frame",emitter.frameStart,2,0,1023),PN("fps",emitter.frameRate,0,0,120),
 PN("max_particles",emitter.maxParticles,2,1,1000000),PN("burst_on_create",emitter.burstOnCreate,2,0,1000000),
 PN("fixed_timestep",emitter.fixedTimestep,0,-1,1),PN("shader",emitter.shaderType,2,0,3),PN("blend",material.blendMode,2,0,5),
 PN("paused",emitter.paused,1,0,1),PN("sorted",emitter.sorted,1,0,1),PN("depth_collision",emitter.depthCollision,1,0,1),
 PN("sph",emitter.sph,1,0,1),PN("spawn_box",emitter.volume,1,0,1),PN("ignore_colliders",emitter.collidersDisabled,1,0,1),
 PN("mesh_colour",emitter.takeColorFromMesh,1,0,1),PN("blend_frames",emitter.frameBlending,1,0,1),
 PN("sph_smoothing",emitter.sphH,0,.001,10000),PN("sph_pressure",emitter.sphK,0,0,10000),PN("sph_density",emitter.sphP0,0,.001,10000),PN("sph_viscosity",emitter.sphE,0,0,10000),
 PN("size_variation",sizeVariation,0,0,1),PN("speed_variation",speedVariation,0,0,1),
 PN("field_1_weight",fieldInfluence[0],0,0,1),PN("field_2_weight",fieldInfluence[1],0,0,1),PN("field_3_weight",fieldInfluence[2],0,0,1),PN("field_4_weight",fieldInfluence[3],0,0,1),
 PN("offset_x",transform.translation.x,0,-1000000,1000000),PN("offset_y",transform.translation.y,0,-1000000,1000000),PN("offset_z",transform.translation.z,0,-1000000,1000000),
 PN("layer_scale_x",transform.scale.x,0,.001,10000),PN("layer_scale_y",transform.scale.y,0,.001,10000),PN("layer_scale_z",transform.scale.z,0,.001,10000),
 PN("qx",transform.rotation.x,0,-10000,10000),PN("qy",transform.rotation.y,0,-10000,10000),PN("qz",transform.rotation.z,0,-10000,10000),PN("qw",transform.rotation.w,0,-10000,10000)};
#undef PN
 return values;
}
struct ParticleFieldValue {
 const char* name;double(*get)(const bridge::ParticleMovementField&);void(*set)(bridge::ParticleMovementField&,double);int kind;double low,high;
};
static const std::vector<ParticleFieldValue>& ParticleFieldValues(){
#define PF(n,m,k,lo,hi) {n,[](const bridge::ParticleMovementField& f)->double{return double(f.m);},[](bridge::ParticleMovementField& f,double v){f.m=static_cast<decltype(f.m)>(v);},k,lo,hi}
 static const std::vector<ParticleFieldValue> values={
 PF("enabled",enabled,1,0,1),PF("clockwise",clockwise,1,0,1),PF("type",type,2,0,3),
 PF("strength",strength,0,0,20),PF("lift",lift,0,-20,20),PF("falloff",falloff,0,0,1),
 PF("noise_scale",noiseScale,0,.05,20),PF("animation_speed",animationSpeed,0,0,10),
 PF("x",transform.translation.x,0,-1000000,1000000),PF("y",transform.translation.y,0,-1000000,1000000),PF("z",transform.translation.z,0,-1000000,1000000),
 PF("width",transform.scale.x,0,.01,10000),PF("height",transform.scale.y,0,.01,10000),PF("depth",transform.scale.z,0,.01,10000),
 PF("qx",transform.rotation.x,0,-10000,10000),PF("qy",transform.rotation.y,0,-10000,10000),PF("qz",transform.rotation.z,0,-10000,10000),PF("qw",transform.rotation.w,0,-10000,10000)};
#undef PF
 return values;
}
static std::string ParticleString(lua_State* state,int at){
 size_t size=0;const char* s=lua_type(state,at)==LUA_TSTRING?lua_tolstring(state,at,&size):nullptr;return s?std::string(s,size):std::string{};
}
bool ResolveParticle(lua_State* state,int at,wi::ecs::Entity& e,std::string& error){
 if(!scene||!particles){error="Particle API is not bound to an active scripted Level.";return false;}
 RuntimeScriptEntityReference ref;if(!ReadEntityReferenceForApi(state,at,ref,error))return false;
 RuntimeScriptEntityApi api(*scene,entitiesById,generation);return api.Resolve(ref,e,error);
}
bool ParticleIndex(lua_State* state,int at,wi::ecs::Entity e,bool field,size_t& index,std::string& error){
 std::vector<std::string> names;if(!(field?particles->Fields(e,names,error):particles->Layers(e,names,error)))return false;
 if(lua_type(state,at)==LUA_TNUMBER){auto value=lua_tonumber(state,at);if(std::isfinite(value)&&std::floor(value)==value&&value>=1&&value<=double(names.size())){index=size_t(value-1);return true;}}
 else if(lua_type(state,at)==LUA_TSTRING){
  auto name=ParticleString(state,at);bool found=false;
  for(size_t n=0;n<names.size();++n)if(names[n]==name){if(found){error="Particle name is ambiguous; use its 1-based index.";return false;}index=n;found=true;}
  if(found)return true;
 }
 error=field?"Unknown movement field name or index.":"Unknown particle layer name or index.";return false;
}
template<class T,class V> static bool ParticlePatch(lua_State* state,int at,T& candidate,const V& values,std::string& error){
 if(!lua_istable(state,at)){error="Particle settings must be a table.";return false;}at=lua_absindex(state,at);
 lua_pushnil(state);
 while(lua_next(state,at)){
  if(lua_type(state,-2)!=LUA_TSTRING){lua_pop(state,2);error="Particle setting names must be strings.";return false;}
  auto key=ParticleString(state,-2);
  if(key=="name"){
   auto text=ParticleString(state,-1);if(text.empty()||text.size()>120||text.find('\0')!=std::string::npos){lua_pop(state,2);error="Particle name must contain 1-120 valid bytes.";return false;}candidate.name=text;
  }else{
   auto found=std::find_if(values.begin(),values.end(),[&](const auto& p){return key==p.name;});
   if(found==values.end()){lua_pop(state,2);error="Unknown particle setting: "+key;return false;}
   double value=0;
   if(found->kind==1){if(lua_type(state,-1)!=LUA_TBOOLEAN){lua_pop(state,2);error="Particle flag must be boolean: "+key;return false;}value=lua_toboolean(state,-1);}
   else {if(lua_type(state,-1)!=LUA_TNUMBER){lua_pop(state,2);error="Particle setting must be numeric: "+key;return false;}value=lua_tonumber(state,-1);}
   if(!std::isfinite(value)||value<found->low||value>found->high||(found->kind==2&&std::floor(value)!=value)){lua_pop(state,2);error="Particle setting is outside its valid range: "+key;return false;}
   found->set(candidate,value);
  }
  lua_pop(state,1);
 }
 return true;
}
template<class T,class V> static void PushParticleSettings(lua_State* state,const T& value,const V& values){
 lua_newtable(state);lua_pushlstring(state,value.name.data(),value.name.size());lua_setfield(state,-2,"name");
 for(const auto& p:values){if(p.kind==1)lua_pushboolean(state,p.get(value)!=0);else lua_pushnumber(state,p.get(value));lua_setfield(state,-2,p.name);}
}
static int ParticlePresetsLua(lua_State* state){
 auto* o=FromUpvalue(state);if(!o||!o->particles)return PushNilError(state,"Particle catalog is unavailable.");
 std::string error;const auto& list=o->particles->Presets(error);if(!error.empty())return PushNilError(state,error);
 lua_createtable(state,int(list.size()),0);int n=1;
 for(const auto& p:list){lua_newtable(state);lua_pushlstring(state,p.id.data(),p.id.size());lua_setfield(state,-2,"id");lua_pushlstring(state,p.name.data(),p.name.size());lua_setfield(state,-2,"name");lua_rawseti(state,-2,n++);}return 1;
}
static int ParticleSpawnLua(lua_State* state){
 auto* o=FromUpvalue(state);if(!o||!o->particles)return PushNilError(state,"Particle API is unavailable.");
 auto name=ParticleString(state,1);if(name.empty()||name.size()>120||name.find('\0')!=std::string::npos)return PushNilError(state,"Pass a project preset name or ID.");
 XMFLOAT3 position={};std::string error;if(!lua_isnoneornil(state,2)&&!ReadVector3Table(state,2,position,error))return PushNilError(state,error);
 auto e=o->particles->Spawn(name,position,error);if(e==wi::ecs::INVALID_ENTITY)return PushNilError(state,error);
 auto id=bridge::PersistentEntityId(*o->scene,e);o->entitiesById[id]=e;o->PushEntityRef(id);return 1;
}
static int ParticleNamesLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;
 if(!o||!o->ResolveParticle(state,1,e,error))return PushNilError(state,error);
 bool fields=lua_toboolean(state,lua_upvalueindex(2))!=0;std::vector<std::string> names;
 if(!(fields?o->particles->Fields(e,names,error):o->particles->Layers(e,names,error)))return PushNilError(state,error);
 lua_createtable(state,int(names.size()),0);for(size_t n=0;n<names.size();++n){lua_pushlstring(state,names[n].data(),names[n].size());lua_rawseti(state,-2,n+1);}return 1;
}
static int ParticleLayerLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;size_t n=0;bool set=lua_toboolean(state,lua_upvalueindex(2))!=0;
 if(!o||!o->ResolveParticle(state,1,e,error)||!o->ParticleIndex(state,2,e,false,n,error))return set?PushFalseError(state,error):PushNilError(state,error);
 bridge::ParticleEffectLayer l;if(!o->particles->GetLayer(e,n,l,error))return set?PushFalseError(state,error):PushNilError(state,error);
 if(!set){PushParticleSettings(state,l,ParticleLayerValues());return 1;}
 bool oldVolume=l.emitter.volume;
 if(!ParticlePatch(state,3,l,ParticleLayerValues(),error))return PushFalseError(state,error);
 lua_pushliteral(state,"shape_type");lua_rawget(state,3);bool explicitShape=!lua_isnil(state,-1);lua_pop(state,1);
 if(explicitShape)l.emitter.volume=l.shape.type==bridge::ParticleShapeType::Box;
 else if(oldVolume!=l.emitter.volume)l.shape.type=l.emitter.volume?bridge::ParticleShapeType::Box:bridge::ParticleShapeType::Point;
 if(l.emitter.frameCount>l.emitter.framesX*l.emitter.framesY||l.emitter.frameStart>=l.emitter.frameCount){return PushFalseError(state,"Frame count/start must fit the spritesheet.");}
 if(!o->particles->SetLayer(e,n,l,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleFieldLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;size_t n=0;bool set=lua_toboolean(state,lua_upvalueindex(2))!=0;
 if(!o||!o->ResolveParticle(state,1,e,error)||!o->ParticleIndex(state,2,e,true,n,error))return set?PushFalseError(state,error):PushNilError(state,error);
 bridge::ParticleMovementField f;if(!o->particles->GetField(e,n,f,error))return set?PushFalseError(state,error):PushNilError(state,error);
 if(!set){PushParticleSettings(state,f,ParticleFieldValues());return 1;}
 if(!ParticlePatch(state,3,f,ParticleFieldValues(),error)||!o->particles->SetField(e,n,f,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleControlLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;if(!o||!o->ResolveParticle(state,1,e,error))return PushFalseError(state,error);
 auto op=ParticleString(state,lua_upvalueindex(2));bool clear=false;
 if(op=="stop"&&!lua_isnoneornil(state,2)){if(lua_type(state,2)!=LUA_TBOOLEAN)return PushFalseError(state,"stop clear flag must be boolean.");clear=lua_toboolean(state,2)!=0;}
 if(!o->particles->Control(e,op,clear,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleBurstLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;size_t n=0;
 if(!o||!o->ResolveParticle(state,1,e,error))return PushFalseError(state,error);
 double count=lua_type(state,2)==LUA_TNUMBER?lua_tonumber(state,2):0;
 if(!std::isfinite(count)||std::floor(count)!=count||count<1||count>1000000)return PushFalseError(state,"Burst count must be an integer between 1 and 1000000.");
 int layer=-1;if(!lua_isnoneornil(state,3)){if(!o->ParticleIndex(state,3,e,false,n,error))return PushFalseError(state,error);layer=int(n);}
 if(!o->particles->Burst(e,int(count),layer,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleDestroyLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;if(!o||!o->ResolveParticle(state,1,e,error))return PushFalseError(state,error);
 auto id=bridge::PersistentEntityId(*o->scene,e);if(!o->particles->Destroy(e,error))return PushFalseError(state,error);o->entitiesById.erase(id);lua_pushboolean(state,1);return 1;
}
static int ParticleAttachLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e,parent;
 if(!o||!o->ResolveParticle(state,1,e,error)||!o->ResolveParticle(state,2,parent,error))return PushFalseError(state,error);
 auto socket=ParticleString(state,3);if(!lua_isnoneornil(state,3)&&lua_type(state,3)!=LUA_TSTRING)return PushFalseError(state,"Socket name must be a string.");
 if(!lua_isnoneornil(state,4)&&lua_type(state,4)!=LUA_TBOOLEAN)return PushFalseError(state,"keep_world must be boolean.");
 if(!o->particles->Attach(e,parent,socket,lua_toboolean(state,4)!=0,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleDetachLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;if(!o||!o->ResolveParticle(state,1,e,error)||!o->particles->Detach(e,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleTransformLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;bool set=lua_toboolean(state,lua_upvalueindex(2))!=0;bridge::TransformState t;
 if(!o||!o->ResolveParticle(state,1,e,error)||!o->particles->GetTransform(e,t,error))return set?PushFalseError(state,error):PushNilError(state,error);
 const char* keys[]={"x","y","z","qx","qy","qz","qw","scale_x","scale_y","scale_z"};
 float* values[]={&t.translation.x,&t.translation.y,&t.translation.z,&t.rotation.x,&t.rotation.y,&t.rotation.z,&t.rotation.w,&t.scale.x,&t.scale.y,&t.scale.z};
 if(!set){lua_newtable(state);for(int n=0;n<10;++n){lua_pushnumber(state,*values[n]);lua_setfield(state,-2,keys[n]);}return 1;}
 if(!lua_istable(state,2))return PushFalseError(state,"Transform must be a settings table.");
 lua_pushnil(state);while(lua_next(state,2)){
  auto key=ParticleString(state,-2);int n=0;while(n<10&&key!=keys[n])++n;
  if(n==10||lua_type(state,-1)!=LUA_TNUMBER||!std::isfinite(lua_tonumber(state,-1))){lua_pop(state,2);return PushFalseError(state,"Unknown or nonnumeric particle transform field.");}
  *values[n]=float(lua_tonumber(state,-1));lua_pop(state,1);
 }
 if(!o->particles->SetTransform(e,t,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleTextureLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;size_t n=0;
 if(!o||!o->ResolveParticle(state,1,e,error)||!o->ParticleIndex(state,2,e,false,n,error))return PushFalseError(state,error);
 const char* slots[]={"colour","normal","surface","emissive","occlusion"};auto slotName=ParticleString(state,3);size_t slot=0;while(slot<5&&slotName!=slots[slot])++slot;
 if(slot==5)return PushFalseError(state,"Texture slot must be colour, normal, surface, emissive or occlusion.");
 auto id=ParticleString(state,4);if(auto* asset=static_cast<ResourceRefPayload*>(luaL_testudata(state,4,ResourceRefMetatable))){if(asset->kind!=ResourceKind::Asset)return PushFalseError(state,"Use an AssetRef for a texture.");id=asset->stableId;}
 if(!o->particles->SetTexture(e,n,slot,id,error))return PushFalseError(state,error);lua_pushboolean(state,1);return 1;
}
static int ParticleLifetimeLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;size_t n=0;bool set=lua_toboolean(state,lua_upvalueindex(2))!=0;
 auto fail=[&](const std::string& message){return set?PushFalseError(state,message):PushNilError(state,message);};
 if(!o||!o->ResolveParticle(state,1,e,error)||!o->ParticleIndex(state,2,e,false,n,error))return fail(error);
 bridge::ParticleEffectLayer layer;if(!o->particles->GetLayer(e,n,layer,error))return fail(error);
 if(!set){
  lua_newtable(state);lua_pushboolean(state,layer.lifetime.enabled);lua_setfield(state,-2,"enabled");
  lua_createtable(state,int(layer.lifetime.keys.size()),0);
  for(size_t i=0;i<layer.lifetime.keys.size();++i){
   const auto& k=layer.lifetime.keys[i];lua_newtable(state);
   const char* names[]={"time","size","red","green","blue","opacity"};const float values[]={k.time,k.size,k.colour.x,k.colour.y,k.colour.z,k.colour.w};
   for(int j=0;j<6;++j){lua_pushnumber(state,values[j]);lua_setfield(state,-2,names[j]);}
   lua_rawseti(state,-2,i+1);
  }
  lua_setfield(state,-2,"keys");return 1;
 }
 if(!lua_istable(state,3))return fail("Lifetime settings must be a table.");
 auto candidate=layer.lifetime;
 lua_pushnil(state);
 while(lua_next(state,3)){
  auto key=ParticleString(state,-2);
  if(key=="enabled"){
   if(lua_type(state,-1)!=LUA_TBOOLEAN){lua_pop(state,2);return fail("Lifetime enabled must be boolean.");}
   candidate.enabled=lua_toboolean(state,-1)!=0;
  }else if(key=="keys"){
   if(!lua_istable(state,-1)){lua_pop(state,2);return fail("Lifetime keys must be an array.");}
   int list=lua_absindex(state,-1);size_t count=lua_rawlen(state,list),entries=0;
   if(count<2||count>8){lua_pop(state,2);return fail("Lifetime curves need 2-8 keys.");}
   lua_pushnil(state);while(lua_next(state,list)){
    bool valid=lua_isinteger(state,-2)&&lua_tointeger(state,-2)>=1&&lua_tointeger(state,-2)<=lua_Integer(count);
    ++entries;lua_pop(state,1);if(!valid){lua_pop(state,3);return fail("Lifetime keys must be a dense 1-based array.");}
   }
   if(entries!=count){lua_pop(state,2);return fail("Lifetime key array has holes.");}
   std::vector<bridge::ParticleLifetimeKey> keys;keys.reserve(count);
   for(size_t i=0;i<count;++i){
    lua_rawgeti(state,list,i+1);int point=lua_absindex(state,-1);
    if(!lua_istable(state,point)){lua_pop(state,3);return fail("Each lifetime key must be a table.");}
    bridge::ParticleLifetimeKey k;bool hasTime=false;
    lua_pushnil(state);while(lua_next(state,point)){
     auto name=ParticleString(state,-2);
     const char* names[]={"time","size","red","green","blue","opacity"};float* values[]={&k.time,&k.size,&k.colour.x,&k.colour.y,&k.colour.z,&k.colour.w};
     int j=0;while(j<6&&name!=names[j])++j;
     if(j==6||lua_type(state,-1)!=LUA_TNUMBER||!std::isfinite(lua_tonumber(state,-1))){lua_pop(state,5);return fail("Unknown or nonnumeric lifetime key setting.");}
     *values[j]=float(lua_tonumber(state,-1));hasTime|=j==0;lua_pop(state,1);
    }
    lua_pop(state,1);if(!hasTime){lua_pop(state,2);return fail("Each lifetime key needs time.");}
    keys.push_back(k);
   }
   candidate.keys=std::move(keys);
  }else {lua_pop(state,2);return fail("Unknown lifetime setting: "+key);}
  lua_pop(state,1);
 }
 if(!bridge::ValidateParticleLifetime(candidate,error))return fail(error);
 layer.lifetime=std::move(candidate);
 if(!o->particles->SetLayer(e,n,layer,error))return fail(error);
 lua_pushboolean(state,1);return 1;
}
static int ParticleTimingLua(lua_State* state){
 auto* o=FromUpvalue(state);std::string error;wi::ecs::Entity e;bool set=lua_toboolean(state,lua_upvalueindex(2))!=0;
 auto fail=[&](const std::string& message){return set?PushFalseError(state,message):PushNilError(state,message);};
 bridge::ParticleEffectTiming t;if(!o||!o->ResolveParticle(state,1,e,error)||!o->particles->GetTiming(e,t,error))return fail(error);
 if(!set){lua_newtable(state);lua_pushboolean(state,t.enabled);lua_setfield(state,-2,"enabled");lua_pushboolean(state,t.loop);lua_setfield(state,-2,"loop");lua_pushnumber(state,t.duration);lua_setfield(state,-2,"duration");lua_pushnumber(state,t.delay);lua_setfield(state,-2,"delay");lua_pushinteger(state,t.burst);lua_setfield(state,-2,"burst");return 1;}
 if(!lua_istable(state,2))return fail("Timing settings must be a table.");
 lua_pushnil(state);while(lua_next(state,2)){
  auto key=ParticleString(state,-2);bool valid=true;
  if(key=="enabled"||key=="loop"){valid=lua_type(state,-1)==LUA_TBOOLEAN;if(valid){if(key=="enabled")t.enabled=lua_toboolean(state,-1)!=0;else t.loop=lua_toboolean(state,-1)!=0;}}
  else if(key=="duration"||key=="delay"||key=="burst"){
   valid=lua_type(state,-1)==LUA_TNUMBER;double value=valid?lua_tonumber(state,-1):0;valid=valid&&std::isfinite(value);
   if(key=="burst"){valid=valid&&value>=0&&value<=1000000&&std::floor(value)==value;if(valid)t.burst=uint32_t(value);}
   else if(valid){if(key=="duration")t.duration=float(value);else t.delay=float(value);}
  }else valid=false;
  lua_pop(state,1);if(!valid){lua_pop(state,1);return fail("Unknown or invalid timing setting: "+key);}
 }
 if(!o->particles->SetTiming(e,t,error))return fail(error);lua_pushboolean(state,1);return 1;
}
void RegisterParticleLua(){
 lua_newtable(lua);lua_pushinteger(lua,1);lua_setfield(lua,-2,"contract_version");
 const auto fn=[this](const char* name,lua_CFunction f){lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,f,1);lua_setfield(lua,-2,name);};
 const auto mode=[this](const char* name,lua_CFunction f,bool value){lua_pushlightuserdata(lua,this);lua_pushboolean(lua,value);lua_pushcclosure(lua,f,2);lua_setfield(lua,-2,name);};
 fn("presets",ParticlePresetsLua);fn("spawn",ParticleSpawnLua);fn("burst",ParticleBurstLua);fn("destroy",ParticleDestroyLua);fn("attach",ParticleAttachLua);fn("detach",ParticleDetachLua);fn("set_texture",ParticleTextureLua);
 mode("layers",ParticleNamesLua,false);mode("fields",ParticleNamesLua,true);mode("get_layer",ParticleLayerLua,false);mode("set_layer",ParticleLayerLua,true);
 mode("get_timing",ParticleTimingLua,false);mode("set_timing",ParticleTimingLua,true);
 mode("get_lifetime",ParticleLifetimeLua,false);mode("set_lifetime",ParticleLifetimeLua,true);
 mode("get_field",ParticleFieldLua,false);mode("set_field",ParticleFieldLua,true);mode("get_transform",ParticleTransformLua,false);mode("set_transform",ParticleTransformLua,true);
 for(const char* name:{"play","pause","resume","restart","stop"}){lua_pushlightuserdata(lua,this);lua_pushstring(lua,name);lua_pushcclosure(lua,ParticleControlLua,2);lua_setfield(lua,-2,name);}
 lua_setfield(lua,-2,"particles");
}
