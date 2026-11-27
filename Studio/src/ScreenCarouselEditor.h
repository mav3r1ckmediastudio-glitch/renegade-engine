#pragma once
#include <windows.h>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <wiHelper.h>
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"comdlg32.lib")

namespace renegade::studio
{
    namespace carousel_editor
    {
        inline std::wstring Wide(const std::string& value)
        {std::wstring result;wi::helper::StringConvert(value,result);return result;}
        inline std::string Text(HWND dialog,int field)
        {
            HWND control=GetDlgItem(dialog,field);
            std::wstring value(GetWindowTextLengthW(control)+1,L'\0');
            value.resize(GetWindowTextW(control,value.data(),static_cast<int>(value.size())));
            std::string result;wi::helper::StringConvert(value,result);return result;
        }
        struct State
        {
            bridge::ScreenCarousel deck;
            std::string root;
            bridge::StableId project;
            int selected=-1;
            std::unique_ptr<Gdiplus::Image> preview;

            void SaveCaption(HWND dialog)
            {
                if(selected>=0 && selected<static_cast<int>(deck.entries.size()))
                    deck.entries[selected].caption=Text(dialog,6422);
            }
            void Show(HWND dialog,int next)
            {
                selected=next;
                SendDlgItemMessageW(dialog,6420,LB_RESETCONTENT,0,0);
                for(size_t i=0;i<deck.entries.size();++i)
                {
                    const auto label=Wide(std::to_string(i+1)+". "+
                        std::filesystem::u8path(deck.entries[i].image).filename().u8string());
                    SendDlgItemMessageW(dialog,6420,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
                }
                SendDlgItemMessageW(dialog,6420,LB_SETCURSEL,selected,0);
                const bool ready=selected>=0 && selected<static_cast<int>(deck.entries.size());
                SetDlgItemTextW(dialog,6422,ready?Wide(deck.entries[selected].caption).c_str():L"");
                SetDlgItemTextW(dialog,6423,ready?Wide(deck.entries[selected].image).c_str():L"No image selected");
                for(int id:{6422,6425,6426,6427,6428,6432,6433})
                    EnableWindow(GetDlgItem(dialog,id),ready);
                EnableWindow(GetDlgItem(dialog,6427),ready && selected>0);
                EnableWindow(GetDlgItem(dialog,6428),ready && selected+1<static_cast<int>(deck.entries.size()));
                preview.reset();
                if(ready)
                {
                    std::string absolute,error;
                    if(bridge::ResolveScreenResourcePath(root,deck.entries[selected].image,absolute,error))
                        preview.reset(Gdiplus::Image::FromFile(Wide(absolute).c_str()));
                }
                InvalidateRect(GetDlgItem(dialog,6421),nullptr,TRUE);
            }
            void Choose(HWND dialog,bool replace)
            {
                if(!replace && deck.entries.size()>=256)
                {MessageBoxW(dialog,L"A carousel supports up to 256 images.",L"Carousel",MB_OK);return;}
                std::wstring path(32768,L'\0');
                OPENFILENAMEW open{};open.lStructSize=sizeof(open);open.hwndOwner=dialog;
                open.lpstrFilter=L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.hdr\0All files\0*.*\0";
                open.lpstrFile=path.data();open.nMaxFile=static_cast<DWORD>(path.size());
                open.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
                if(!GetOpenFileNameW(&open))return;
                path.resize(wcslen(path.c_str()));
                std::string source;wi::helper::StringConvert(path,source);
                namespace fs=std::filesystem;
                std::string relative,error,absolute;
                const auto candidate=fs::u8path(source).lexically_relative(fs::u8path(root)).generic_u8string();
                const auto format=bridge::DetectResourceSourceFormat(source);
                if(bridge::ClassifyResourceSourceFormat(format)!=bridge::ResourceClass::Texture)
                {MessageBoxW(dialog,L"Choose a supported image.",L"Carousel",MB_OK|MB_ICONWARNING);return;}
                if(bridge::ResolveScreenResourcePath(root,candidate,absolute,error))
                    relative=candidate;
                else
                {
                    const auto imported=bridge::CreatorTextureWorkflowService{}.ImportTexture(root,project,source);
                    if(!imported.succeeded)
                    {MessageBoxW(dialog,Wide(imported.error).c_str(),L"Image import",MB_OK|MB_ICONERROR);return;}
                    auto image=fs::path("Content/UI/Images")/
                        fs::u8path(imported.assetId+fs::u8path(source).extension().u8string());
                    std::error_code ec;
                    fs::create_directories((fs::u8path(root)/image).parent_path(),ec);
                    if(!ec)fs::copy_file(fs::u8path(root)/fs::u8path(imported.sourceProjectRelativePath),
                        fs::u8path(root)/image,fs::copy_options::overwrite_existing,ec);
                    if(ec)
                    {MessageBoxW(dialog,Wide(ec.message()).c_str(),L"Image import",MB_OK|MB_ICONERROR);return;}
                    relative=image.generic_u8string();
                }
                SaveCaption(dialog);
                if(replace && selected>=0)deck.entries[selected].image=relative;
                else {deck.entries.push_back({bridge::GenerateStableId(),relative,{}});selected=static_cast<int>(deck.entries.size())-1;}
                Show(dialog,selected);
            }
        };

