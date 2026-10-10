// Included by FirstPersonAssemblyService.cpp inside the bridge namespace.
static bool PrepareIndependentHandAssembly(const std::string& root,const StableId& project,
 const FirstPersonAssemblySettings& s,wi::scene::Scene& arms,wi::scene::Scene& weapon,
 wi::scene::Scene& result,std::string& error)
{
 auto off=ReusableAssetService().PrepareModelAssetPlacement({root,project,s.offHandWeaponAssetId});
 if(!off.IsReady()){error=off.Result().error;return false;}
 auto& shield=*off.PeekMutableScene();
 if(arms.armatures.GetCount()!=1 || weapon.armatures.GetCount()!=0 || shield.armatures.GetCount()!=0 ||
    weapon.animations.GetCount()!=0 || shield.animations.GetCount()!=0 ||
    weapon.objects.GetCount()==0 || shield.objects.GetCount()==0) {
  error="Independent hand assembly requires one arms armature and two static mesh products.";return false;
 }
 std::vector<PlayerViewBoneChoice> choices;
 if(!CollectPlayerViewBones(arms,choices,error))return false;
 auto resolve=[&](const std::string& path) {
  const auto found=std::find_if(choices.begin(),choices.end(),[&](const auto& c){return c.path==path;});
  return found==choices.end()?wi::ecs::INVALID_ENTITY:found->entity;
 };
 const auto primaryAnchor=resolve(s.parentBonePath),offAnchor=resolve(s.offHandParentBonePath);
 const auto primaryRoot=resolve(s.primaryLayerRootPath),offRoot=resolve(s.offHandLayerRootPath);
 PlayerViewBonePartition partition;
 if(primaryAnchor==wi::ecs::INVALID_ENTITY || offAnchor==wi::ecs::INVALID_ENTITY ||
    !CollectPlayerViewBonePartition(arms,arms.armatures.GetEntity(0),primaryRoot,offRoot,partition,error)) {
  if(error.empty())error="An explicit hand bone path is unavailable.";return false;
 }
 if(std::find(partition.primary.begin(),partition.primary.end(),primaryAnchor)==partition.primary.end() ||
    std::find(partition.offHand.begin(),partition.offHand.end(),offAnchor)==partition.offHand.end()) {
  error="Weapon anchors must belong to their authored hand subtrees.";return false;
 }
 for(const auto clip:{s.blockStartClip,s.blockLoopClip,s.blockEndClip})
  if(clip>=arms.animations.GetCount()){error="Shield clip index is unavailable.";return false;}
 for(const auto& pair:s.pairs)
  if(pair.armsClip>=arms.animations.GetCount()){error="Primary hand clip index is unavailable.";return false;}
 auto weaponRoots=Roots(weapon),shieldRoots=Roots(shield);
 if(weaponRoots.size()!=1 || shieldRoots.size()!=1){error="Each static hand product needs one transform root.";return false;}
 for(size_t i=0;i<arms.animations.GetCount();++i) {
  const auto entity=arms.animations.GetEntity(i);
  auto& clip=arms.animations[i];clip.Pause();clip.RootMotionOff();clip.amount=0;
  arms.metadatas.Create(entity).string_values.set(CreatorCharacterAnimationActionMetadataKey,"Unassigned");
 }
 unsigned attackOrder=0;
 for(const auto& pair:s.pairs) {
  auto& metadata=arms.metadatas.Create(arms.animations.GetEntity(pair.armsClip));
  metadata.string_values.set(CreatorCharacterAnimationActionMetadataKey,pair.action);
  if(pair.action=="Attack")metadata.int_values.set("renegade.first_person.attack_order",attackOrder++);
 }
 const unsigned blockIndices[]={s.blockStartClip,s.blockLoopClip,s.blockEndClip};
 const char* blockActions[]={"BlockStart","BlockLoop","BlockEnd"};
 for(size_t i=0;i<3;++i)
  arms.metadatas.Create(arms.animations.GetEntity(blockIndices[i])).string_values.set(
   CreatorCharacterAnimationActionMetadataKey,blockActions[i]);
 arms.metadatas.Create(primaryRoot).bool_values.set("renegade.first_person.primary_layer_root",true);
 arms.metadatas.Create(offRoot).bool_values.set("renegade.first_person.off_hand_layer_root",true);
 // This view-model has no procedural character controller or look-at authority.
 arms.humanoids.Clear();arms.characters.Clear();arms.rigidbodies.Clear();arms.springs.Clear();
 if(!AttachLaunchSockets(arms,s.launchSockets,LaunchSocketPart::Arms,error)||
    !AttachLaunchSockets(weapon,s.launchSockets,LaunchSocketPart::PrimaryWeapon,error)||
    !AttachLaunchSockets(shield,s.launchSockets,LaunchSocketPart::OffHandWeapon,error))return false;
 const auto armRoots=Roots(arms);
 const auto weaponRoot=weaponRoots.front(),shieldRoot=shieldRoots.front();
 arms.Merge(weapon);arms.Merge(shield);
 arms.Component_Attach(weaponRoot,primaryAnchor,true);
 arms.Component_Attach(shieldRoot,offAnchor,true);
 Set(*arms.transforms.GetComponent(weaponRoot),s.weaponPosition,s.weaponRotation);
 Set(*arms.transforms.GetComponent(shieldRoot),s.offHandWeaponPosition,s.offHandWeaponRotation);
 arms.transforms.GetComponent(weaponRoot)->scale_local={s.weaponScale,s.weaponScale,s.weaponScale};
 arms.transforms.GetComponent(shieldRoot)->scale_local={s.offHandWeaponScale,s.offHandWeaponScale,s.offHandWeaponScale};
 arms.transforms.GetComponent(weaponRoot)->SetDirty();arms.transforms.GetComponent(shieldRoot)->SetDirty();
 const auto viewRoot=arms.Entity_CreateTransform(CreatorAuthoredTransformRootName);
 Set(*arms.transforms.GetComponent(viewRoot),s.cameraPosition,s.cameraRotation);
 for(const auto entity:armRoots)arms.Component_Attach(entity,viewRoot,true);
 arms.metadatas.Create(viewRoot).bool_values.set("renegade.first_person.independent_hands",true);
 auto& timing=arms.metadatas.Create(viewRoot);
 timing.float_values.set("renegade.first_person.full_charge_seconds",s.fullChargeSeconds);
 timing.float_values.set("renegade.first_person.chain_window_seconds",s.chainWindowSeconds);
 timing.float_values.set("renegade.first_person.queued_release_seconds",s.queuedReleaseSeconds);
 if(s.avoidOffHand) {
  auto& m=arms.metadatas.Create(viewRoot);
  m.bool_values.set("renegade.first_person.avoid_off_hand",true);
  const auto vector=[&](const char* key,const XMFLOAT3& v) {
   m.float_values.set(std::string(key)+"_x",v.x);m.float_values.set(std::string(key)+"_y",v.y);m.float_values.set(std::string(key)+"_z",v.z);
  };
  vector("blade_base",s.bladeBase);vector("blade_tip",s.bladeTip);
  vector("shield_center",s.shieldCenter);vector("shield_half_extents",s.shieldHalfExtents);
  m.float_values.set("blade_radius",s.bladeRadius);m.float_values.set("maximum_correction",s.maximumHandCorrection);
  arms.metadatas.Create(weaponRoot).bool_values.set("renegade.first_person.blade_proxy",true);
  arms.metadatas.Create(shieldRoot).bool_values.set("renegade.first_person.shield_proxy",true);
 }

 for(size_t i=0;i<arms.metadatas.GetCount();++i) {
  arms.metadatas[i].bool_values.erase(CharacterAssetTemplateMetadataKey);
  arms.metadatas[i].int_values.erase(CharacterAssetTemplateVersionMetadataKey);
 }
 arms.Update(0);
 if(!FirstPersonAssemblyService().Pose(arms,"Idle",0,error))return false;
 result.Clear();result.Merge(arms);return true;
}
