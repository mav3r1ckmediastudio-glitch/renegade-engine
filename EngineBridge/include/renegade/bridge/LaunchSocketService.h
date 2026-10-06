#pragma once
#include <WickedEngine.h>
#include <string>
#include <vector>
#include "renegade/bridge/PlayerViewGripService.h"
namespace renegade::bridge {
enum class LaunchSocketPart { PrimaryWeapon, OffHandWeapon, Arms };
struct LaunchSocketDefinition {
    std::string name="Muzzle";
    LaunchSocketPart part=LaunchSocketPart::PrimaryWeapon;
    // Empty means the product's single authored transform root.
    std::string parentPath;
    XMFLOAT3 position={}, rotationDegrees={};
};
inline constexpr const char* LaunchSocketNameKey="renegade.launch_socket.name";
bool ValidateLaunchSockets(const std::vector<LaunchSocketDefinition>&,std::string&);
bool SerializeLaunchSockets(const std::vector<LaunchSocketDefinition>&,std::string&,std::string&);
bool ParseLaunchSockets(const std::string&,std::vector<LaunchSocketDefinition>&,std::string&);
std::vector<PlayerViewBoneChoice> CollectLaunchSocketParents(const wi::scene::Scene&);
wi::ecs::Entity ResolveLaunchSocketParent(const wi::scene::Scene&,const std::string&);
bool AttachLaunchSockets(wi::scene::Scene&,const std::vector<LaunchSocketDefinition>&,
    LaunchSocketPart,std::string&);
bool ReadLaunchSocketPose(const wi::scene::Scene&,wi::ecs::Entity root,
    const std::string& name,XMFLOAT3& position,XMFLOAT3& direction,std::string&);
bool LaunchSocketSurfacePoint(const wi::scene::Scene&,const wi::primitive::Ray&,
    const std::string& parentPath,XMFLOAT3&,std::string&);
bool LaunchSocketInspectionPose(const wi::scene::Scene&,const LaunchSocketDefinition&,
    XMFLOAT3&,XMFLOAT3&,std::string&);
}
