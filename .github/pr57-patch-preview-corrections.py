from pathlib import Path
import re


def load(path):
    return Path(path).read_text(encoding="utf-8")


def save(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path, old, new):
    text = load(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one target, found {count}")
    save(path, text.replace(old, new, 1))


def regex_once(path, pattern, replacement):
    text = load(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{path}: expected one regex target, found {count}")
    save(path, updated)


path = "Studio/src/CreatorAssetDragPreview.cpp"
replace_once(path, "constexpr std::uint32_t DragPreviewLayer = 1u << 30;",
             "constexpr std::uint32_t DragPreviewLayer = 1u << 31;")

# Keep the preview out of both renderer visibility and pick acceleration data.
insert_after = r'''    bool IsLivePreviewWrapper(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity wrapper) noexcept
    {
        if (wrapper == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(wrapper))
        {
            return false;
        }
        const auto* name = scene.names.GetComponent(wrapper);
        return name != nullptr && name->name == DragPreviewWrapperName;
    }
'''
layer_helper = r'''

    void SetPreviewObjectLayer(
        wi::scene::Scene& scene,
        const wi::ecs::Entity wrapper,
        const std::uint32_t layerMask)
    {
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.objects.GetEntity(index);
            if (entity != wrapper && !scene.Entity_IsDescendant(entity, wrapper))
                continue;
            auto* layer = scene.layers.GetComponent(entity);
            if (layer == nullptr)
                layer = &scene.layers.Create(entity);
            layer->layerMask = layerMask;
            layer->propagationMask = layerMask;
            if (index < scene.aabb_objects.size())
                scene.aabb_objects[index].layerMask = layerMask;
        }
    }
'''
replace_once(path, insert_after, insert_after + layer_helper)

replace_once(
    path,
    """        for (const wi::ecs::Entity entity : createdEntities)
        {
            if (auto* layer = scene->layers.GetComponent(entity))
                layer->layerMask = 0u;
        }
""",
    """        SetPreviewObjectLayer(*scene, wrapper, 0u);
""",
)

# Thread-safe cleanup removes the exact remaining entities, including component
# types a model may add beyond the common render components.
regex_once(
    path,
    r"                scene->Entity_Remove\(wrapper, true\);\n                for \(const wi::ecs::Entity entity : createdEntities\)\n                \{.*?\n                \}\n                finish\(\);",
    """                scene->Entity_Remove(wrapper, true);
                wi::unordered_set<wi::ecs::Entity> remaining;
                scene->FindAllEntities(remaining);
                for (const wi::ecs::Entity entity : createdEntities)
                {
                    if (remaining.count(entity) != 0)
                        scene->Entity_Remove(entity, false);
                }
                finish();""",
)

# ViewportBounds stores left/top/right/bottom, not left/top/width/height.
replace_once(
    path,
    """        return pointer.x >= bounds.x && pointer.x <= bounds.x + bounds.z &&
            pointer.y >= bounds.y && pointer.y <= bounds.y + bounds.w;
""",
    """        return pointer.x >= bounds.x && pointer.x < bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.w;
""",
)

# The ghost wrapper must carry the same placement scale as the production
# instance wrapper or the cursor preview and committed object disagree.
replace_once(
    path,
    """        transform->translation_local = preview.position;
        transform->SetDirty();
""",
    """        transform->translation_local = preview.position;
        transform->scale_local = XMFLOAT3(
            preview.scale, preview.scale, preview.scale);
        transform->SetDirty();
""",
)

# Preserve all top-level roots when a reusable scene contains more than one.
regex_once(
    path,
    r"    wi::ecs::Entity FindRootEntity\(.*?\n    \}\n\n    bool PointerInsideViewport",
    r'''    std::vector<wi::ecs::Entity> FindRootEntities(
        const wi::scene::Scene& scene,
        const std::vector<wi::ecs::Entity>& created)
    {
        std::vector<wi::ecs::Entity> roots;
        const std::unordered_set<wi::ecs::Entity> createdSet(
            created.begin(), created.end());
        for (const wi::ecs::Entity entity : created)
        {
            if (!scene.transforms.Contains(entity))
                continue;
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                createdSet.find(hierarchy->parentID) == createdSet.end())
            {
                roots.push_back(entity);
            }
        }
        return roots;
    }

    bool PointerInsideViewport''',
)

old = r'''        auto created = CollectNewEntities(scene, before);
        const wi::ecs::Entity root = FindRootEntity(scene, created);
        if (root == wi::ecs::INVALID_ENTITY)
        {
            for (const wi::ecs::Entity entity : created)
            {
                if (!scene.hierarchy.Contains(entity))
                    scene.Entity_Remove(entity, true);
            }
            error = "The reusable asset cursor preview has no transform root.";
            return false;
        }

        const wi::ecs::Entity wrapper =
            scene.Entity_CreateTransform(DragPreviewWrapperName);
        auto* wrapperTransform = scene.transforms.GetComponent(wrapper);
        if (wrapperTransform == nullptr)
        {
            scene.Entity_Remove(root, true);
            error = "The reusable asset cursor preview root could not be created.";
            return false;
        }

        scene.Component_Attach(root, wrapper, true);
        created.push_back(wrapper);
        for (const wi::ecs::Entity entity : created)
        {
            if (!scene.layers.Contains(entity))
                scene.layers.Create(entity);
            if (auto* layer = scene.layers.GetComponent(entity))
                layer->layerMask = DragPreviewLayer;
        }
'''
new = r'''        auto created = CollectNewEntities(scene, before);
        const auto roots = FindRootEntities(scene, created);
        const wi::ecs::Entity wrapper =
            scene.Entity_CreateTransform(DragPreviewWrapperName);
        created.push_back(wrapper);
        auto* wrapperTransform = scene.transforms.GetComponent(wrapper);
        if (wrapperTransform == nullptr || roots.empty())
        {
            HideAndDeferPreviewRemoval(
                &scene, wrapper, std::move(created));
            error = roots.empty()
                ? "The reusable asset cursor preview has no transform root."
                : "The reusable asset cursor preview root could not be created.";
            return false;
        }

        for (const wi::ecs::Entity root : roots)
            scene.Component_Attach(root, wrapper, true);
        SetPreviewObjectLayer(scene, wrapper, DragPreviewLayer);
'''
replace_once(path, old, new)

text = load(path)
for required in (
    "DragPreviewLayer = 1u << 31",
    "SetPreviewObjectLayer(scene, wrapper, DragPreviewLayer)",
    "transform->scale_local = XMFLOAT3(",
    "FindRootEntities",
    "pointer.x < bounds.z",
):
    if required not in text:
        raise RuntimeError(f"preview correction missing: {required}")

print("PR57_PREVIEW_CORRECTIONS_APPLIED")
