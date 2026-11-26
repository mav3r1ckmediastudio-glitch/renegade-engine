#pragma once

#include "../../Runtime/src/RuntimeScriptRuntime.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ScriptMetadataService.h"
#include "renegade/bridge/ZoneService.h"
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace renegade::studio
{
    // A window owns one project's source, even if the main Studio changes level.
    class ZoneScriptEditor
    {
        HWND window_ = nullptr, code_ = nullptr, status_ = nullptr;
        WNDPROC nativeEdit_ = nullptr;
        std::string root_, relative_, original_;
        bridge::ScriptAttachment attachment_;
        bridge::zones::Definition zone_;
        std::function<void()> refreshed_;
        bool enter_ = true;
        std::filesystem::file_time_type observed_{};
        HBRUSH background_ = CreateSolidBrush(RGB(29,32,35));
        HBRUSH sourceBackground_ = CreateSolidBrush(RGB(18,21,24));
        HFONT font_ = CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        enum { Save = 1, Enter, Exit };

        static std::wstring Wide(const std::string& s)
        {
            if (s.empty()) return {};
            int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
            std::wstring w(size, 0);
            if (size) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), w.data(), size);
            return w;
        }
        static std::string Utf8(const std::wstring& w)
        {
            if (w.empty()) return {};
            int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
            std::string s(size, 0);
            if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), int(w.size()), s.data(), size, nullptr, nullptr);
            return s;
        }
        std::filesystem::path Path() const { return std::filesystem::u8path(root_) / std::filesystem::u8path(relative_); }
        bool Read(std::string& value, std::string& error) const
        {
            if (!bridge::ValidateProjectScriptSource(root_, relative_, error)) return false;
            std::ifstream input(Path(), std::ios::binary);
            if (!input) { error = "Could not read the project script."; return false; }
            value.assign(std::istreambuf_iterator<char>(input), {});
            return !input.bad();
        }
        void DisplaySource()
        {
            auto w = Wide(original_);
            std::wstring lines;
            for (size_t i = 0; i < w.size(); ++i)
            {
                if (w[i] == L'\n' && (i == 0 || w[i-1] != L'\r')) lines += L'\r';
                lines += w[i];
            }
            SetWindowTextW(code_, lines.c_str());
            SendMessageW(code_, EM_SETMODIFY, FALSE, 0);
        }
        void CheckExternalSave()
        {
            std::error_code ec;
            const auto timestamp = std::filesystem::last_write_time(Path(),ec);
            if (ec || timestamp == observed_) return;
            observed_ = timestamp;
            std::string disk,error;
            if (!Read(disk,error)) { Status(error); return; }
            if (disk == original_) return;
            if (SendMessageW(code_,EM_GETMODIFY,0,0))
            {
                Status("Project file changed on disk. Your unsaved edits are still here.");
                return;
            }
            original_=std::move(disk); DisplaySource();
            if (refreshed_) refreshed_();
            Preview("Reloaded  |  ");
        }
        void Status(const std::string& s) { SetWindowTextW(status_, Wide(s).c_str()); }

        void Preview(const std::string& prefix)
        {
            wi::scene::Scene scene;
            auto id = scene.Entity_CreateTransform("Preview zone");
            std::string error;
            if (!bridge::AssignPersistentEntityId(scene, id, attachment_.ownerEntityId, error)) { Status(error); return; }
            auto doc = bridge::CreateScriptDocument(bridge::GenerateStableId(), bridge::GenerateStableId(), "Content/Scenes/Preview.wiscene", "zone-preview");
            doc.attachments.push_back(attachment_);
            doc.attachments.back().order = 0;
            std::string result;
            runtime::RuntimeScriptRuntime lua;
            const std::string edge = bridge::zones::EdgeEvent(zone_.behaviour, enter_);
            lua.SetGameplayEventObserver([&](const bridge::GameplayEvent& e) {
                if (e.name == edge) return;
                std::string action = e.name;
                if (e.name == "zone.complete") action = "Complete level";
                if (e.name == "zone.forward.enter" || e.name == "zone.forward.exit")
                    action = e.payload.empty() ? "No event configured" : "Send " + e.payload;
                if (!result.empty()) result += ", ";
                result += action;
            });
            if (!lua.StartScene(doc, scene, root_, error)) { Status(prefix + error); return; }
            bridge::GameplayEvent e;
            e.name = edge; e.targetEntityId = e.senderEntityId = attachment_.ownerEntityId;
            e.payload = zone_.behaviour==bridge::zones::Behaviour::Trigger ? (enter_ ? zone_.enter : zone_.exit)
                : (enter_ ? zone_.primary : zone_.secondary);
            if (!lua.EnqueueGameplayEvent(std::move(e), error)) { Status(prefix + error); return; }
            lua.Update(0); lua.Update(0);
            for (const auto& diagnostic : lua.Diagnostics())
                if (diagnostic.disabledInstance) { Status(prefix + diagnostic.message); return; }
            Status(prefix + (enter_ ? "Enter" : "Exit") + " preview  |  " + (result.empty() ? "No event emitted" : result));
        }
        bool SaveSource()
        {
            if (!SendMessageW(code_, EM_GETMODIFY, 0, 0)) { Preview(""); return true; }
            std::wstring buffer(GetWindowTextLengthW(code_) + 1, 0);
            buffer.resize(GetWindowTextW(code_, buffer.data(), int(buffer.size())));
            std::string source = Utf8(buffer);
            if (source.empty() || source.size() > bridge::MaximumScriptSourceBytes) { Status("Invalid or oversized Lua source."); return false; }
            std::string bytes;
            for (size_t i=0; i<source.size(); ++i)
                if (source[i] != '\r' || i+1 == source.size() || source[i+1] != '\n') bytes += source[i];
            std::string current, error;
            if (!Read(current,error)) { Status(error); return false; }
            if (current != original_) { Status("The file changed on disk. Copy your edits before reopening it."); return false; }

            // The existing metadata evaluator compiles the full Lua chunk and checks its declaration.
            // Use a separate project root so a rejected draft never replaces the working file.
            std::error_code ec;
            auto temp = std::filesystem::temp_directory_path(ec) / ("RenegadeZoneDraft-" + bridge::GenerateStableId());
            if (ec || !std::filesystem::create_directory(temp,ec)) { Status("Could not stage the draft."); return false; }
            struct Cleanup { std::filesystem::path path; ~Cleanup(){ std::error_code ignored; std::filesystem::remove_all(path,ignored); } } cleanup{temp};
            const auto staged = temp / "Content/Scripts/Draft.lua";
            std::filesystem::create_directories(staged.parent_path(),ec);
            std::ofstream file(staged,std::ios::binary);
            file.write(bytes.data(),bytes.size()); file.close();
            if (ec || !file) { Status("Could not stage the draft."); return false; }
            const auto metadata = bridge::EvaluateScriptMetadata(temp.generic_u8string(), "Content/Scripts/Draft.lua");
            if (!metadata.succeeded || metadata.descriptor.presentation != bridge::ScriptPresentation::Script)
            {
                Status(metadata.diagnostics.empty() ? "Zone source must declare the SCRIPT role." : metadata.diagnostics.front().message);
                return false;
            }
            bridge::ProjectDocumentWrite write;
            write.destinationPath = Path().generic_u8string();
            write.content.assign(bytes.begin(),bytes.end());
            bridge::ProjectDocumentTransactionOptions options;
            options.allowedRoot = root_;
            options.journalDirectory = (std::filesystem::u8path(root_) / "Intermediate/Transactions").generic_u8string();
            auto saved = bridge::ProjectDocumentTransaction{}.Execute({std::move(write)},options);
            if (!saved.success) { Status(saved.message); return false; }
            original_ = std::move(bytes);
            SendMessageW(code_, EM_SETMODIFY, FALSE, 0);
            if (refreshed_) refreshed_();
            Preview("Saved  |  ");
            return true;
        }
        static LRESULT CALLBACK TextProc(HWND h,UINT m,WPARAM w,LPARAM l)
        {
            auto* self = reinterpret_cast<ZoneScriptEditor*>(GetWindowLongPtrW(h,GWLP_USERDATA));
            if (m == WM_KEYDOWN && w == 'S' && (GetKeyState(VK_CONTROL)&0x8000)) { self->SaveSource(); return 0; }
            if (m == WM_CHAR && w == 19) return 0;
            return CallWindowProcW(self->nativeEdit_,h,m,w,l);
        }
        static LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l)
        {
            if (m == WM_NCCREATE)
            {
                auto* self = static_cast<ZoneScriptEditor*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
                SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
            }
            auto* self = reinterpret_cast<ZoneScriptEditor*>(GetWindowLongPtrW(h,GWLP_USERDATA));
            if (!self) return DefWindowProcW(h,m,w,l);
            switch(m)
            {
                case WM_TIMER: self->CheckExternalSave(); return 0;
                case WM_ERASEBKGND: {
                    RECT rc; GetClientRect(h,&rc);
                    FillRect(reinterpret_cast<HDC>(w),&rc,self->background_); return 1;
                }
                case WM_CTLCOLORSTATIC:
                case WM_CTLCOLOREDIT: {
                    HDC dc=reinterpret_cast<HDC>(w);
                    const bool source=m==WM_CTLCOLOREDIT;
                    SetTextColor(dc,RGB(232,237,240));SetBkColor(dc,source?RGB(18,21,24):RGB(29,32,35));
                    return reinterpret_cast<LRESULT>(source?self->sourceBackground_:self->background_);
                }
                case WM_SIZE:
                    if (self->code_) {
                        int width=LOWORD(l),height=HIWORD(l);
                        MoveWindow(self->code_,8,46,std::max(1,width-16),std::max(1,height-122),TRUE);
                        MoveWindow(self->status_,10,std::max(52,height-70),std::max(1,width-20),60,TRUE);
                    }
                    break;
                case WM_COMMAND:
                    if (HIWORD(w) != BN_CLICKED) break;
                    if (LOWORD(w)==Save) self->SaveSource();
                    if (LOWORD(w)==Enter || LOWORD(w)==Exit) {
                        self->enter_ = LOWORD(w)==Enter;
                        self->Preview(SendMessageW(self->code_,EM_GETMODIFY,0,0) ? "Saved source  |  " : "");
                    }
                    break;
                case WM_CLOSE:
                    if (SendMessageW(self->code_,EM_GETMODIFY,0,0)) {
                        int choice=MessageBoxW(h,L"Save changes to this project script?",L"Zone Lua",MB_YESNOCANCEL);
                        if (choice==IDCANCEL || (choice==IDYES && !self->SaveSource())) return 0;
                    }
                    DestroyWindow(h); return 0;
                case WM_NCDESTROY:
                    self->window_=nullptr; self->code_=nullptr; self->status_=nullptr; self->refreshed_={}; break;
            }
            return DefWindowProcW(h,m,w,l);
        }
    public:
        ~ZoneScriptEditor()
        {
            if (window_) DestroyWindow(window_);
            DeleteObject(font_); DeleteObject(sourceBackground_); DeleteObject(background_);
        }
        bool Open(HWND owner,const std::string& root,const bridge::ScriptAttachment& attachment,
                  const bridge::zones::Definition& zone,std::function<void()> refreshed,std::string& error)
        {
            if(window_) {
                if (root==root_ && relative_==attachment.sourcePath && attachment.scriptInstanceId==attachment_.scriptInstanceId) {
                    zone_=zone; SetForegroundWindow(window_); return true;
                }
                SendMessageW(window_,WM_CLOSE,0,0);
                if (window_) return true;
            }
            root_=root; relative_=attachment.sourcePath; attachment_=attachment; zone_=zone; enter_=true;
            if (!Read(original_,error)) return false;
            WNDCLASSW type{}; type.lpfnWndProc=Proc; type.hInstance=GetModuleHandleW(nullptr);
            type.lpszClassName=L"RenegadeZoneLuaEditor"; type.hCursor=LoadCursorW(nullptr,IDC_ARROW);
            type.hbrBackground=GetSysColorBrush(COLOR_BTNFACE);
            if (!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) { error="Cannot register Lua editor.";return false; }
            const auto title=L"Zone Lua  |  "+Path().wstring();
            window_=CreateWindowExW(0,type.lpszClassName,title.c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,860,630,owner,nullptr,type.hInstance,this);
            if(!window_){error="Cannot open Lua editor.";return false;}
            refreshed_=std::move(refreshed);
            auto child=[&](const wchar_t* cls,const wchar_t* label,DWORD style,int x,int y,int width,int height,int id){
                return CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window_,reinterpret_cast<HMENU>(INT_PTR(id)),type.hInstance,nullptr);
            };
            child(L"BUTTON",L"Save (Ctrl+S)",WS_TABSTOP,8,7,140,31,Save);
            child(L"BUTTON",L"Preview Enter",WS_TABSTOP,156,7,140,31,Enter);
            child(L"BUTTON",L"Preview Exit",WS_TABSTOP,304,7,140,31,Exit);
            code_=child(L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_WANTRETURN|WS_HSCROLL|WS_VSCROLL,0,0,0,0,10);
            status_=child(L"STATIC",L"",SS_LEFT,0,0,0,0,11);
            SetWindowLongPtrW(code_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
            nativeEdit_=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(code_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&TextProc)));
            SendMessageW(code_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),FALSE);
            SendMessageW(code_,EM_SETLIMITTEXT,bridge::MaximumScriptSourceBytes,0);
            DisplaySource(); RECT rc;GetClientRect(window_,&rc);SendMessageW(window_,WM_SIZE,0,MAKELPARAM(rc.right,rc.bottom));
            std::error_code ec; observed_ = std::filesystem::last_write_time(Path(),ec);
            SetTimer(window_,1,750,nullptr);
            ShowWindow(window_,SW_SHOW); SetFocus(code_); Preview(""); return true;
        }
    };
}
