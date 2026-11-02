// Adapted from pinned Wicked Editor ModelImporter_OBJ.cpp (MIT).
// Renegade adds bounded index validation and fills missing normals; upstream stays untouched.
#include <WickedEngine.h>
#include <filesystem>
#include <cmath>
#include <charconv>
#include <limits>
#include <sstream>
#include "renegade/bridge/ModelImporterFailureAdapter.h"
namespace wi::helper { void RenegadeImporterMessageBox(const std::string&, const std::string&); }
#include "wiScene.h"
#include "ModelImporter.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

using namespace wi::graphics;
using namespace wi::scene;
using namespace wi::ecs;

struct membuf : std::streambuf
{
	membuf(char* begin, char* end) {
		this->setg(begin, begin, end);
	}
};

// Custom material file reader:
class MaterialFileReader : public tinyobj::MaterialReader {
public:
	explicit MaterialFileReader(const std::string& mtl_basedir)
		: m_mtlBaseDir(mtl_basedir) {}
	virtual ~MaterialFileReader() {}
	virtual bool operator()(const std::string& matId,
		std::vector<tinyobj::material_t>* materials,
		std::map<std::string, int>* matMap, std::string* err)
	{
		std::string filepath;

		if (!m_mtlBaseDir.empty()) {
			filepath = std::string(m_mtlBaseDir) + matId;
		}
		else {
			filepath = matId;
		}

		wi::vector<uint8_t> filedata;
		if (!wi::helper::FileRead(filepath, filedata))
		{
			std::string ss;
			ss += "WARN: Material file [ " + filepath + " ] not found.\n";
			if (err) {
				(*err) += ss;
			}
			return false;
		}

		membuf sbuf((char*)filedata.data(), (char*)filedata.data() + filedata.size());
		std::istream matIStream(&sbuf);

		std::string warning;
        const auto firstMaterial = materials->size();
		LoadMtl(matMap, materials, &matIStream, &warning);
        const auto folder = std::filesystem::u8path(filepath).parent_path();
        for (size_t i = firstMaterial; i < materials->size(); ++i)
        {
            auto& mat = (*materials)[i];
            for (auto* name : {&mat.diffuse_texname, &mat.displacement_texname, &mat.normal_texname,
                &mat.bump_texname, &mat.specular_texname, &mat.specular_highlight_texname})
                if (!name->empty() && !std::filesystem::u8path(*name).is_absolute())
                    *name = (folder / std::filesystem::u8path(*name)).lexically_normal().generic_u8string();
        }

		if (!warning.empty()) {
			if (err) {
				(*err) += warning;
			}
		}

		return true;
	}

private:
	std::string m_mtlBaseDir;
};

// Transform the data from OBJ space to engine-space:
static const bool transform_to_LH = true;


struct index_and_mat
{
	tinyobj::index_t index;
	int materialIndex;
	constexpr bool operator==(const index_and_mat& other) const
	{
		return std::tuple(index.normal_index, index.texcoord_index, index.vertex_index, materialIndex)
			== std::tuple(other.index.normal_index, other.index.texcoord_index, other.index.vertex_index, other.materialIndex);
	}
};
template<>
struct std::hash<index_and_mat>
{
	constexpr std::size_t operator()(const index_and_mat& o) const noexcept
	{
		size_t vertexHash = 0;
		wi::helper::hash_combine(vertexHash, o.index.vertex_index);
		wi::helper::hash_combine(vertexHash, o.index.normal_index);
		wi::helper::hash_combine(vertexHash, o.index.texcoord_index);
		wi::helper::hash_combine(vertexHash, o.materialIndex);
		return vertexHash;
	}
};

namespace
{
    // tinyobj's polygon triangulation reads vertex positions before returning.
    // Validate position indices first, including relative indices at each face.
    bool ValidateObjPositions(const wi::vector<uint8_t>& bytes)
    {
        const std::string text(bytes.begin(), bytes.end());
        std::istringstream countStream(text);
        std::string line, keyword;
        size_t total = 0;
        while (std::getline(countStream, line))
        {
            std::istringstream fields(line);
            fields >> keyword;
            if (keyword == "v")
            {
                double x, y, z;
                if (!(fields >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                    return false;
                ++total;
            }
            keyword.clear();
        }
        if (total == 0 || total > size_t((std::numeric_limits<int>::max)())) return false;
        std::istringstream faces(text);
        size_t available = 0;
        while (std::getline(faces, line))
        {
            std::istringstream fields(line);
            fields >> keyword;
            if (keyword == "v") ++available;
            if (keyword == "f")
            {
                std::string token;
                size_t count = 0;
                while (fields >> token)
                {
                    if (token.front() == '#') break;
                    const auto slash = token.find('/');
                    const auto value = token.substr(0, slash);
                    int index = 0;
                    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), index);
                    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || index == 0 ||
                        (index > 0 && size_t(index) > total) ||
                        (index < 0 && -static_cast<long long>(index) > static_cast<long long>(available)))
                        return false;
                    ++count;
                }
                if (count < 3) return false;
            }
            keyword.clear();
        }
        return true;
    }
}

