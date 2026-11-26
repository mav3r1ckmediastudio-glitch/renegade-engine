// Unity ModelImporter stores the avatar's neutral local transforms alongside the FBX.
// Only adopt the sidecar when its hierarchy and limb offsets agree with this import.
#include <fstream>
#include <sstream>

static bool LoadAvatarReference(wi::scene::Scene& scene, const fs::path& fbx)
{
    auto sidecar = fbx;
    sidecar += ".meta";
    std::ifstream input(sidecar);
    if (!input) return false;

    struct Joint {
        std::string parent;
        XMFLOAT3 position{}, scale{};
        XMFLOAT4 rotation{};
        unsigned fields = 0;
    };
    std::map<std::string, Joint> joints;
    Joint* joint = nullptr;
    bool avatar = false, skeleton = false;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "  humanDescription:") { avatar = true; continue; }
        if (avatar && line == "    skeleton:") { skeleton = true; continue; }
        if (!skeleton) continue;
        if (line.compare(0, 12, "    - name: ") == 0) {
            auto entry = joints.try_emplace(line.substr(12));
            if (!entry.second) return false;
            joint = &entry.first->second;
            continue;
        }
        if (line.compare(0, 6, "      ") != 0) break;
        if (!joint) return false;
        if (line.compare(6, 12, "parentName: ") == 0) {
            joint->parent = line.substr(18);
            continue;
        }
        float x,y,z,w;
        if (std::sscanf(line.c_str(), " position: {x: %f, y: %f, z: %f}", &x,&y,&z) == 3) {
            joint->position = {-x,y,-z}; joint->fields |= 1;
        } else if (std::sscanf(line.c_str(), " rotation: {x: %f, y: %f, z: %f, w: %f}", &x,&y,&z,&w) == 4) {
            joint->rotation = {-x,y,-z,w}; joint->fields |= 2;
        } else if (std::sscanf(line.c_str(), " scale: {x: %f, y: %f, z: %f}", &x,&y,&z) == 3) {
            joint->scale = {x,y,z}; joint->fields |= 4;
        }
    }
    const auto& bones = scene.armatures[0].boneCollection;
    const std::set<wi::ecs::Entity> members(bones.begin(),bones.end());
    size_t comparable = 0, consistent = 0;
    for (auto bone : bones) {
        const auto* name = scene.names.GetComponent(bone);
        const auto* transform = scene.transforms.GetComponent(bone);
        if (!name || !transform) return false;
        auto it = joints.find(name->name);
        if (it == joints.end() || it->second.fields != 7) return false;
        const auto& ref = it->second;
        const auto p = XMLoadFloat3(&ref.position);
        const auto scale = XMLoadFloat3(&ref.scale);
        const float q = XMVectorGetX(XMVector4LengthSq(XMLoadFloat4(&ref.rotation)));
        if (XMVector3IsNaN(p) || XMVector3IsInfinite(p) || XMVector3IsNaN(scale) ||
            XMVector3IsInfinite(scale) || !(q > .98f && q < 1.02f)) return false;
        const auto* h = scene.hierarchy.GetComponent(bone);
        if (!h || !members.count(h->parentID)) continue;
        const auto* parent = scene.names.GetComponent(h->parentID);
        if (!parent || parent->name != ref.parent) return false;
        const auto original = XMLoadFloat3(&transform->translation_local);
        const float length = XMVectorGetX(XMVector3Length(original));
        if (length < .025f) continue;
        ++comparable;
        if (XMVectorGetX(XMVector3Length(original-p)) < .03f*length+.001f) ++consistent;
    }
    if (comparable < 4 || consistent*4 < comparable*3) return false;
    for (auto bone : bones) {
        const auto& ref = joints.at(scene.names.GetComponent(bone)->name);
        auto* t = scene.transforms.GetComponent(bone);
        t->translation_local = ref.position;
        t->rotation_local = ref.rotation;
        t->scale_local = ref.scale;
        t->SetDirty();
    }
    return true;
}
