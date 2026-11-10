
#ifndef RENEGADE_PARTICLE_FIELDS_HF
#define RENEGADE_PARTICLE_FIELDS_HF
void RenegadeApplyMovementFields(float3 position, inout float3 velocity, float dt, uint particleIndex)
{
 float3 flow=0, turbulence=0;float response=0;
 [loop] for(uint i=0;i<min(xMovementFieldCount.x,4u);++i) {
  RenegadeParticleFieldGPU f=xMovementFields[i];
  if(f.motion.x<=0 || (abs(f.axisX_strength.w)<.00001 && abs(f.motion.y)<.00001))continue;
  float4 p=float4(position,1);
  float3 local=float3(dot(p,f.toFieldX),dot(p,f.toFieldY),dot(p,f.toFieldZ));
  float edge=max(abs(local.x),max(abs(local.y),abs(local.z)));
  if(edge>=1)continue;
  float fade=f.axisZ_falloff.w<=0?1:smoothstep(0,1,saturate((1-edge)/f.axisZ_falloff.w));
  float weight=fade*f.motion.x;float3 v=0;
  uint type=(uint)f.axisY_type.w;
  if(type==0) {
   float radius=length(local.xz);
   if(radius>0.00001)v=float3(-local.z,0,local.x)/radius*min(radius/.2,1)*f.axisX_strength.w;
   v.y=f.motion.y;
  } else if(type==1) {
   float3 q=local*f.motion.z*3.14159265;float t=f.motion.w;
   float a=cos(q.y+1.7*q.z+t),b=cos(q.z+1.3*q.x-.8*t),c=cos(q.x+1.9*q.y+.6*t);
   v=float3(1.9*c-b,1.7*a-c,1.3*b-a)*.4*f.axisX_strength.w;
   v.y+=f.motion.y;
  } else if(type==2) v=float3(0,f.axisX_strength.w+f.motion.y,0);
  else v=float3(0,f.motion.y,f.axisX_strength.w);
  float3 world=f.axisX_strength.xyz*v.x+f.axisY_type.xyz*v.y+f.axisZ_falloff.xyz*v.z;
  if(type==1)turbulence+=world*weight;
  else {flow+=world*weight;response=max(response,weight);}
 }
 uint hash=particleIndex*747796405u+2891336453u;hash=((hash>>((hash>>28u)+4u))^hash)*277803737u;hash=(hash>>22u)^hash;
 flow*=lerp(1-xParticleVariation.y,1+xParticleVariation.y,(hash&0x00ffffffu)/16777215.0);
 if(response>0)velocity=lerp(velocity,flow,1-exp(-4*response*dt));
 velocity+=turbulence*dt;
}
#endif