void ImportModel_OBJ(const std::string& fileName, Scene& scene)
{
	std::string directory = wi::helper::GetDirectoryFromPath(fileName);
	std::string name = wi::helper::GetFileNameFromPath(fileName);

	tinyobj::attrib_t obj_attrib;
	std::vector<tinyobj::shape_t> obj_shapes;
	std::vector<tinyobj::material_t> obj_materials;
	std::string obj_errors;

	wi::vector<uint8_t> filedata;
	bool success = wi::helper::FileRead(fileName, filedata);
    if (success && !ValidateObjPositions(filedata))
    { wi::helper::RenegadeImporterMessageBox("OBJ has invalid positions or face indices.", "Import Model"); return; }

	if (success)
	{
		membuf sbuf((char*)filedata.data(), (char*)filedata.data() + filedata.size());
		std::istream in(&sbuf);
		MaterialFileReader matFileReader(directory);
		success = tinyobj::LoadObj(&obj_attrib, &obj_shapes, &obj_materials, &obj_errors, &in, &matFileReader, true);
	}
	else
	{
		obj_errors = "Failed to read file: " + fileName;
	}

	if (!obj_errors.empty())
	{
		wi::backlog::post(obj_errors, wi::backlog::LogLevel::Error);
	}

    if (success)
    {
        for (const auto value : obj_attrib.vertices)
            if (!std::isfinite(value)) { success = false; break; }
        for (const auto& shape : obj_shapes)
        {
            if (shape.mesh.indices.size() % 3 != 0) { success = false; break; }
            for (const auto& index : shape.mesh.indices)
                if (index.vertex_index < 0 || size_t(index.vertex_index) >= obj_attrib.vertices.size() / 3)
                { success = false; break; }
        }
    }
	if (success)
	{
		Entity rootEntity = CreateEntity();
		scene.transforms.Create(rootEntity);
		scene.names.Create(rootEntity) = name;

		// Load material library:
		wi::vector<Entity> materialLibrary = {};
		for (auto& obj_material : obj_materials)
		{
			Entity materialEntity = scene.Entity_CreateMaterial(obj_material.name);
			scene.Component_Attach(materialEntity, rootEntity);
			MaterialComponent& material = *scene.materials.GetComponent(materialEntity);

			material.baseColor = XMFLOAT4(obj_material.diffuse[0], obj_material.diffuse[1], obj_material.diffuse[2], 1);
			material.textures[MaterialComponent::BASECOLORMAP].name = obj_material.diffuse_texname;
			material.textures[MaterialComponent::DISPLACEMENTMAP].name = obj_material.displacement_texname;
			material.emissiveColor.x = obj_material.emission[0];
			material.emissiveColor.y = obj_material.emission[1];
			material.emissiveColor.z = obj_material.emission[2];
			material.emissiveColor.w = std::max(obj_material.emission[0], std::max(obj_material.emission[1], obj_material.emission[2]));
			//material.refractionIndex = obj_material.ior;
			material.metalness = obj_material.metallic;
			material.textures[MaterialComponent::NORMALMAP].name = obj_material.normal_texname;
			material.textures[MaterialComponent::SURFACEMAP].name = obj_material.specular_texname;
			material.roughness = obj_material.roughness;

			if (material.textures[MaterialComponent::NORMALMAP].name.empty())
			{
				material.textures[MaterialComponent::NORMALMAP].name = obj_material.bump_texname;
			}
			if (material.textures[MaterialComponent::SURFACEMAP].name.empty())
			{
				material.textures[MaterialComponent::SURFACEMAP].name = obj_material.specular_highlight_texname;
			}

			for (auto& x : material.textures)
			{
				if (!x.name.empty() && !std::filesystem::u8path(x.name).is_absolute())
				{
					x.name = directory + x.name;
				}
			}

			material.CreateRenderData();

			materialLibrary.push_back(materialEntity); // for subset-indexing...
		}

		if (materialLibrary.empty())
		{
			// Create default material if nothing was found:
			Entity materialEntity = scene.Entity_CreateMaterial("OBJImport_defaultMaterial");
			scene.Component_Attach(materialEntity, rootEntity);
			MaterialComponent& material = *scene.materials.GetComponent(materialEntity);
			materialLibrary.push_back(materialEntity); // for subset-indexing...
		}

		// Load objects, meshes:
		for (auto& shape : obj_shapes)
		{
            if (shape.mesh.indices.empty()) continue;
            bool missingNormals = false;
			Entity objectEntity = scene.Entity_CreateObject(shape.name);
			scene.Component_Attach(objectEntity, rootEntity);
			Entity meshEntity = scene.Entity_CreateMesh(shape.name + "_mesh");
			scene.Component_Attach(meshEntity, rootEntity);
			ObjectComponent& object = *scene.objects.GetComponent(objectEntity);
			MeshComponent& mesh = *scene.meshes.GetComponent(meshEntity);

			object.meshID = meshEntity;

			int previousMaterialIndex = -1;
			wi::unordered_map<index_and_mat, uint32_t> uniqueVertices = {};

			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3)
			{
				tinyobj::index_t reordered_indices[] = {
					shape.mesh.indices[i + 0],
					shape.mesh.indices[i + 1],
					shape.mesh.indices[i + 2],
				};

				// todo: option param would be better
				bool flipCulling = false;
				if (flipCulling)
				{
					reordered_indices[1] = shape.mesh.indices[i + 2];
					reordered_indices[2] = shape.mesh.indices[i + 1];
				}

				for (auto& index : reordered_indices)
				{
					XMFLOAT3 pos = XMFLOAT3(
						obj_attrib.vertices[index.vertex_index * 3 + 0],
						obj_attrib.vertices[index.vertex_index * 3 + 1],
						obj_attrib.vertices[index.vertex_index * 3 + 2]
					);

					XMFLOAT3 nor = XMFLOAT3(0, 0, 0);
					if (index.normal_index >= 0 && size_t(index.normal_index) < obj_attrib.normals.size() / 3)
					{
						nor = XMFLOAT3(
							obj_attrib.normals[index.normal_index * 3 + 0],
							obj_attrib.normals[index.normal_index * 3 + 1],
							obj_attrib.normals[index.normal_index * 3 + 2]
						);
					}

					if (index.normal_index < 0 || size_t(index.normal_index) >= obj_attrib.normals.size() / 3) missingNormals = true;
					XMFLOAT2 tex = XMFLOAT2(0, 0);
					if (index.texcoord_index >= 0 && size_t(index.texcoord_index) < obj_attrib.texcoords.size() / 2)
					{
						tex = XMFLOAT2(
							obj_attrib.texcoords[index.texcoord_index * 2 + 0],
							1 - obj_attrib.texcoords[index.texcoord_index * 2 + 1]
						);
					}

					int materialIndex = std::clamp(i / 3 < shape.mesh.material_ids.size() ? shape.mesh.material_ids[i / 3] : 0, 0, int(materialLibrary.size()) - 1); // this indexes the material library
					if (materialIndex != previousMaterialIndex)
					{
						// Preserve each contiguous material run (including A -> B -> A).
						previousMaterialIndex = materialIndex;
						mesh.subsets.push_back(MeshComponent::MeshSubset());
						mesh.subsets.back().materialID = materialLibrary[materialIndex];
						mesh.subsets.back().indexOffset = (uint32_t)mesh.indices.size();
					}

					if (transform_to_LH)
					{
						pos.z *= -1;
						nor.z *= -1;
					}

					const index_and_mat im = {index, materialIndex};
					if (uniqueVertices.count(im) == 0)
					{
						uniqueVertices[im] = (uint32_t)mesh.vertex_positions.size();
						mesh.vertex_positions.push_back(pos);
						mesh.vertex_normals.push_back(nor);
						mesh.vertex_uvset_0.push_back(tex);
					}
					mesh.indices.push_back(uniqueVertices[im]);
					mesh.subsets.back().indexCount++;
				}
			}
			if (missingNormals) mesh.ComputeNormals(MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
            mesh.CreateRenderData();
		}

	}
	else
	{
		wi::helper::RenegadeImporterMessageBox("OBJ geometry could not be read. Check the model indices and faces.", "Import Model");
	}
}
