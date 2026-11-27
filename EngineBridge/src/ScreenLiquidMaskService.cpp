#include "renegade/bridge/ScreenLiquidMaskService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <vector>
#include <limits>
#ifdef _WIN32
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib,"windowscodecs.lib")
#endif
namespace renegade::bridge
{
namespace
{
    bool InspectFile(const std::string& file,ScreenLiquid& next,std::string& error)
    {
#ifdef _WIN32
        using Microsoft::WRL::ComPtr;
        const auto apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        struct Apartment{bool owns;~Apartment(){if(owns)CoUninitialize();}} apartmentOwner{SUCCEEDED(apartment)};
        if(FAILED(apartment)&&apartment!=RPC_E_CHANGED_MODE){error="Cannot initialize mask image reader.";return false;}
        ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
        UINT width=0,height=0;
        const auto path=std::filesystem::u8path(file);
        if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())))||
            FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||
            FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&width,&height))||!width||!height||
            width>4096||height>4096||FAILED(factory->CreateFormatConverter(&converter))||
            FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))
        {error="Choose a readable PNG, BMP or JPEG mask up to 4096 pixels per side.";return false;}
        std::vector<unsigned char> pixels(size_t(width)*height*4);
        if(FAILED(converter->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data())))
        {error="Cannot read mask pixels.";return false;}
        UINT left=width,right=0,top=height,bottom=0;
        for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x)
        {
            const auto at=(size_t(y)*width+x)*4;
            // Match the renderer: white/red-channel coverage times alpha.
            if(unsigned(pixels[at])*pixels[at+3]<=255)continue;
            left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1);
        }
        if(left>=right||top>=bottom){error="The mask has no visible fill area. Use white inside the shape and black or transparency outside.";return false;}
        next.left=float(left)/width;next.right=float(right)/width;next.top=float(top)/height;next.bottom=float(bottom)/height;
        error.clear();return true;
#else
        error="Liquid mask inspection currently requires Windows.";return false;
#endif
    }
}
    bool InspectScreenLiquidMask(const std::string& root,const std::string& relative,ScreenLiquid& settings,std::string& error)
    {
        std::string absolute;if(!ResolveScreenResourcePath(root,relative,absolute,error))return false;
        auto next=settings;if(!InspectFile(absolute,next,error))return false;
        next.mask=relative;settings=std::move(next);return true;
    }
    bool ImportScreenLiquidMask(const std::string& root,const StableId& project,const std::string& source,ScreenLiquid& settings,std::string& error)
    {
        namespace fs=std::filesystem;
        auto next=settings;if(!InspectFile(source,next,error))return false;
        const auto relative=fs::u8path(source).lexically_relative(fs::u8path(root)).generic_u8string();
        std::string absolute;
        if(ResolveScreenResourcePath(root,relative,absolute,error))next.mask=relative;
        else
        {
            const auto imported=CreatorTextureWorkflowService{}.ImportTexture(root,project,source);
            if(!imported.succeeded){error=imported.error;return false;}
            const auto mirror=fs::path("Content/UI/Images")/fs::u8path(imported.assetId+fs::u8path(source).extension().u8string());
            const auto retained=fs::u8path(root)/fs::u8path(imported.sourceProjectRelativePath);
            std::ifstream input(retained,std::ios::binary|std::ios::ate);
            const auto size=input?input.tellg():std::streampos(-1);
            if(size<=0||size>128*1024*1024){error="Cannot copy imported mask source.";return false;}
            ProjectDocumentWrite write;write.destinationPath=(fs::u8path(root)/mirror).generic_u8string();write.content.resize(size_t(size));input.seekg(0);
            if(!input.read(reinterpret_cast<char*>(write.content.data()),size)){error="Cannot read imported mask source.";return false;}
            ProjectDocumentTransactionOptions options;options.allowedRoot=root;options.journalDirectory=(fs::u8path(root)/"Saved/Transactions").generic_u8string();
            auto committed=ProjectDocumentTransaction{}.Execute({std::move(write)},options);
            if(!committed.success){error=committed.message;return false;}
            next.mask=mirror.generic_u8string();
        }
        settings=std::move(next);error.clear();return true;
    }
}
