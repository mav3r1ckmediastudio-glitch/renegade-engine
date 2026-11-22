#include "renegade/bridge/GrassPalette.h"
#include "renegade/bridge/PayloadDigest.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#include <objbase.h>

namespace renegade::bridge
{
    namespace fs = std::filesystem;
    using namespace DirectX;
    namespace
    {
        constexpr auto Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        struct Apartment
        {
            HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            ~Apartment() { if (SUCCEEDED(status)) CoUninitialize(); }
        };
        bool ReadImage(const fs::path& source, ScratchImage& image, std::string& error)
        {
            ScratchImage decoded;
            if (FAILED(LoadFromWICFile(source.c_str(), WIC_FLAGS_IGNORE_SRGB, nullptr, decoded)))
            { error = "Unable to read " + source.filename().u8string(); return false; }
            if (decoded.GetMetadata().format == Format) image = std::move(decoded);
            else if (FAILED(Convert(*decoded.GetImage(0,0,0), Format, TEX_FILTER_DEFAULT, 0, image)))
            { error = "Unable to convert grass image to RGBA."; return false; }
            return true;
        }
        std::vector<uint8_t> Bytes(const Blob& blob)
        {
            const auto* begin=blob.GetBufferPointer();
            return {begin,begin+blob.GetBufferSize()};
        }

        bool Publish(const fs::path& project, std::vector<ProjectDocumentWrite> files, std::string& error)
        {
            ProjectDocumentTransactionOptions options;
            options.allowedRoot=project.generic_u8string();
            options.journalDirectory=(project/"Intermediate/Transactions").generic_u8string();
            const auto result=ProjectDocumentTransaction{}.Execute(std::move(files),options);
            if (!result.success) error=result.message;
            return result.success;
        }
    }
    bool ImportGrassSheet(const fs::path& project, const fs::path& source,
        unsigned columns, unsigned rows, std::string& error)
    {
        Apartment com;
        ScratchImage image;
        if (!ReadImage(source, image, error)) return false;
        const auto& base = *image.GetImage(0,0,0);
        if (!columns || !rows || columns > 64 || rows > 64 / columns ||
            base.width % columns || base.height % rows)
        { error = "Choose an evenly divided sheet with 1 to 64 cells."; return false; }
        const auto w = base.width / columns, h = base.height / rows;
        std::vector<ProjectDocumentWrite> files;
        for (unsigned y = 0; y < rows; ++y)
        for (unsigned x = 0; x < columns; ++x)
        {
            const auto* pixels = base.pixels + y * h * base.rowPitch + x * w * 4;
            ScratchImage tile;
            if (FAILED(tile.Initialize2D(Format,w,h,1,1))) { error="Unable to allocate grass tile."; return false; }
            for (size_t line=0; line<h; ++line)
                std::memcpy(tile.GetPixels()+line*tile.GetImage(0,0,0)->rowPitch,pixels+line*base.rowPitch,w*4);
            bool visible=false;
            for(size_t offset=3; offset<tile.GetPixelsSize(); offset+=4)
                if(tile.GetPixels()[offset]!=0) { visible=true; break; }
            if (!visible) continue;
            Blob png;
            if (FAILED(SaveToWICMemory(*tile.GetImage(0,0,0), WIC_FLAGS_NONE,
                GetWICCodec(WIC_CODEC_PNG), png))) { error = "Unable to encode foliage tile."; return false; }
            std::string id;
            auto bytes=Bytes(png);
            if (!ComputePayloadSha256(bytes,id,error)) return false;
            const auto file = source.stem().u8string() + "_" + std::to_string(1 + x + y*columns)
                + "_" + id.substr(0,16) + ".png";
            files.push_back({(project/CustomPlantDirectory/fs::u8path(file)).generic_u8string(),std::move(bytes)});
        }
        if (files.empty()) { error="The PNG contains no visible grass."; return false; }
        return Publish(project,std::move(files),error);
    }
    bool BuildGrassAtlas(const fs::path& project, const std::vector<fs::path>& selected,
        GrassAtlas& atlas, std::string& error)
    {
        if (selected.empty() || selected.size()>64) { error="Select between 1 and 64 grass thumbnails."; return false; }
        Apartment com;
        const unsigned side = static_cast<unsigned>(std::ceil(std::sqrt(double(selected.size()))));
        size_t tileSize=1;
        for (const auto& entry:selected)
        {
            TexMetadata metadata{};
            if (FAILED(GetMetadataFromWICFile(entry.c_str(),WIC_FLAGS_IGNORE_SRGB,metadata)))
            { error="Unable to read grass image dimensions."; return false; }
            tileSize=(std::max)({tileSize,metadata.width,metadata.height});
        }
        tileSize=(std::min)({tileSize,size_t(1024),size_t(4096/side)});
        ScratchImage sheet;
        if (FAILED(sheet.Initialize2D(Format, side*tileSize, side*tileSize, 1, 1)))
        { error="Unable to allocate grass atlas."; return false; }
        std::memset(sheet.GetPixels(), 0, sheet.GetPixelsSize());
        const auto pitch = sheet.GetImage(0,0,0)->rowPitch;
        for (size_t i=0; i<selected.size(); ++i)
        {
            ScratchImage image, scaled;
            if (!ReadImage(selected[i],image,error)) return false;
            auto* tile = image.GetImage(0,0,0);
            const double factor=double(tileSize)/(std::max)(tile->width,tile->height);
            const size_t width=(std::max)(size_t(1),size_t(std::lround(tile->width*factor)));
            const size_t height=(std::max)(size_t(1),size_t(std::lround(tile->height*factor)));
            if (tile->width != width || tile->height != height)
            {
                if (FAILED(Resize(*tile, width, height, TEX_FILTER_FANT, scaled)))
                { error="Unable to resize grass tile."; return false; }
                tile=scaled.GetImage(0,0,0);
            }
            const size_t x=(i%side)*tileSize+(tileSize-width)/2;
            const size_t y=(i/side)*tileSize+tileSize-height;
            for (size_t line=0;line<height;++line)
                std::memcpy(sheet.GetPixels()+(y+line)*pitch+x*4,tile->pixels+line*tile->rowPitch,width*4);
        }
        Blob png;
        if (FAILED(SaveToWICMemory(*sheet.GetImage(0,0,0),WIC_FLAGS_NONE,GetWICCodec(WIC_CODEC_PNG),png)))
        { error="Unable to encode grass atlas."; return false; }
        std::string id;
        auto bytes=Bytes(png);
        if (!ComputePayloadSha256(bytes,id,error)) return false;
        auto path=project/"Content/Vegetation/Mixes"/(id+".png");
        std::error_code ec;
        if (!fs::is_regular_file(path,ec))
        {
            std::vector<ProjectDocumentWrite> product;
            product.push_back({path.generic_u8string(),std::move(bytes)});
            if (!Publish(project,std::move(product),error)) return false;
        }
        atlas={path.generic_u8string(),static_cast<unsigned>(selected.size())};
        return true;
    }
}