        inline INT_PTR CALLBACK Dialog(HWND dialog,UINT message,WPARAM w,LPARAM l)
        {
            auto* state=reinterpret_cast<State*>(GetWindowLongPtrW(dialog,DWLP_USER));
            if(message==WM_INITDIALOG)
            {
                state=reinterpret_cast<State*>(l);SetWindowLongPtrW(dialog,DWLP_USER,l);
                SendDlgItemMessageW(dialog,6422,EM_SETLIMITTEXT,16000,0);
                SetDlgItemTextW(dialog,6430,Wide(std::to_string(state->deck.seconds)).c_str());
                CheckDlgButton(dialog,6431,state->deck.shuffle?BST_CHECKED:BST_UNCHECKED);
                state->Show(dialog,state->deck.entries.empty()?-1:0);
                return TRUE;
            }
            if(!state)return FALSE;
            if(message==WM_DRAWITEM && w==6421)
            {
                auto* draw=reinterpret_cast<DRAWITEMSTRUCT*>(l);
                Gdiplus::Graphics graphics(draw->hDC);graphics.Clear(Gdiplus::Color(255,25,28,33));
                auto* image=state->preview.get();
                if(image && image->GetLastStatus()==Gdiplus::Ok && image->GetWidth() && image->GetHeight())
                {
                    const float width=float(draw->rcItem.right-draw->rcItem.left);
                    const float height=float(draw->rcItem.bottom-draw->rcItem.top);
                    const float factor=std::min(width/image->GetWidth(),height/image->GetHeight());
                    const float iw=image->GetWidth()*factor,ih=image->GetHeight()*factor;
                    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    graphics.DrawImage(state->preview.get(),Gdiplus::RectF((width-iw)/2,(height-ih)/2,iw,ih));
                }
                return TRUE;
            }
            if(message==WM_COMMAND)
            {
                const int id=LOWORD(w);
                if(id==6420 && HIWORD(w)==LBN_SELCHANGE)
                {const int next=static_cast<int>(SendDlgItemMessageW(dialog,6420,LB_GETCURSEL,0,0));state->SaveCaption(dialog);state->Show(dialog,next);return TRUE;}
                if(id==6424 || id==6425){state->Choose(dialog,id==6425);return TRUE;}
                if(id==6426 && state->selected>=0)
                {
                    state->deck.entries.erase(state->deck.entries.begin()+state->selected);
                    state->Show(dialog,state->deck.entries.empty()?-1:std::min(state->selected,static_cast<int>(state->deck.entries.size())-1));
                    return TRUE;
                }
                if(id==6427 || id==6428 || id==6432 || id==6433)
                {
                    state->SaveCaption(dialog);
                    const int size=static_cast<int>(state->deck.entries.size());
                    if(size && state->selected>=0)
                    {
                        const bool moving=id==6427||id==6428;
                        const int direction=(id==6427||id==6432)?-1:1;
                        const int next=moving?std::clamp(state->selected+direction,0,size-1):
                            (state->selected+direction+size)%size;
                        if(moving)std::swap(state->deck.entries[state->selected],state->deck.entries[next]);
                        state->Show(dialog,next);
                    }
                    return TRUE;
                }
                if(id==IDOK)
                {
                    state->SaveCaption(dialog);
                    const auto value=Text(dialog,6430);char* end=nullptr;
                    const auto seconds=std::strtof(value.c_str(),&end);
                    if(end==value.c_str() || *end || !std::isfinite(seconds) || seconds<.5f || seconds>3600)
                    {MessageBoxW(dialog,L"Seconds per image must be between 0.5 and 3600.",L"Carousel",MB_OK|MB_ICONWARNING);return TRUE;}
                    for(const auto& slide:state->deck.entries)if(slide.caption.size()>16384)
                    {MessageBoxW(dialog,L"A caption is too long (maximum 16384 UTF-8 bytes).",L"Carousel",MB_OK|MB_ICONWARNING);return TRUE;}
                    state->deck.seconds=seconds;state->deck.shuffle=IsDlgButtonChecked(dialog,6431)==BST_CHECKED;
                    EndDialog(dialog,IDOK);return TRUE;
                }
                if(id==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
            }
            if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
            return FALSE;
        }
    }
    inline bool EditScreenCarousel(bridge::ScreenCarousel& deck,const std::string& root,const bridge::StableId& project)
    {
        Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
        const bool started=Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok;
        bool saved=false;
        {
            carousel_editor::State state;state.deck=deck;state.root=root;state.project=project;
            saved=DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(6402),GetActiveWindow(),
                carousel_editor::Dialog,reinterpret_cast<LPARAM>(&state))==IDOK;
            if(saved)deck=std::move(state.deck);
        }
        if(started)Gdiplus::GdiplusShutdown(token);
        return saved;
    }
}
