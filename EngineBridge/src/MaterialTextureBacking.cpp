#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include "renegade/bridge/MaterialTextureCook.h"

#ifdef _WIN32
#include "../../cmake/RenegadeTextureBacking.h"
#include <Windows.h>
#endif

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>

namespace renegade::bridge
{
    std::string MaterialTextureSemantic(MaterialTextureSlot slot,
        const wi::scene::MaterialComponent& material)
    {
        switch (slot)
        {
        case MaterialTextureSlot::Normal: return "normal";
        case MaterialTextureSlot::Surface:
        case MaterialTextureSlot::Occlusion: return "linear";
        case MaterialTextureSlot::Emissive: return "color";
        case MaterialTextureSlot::BaseColor:
            if (!material.IsAlphaTestEnabled())
                return "color";
            // Wicked uploads 1 - alphaRef as the shader's alpha-test threshold.
            return TextureRecipeName({TextureSemantic::Cutout, 1.0f - material.alphaRef});
        }
        return {};
    }

#ifdef _WIN32
    namespace
    {
        struct Generation
        {
            HANDLE file = INVALID_HANDLE_VALUE;
            mutable std::mutex mutex;

            ~Generation() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }

            bool Read(std::uint64_t position, size_t length, uint8_t* out) const
            {
                std::lock_guard<std::mutex> lock(mutex);
                LARGE_INTEGER offset{};
                offset.QuadPart = static_cast<LONGLONG>(position);
                if (!SetFilePointerEx(file, offset, nullptr, FILE_BEGIN))
                    return false;
                while (length)
                {
                    const DWORD count = static_cast<DWORD>((std::min)(length, size_t(1u << 20)));
                    DWORD actual = 0;
                    if (!ReadFile(file, out, count, &actual, nullptr) || actual != count)
                        return false;
                    length -= actual;
                    out += actual;
                }
                return true;
            }
        };

        struct TextureSpan final : wi::resourcemanager::GovernedTextureBacking
        {
            std::shared_ptr<Generation> generation;
            size_t offset = 0;
            size_t length = 0;
            std::string cacheName;
            mutable std::atomic<std::uint64_t> initialBytes{0}, mipBytes{0}, mipReads{0};

            bool Read(size_t relative, size_t count, wi::vector<uint8_t>& out) const override
            {
                if (relative > length || count > length - relative)
                    return false;
                out.resize(count);
                if (generation->Read(offset + relative, count, out.data()))
                {
                    mipReads.fetch_add(1, std::memory_order_relaxed);
                    mipBytes.fetch_add(count, std::memory_order_relaxed);
                    return true;
                }
                out.clear();
                return false;
            }
        };
    }

    struct PreparedTextureBacking
    {
        std::string container;
        std::string scope;
        std::map<std::string, std::shared_ptr<TextureSpan>> variants;
    };
#endif

    bool PrepareTextureBacking(const std::string& container, const ResourceAssetPayloadSpan& source,
        const ResourceAssetDocument& document, std::shared_ptr<const PreparedTextureBacking>& backing,
        std::string& error)
    {
        backing.reset();
#ifdef _WIN32
        if (document.manifest.textureVariants.empty())
            return true;
        auto generation = std::make_shared<Generation>();
        generation->file = CreateFileW(std::filesystem::u8path(container).c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        LARGE_INTEGER size{};
        if (generation->file == INVALID_HANDLE_VALUE || !GetFileSizeEx(generation->file, &size) ||
            size.QuadPart < 0 || source.offset > static_cast<std::uint64_t>(size.QuadPart) ||
            source.length > static_cast<std::uint64_t>(size.QuadPart) - source.offset)
        {
            error = "Could not pin the governed texture generation: " + container;
            return false;
        }
        auto result = std::make_shared<PreparedTextureBacking>();
        result->container = container;
        const auto location = std::filesystem::u8path(container).lexically_normal().generic_u8string();
        if (!ComputePayloadSha256({location.begin(), location.end()}, result->scope, error))
            return false;
        std::uint64_t offset = source.offset + source.length;
        for (size_t i = 0; i < document.manifest.textureVariants.size(); ++i)
        {
            const auto& variant = document.manifest.textureVariants[i];
            if (!variant.length || variant.length > static_cast<std::uint64_t>(size.QuadPart) - offset)
            {
                error = "Governed texture backing span is outside the pinned file.";
                return false;
            }
            auto span = std::make_shared<TextureSpan>();
            span->generation = generation;
            span->offset = static_cast<size_t>(offset);
            span->length = static_cast<size_t>(variant.length);
            const std::string prefix = "renegade_t1_stream_v1_" +
                document.manifest.projectId + "_" + result->scope + "_" + variant.semantic + "_";
            span->cacheName = prefix + document.manifest.assetId + "_sha256_" + variant.hash + ".dds";
            result->variants.emplace(variant.semantic, std::move(span));
            offset += variant.length;
        }
        if (offset != static_cast<std::uint64_t>(size.QuadPart))
        {
            error = "Governed texture generation changed during preparation.";
            return false;
        }
        backing = std::move(result);
#endif
        return true;
    }

    MaterialTextureReadStats CaptureMaterialTextureReads(
        const PreparedMaterialTextureAsset& prepared, const std::string& semantic)
    {
#ifdef _WIN32
        if (prepared.backing)
        {
            const auto found = prepared.backing->variants.find(semantic);
            if (found != prepared.backing->variants.end())
            {
                const auto& s = *found->second;
                return {s.initialBytes.load(), s.mipBytes.load(), s.mipReads.load()};
            }
        }
#endif
        return {};
    }

    wi::Resource LoadMaterialTextureForSemantic(const PreparedMaterialTextureAsset& prepared,
        const std::string& semantic, std::string& error)
    {
#ifdef _WIN32
        if (prepared.backing && !semantic.empty())
        {
            auto found = prepared.backing->variants.find(semantic);
            if (found != prepared.backing->variants.end())
            {
                const auto& span = found->second;
                std::vector<uint8_t> initial(span->length);
                if (!span->generation->Read(span->offset, span->length, initial.data()))
                {
                    error = "Could not read the pinned DDS payload.";
                    return {};
                }
                span->initialBytes.fetch_add(initial.size(), std::memory_order_relaxed);
                const std::string prefix = "renegade_t1_stream_v1_" + prepared.projectId +
                    "_" + prepared.backing->scope + "_" + found->first + "_";
                std::string verified;
                if (!BuildResourcePayloadCacheName(prefix, prepared.assetId, ResourceSourceFormat::Dds,
                        initial, verified, error) || verified != span->cacheName)
                {
                    error = "Pinned DDS payload differs from the validated version.";
                    return {};
                }
                MaterialTextureRecipe recipe;
                if (!ParseTextureRecipe(semantic, recipe) ||
                    !ValidateMaterialTextureProduct(initial, recipe.semantic, error))
                    return {};
                auto resource = wi::resourcemanager::LoadGovernedTexture(verified, initial.data(),
                    initial.size(), prepared.backing->container, span->offset, span);
                if (!resource.IsValid() || !resource.GetTexture().IsValid())
                {
                    error = "Wicked could not load the governed DDS span.";
                    return {};
                }
                error.clear();
                return resource;
            }
        }
#endif
        return LoadPreparedMaterialTextureAsset(prepared, error);
    }
}
