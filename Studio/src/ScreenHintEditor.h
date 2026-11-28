#pragma once
#include <windows.h>
#include <string>
#include <wiHelper.h>

namespace renegade::studio
{
    inline bool EditHintText(std::string& text)
    {
        std::wstring lines;
        wi::helper::StringConvert(text,lines);
        for (size_t i=0;(i=lines.find(L'\n',i))!=std::wstring::npos;++i)
            if (i==0 || lines[i-1]!=L'\r') {lines.insert(i,1,L'\r');++i;}
        auto proc=[](HWND dialog,UINT msg,WPARAM w,LPARAM l)->INT_PTR
        {
            if (msg==WM_INITDIALOG)
            {
                auto& value=*reinterpret_cast<std::wstring*>(l);
                SetWindowLongPtrW(dialog,DWLP_USER,l);
                SendDlgItemMessageW(dialog,6410,EM_SETLIMITTEXT,0,0);
                SetDlgItemTextW(dialog,6410,value.c_str());
                return TRUE;
            }
            if (msg==WM_COMMAND && LOWORD(w)==IDOK)
            {
                auto& value=*reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dialog,DWLP_USER));
                HWND field=GetDlgItem(dialog,6410);
                value.resize(GetWindowTextLengthW(field)+1);
                value.resize(GetWindowTextW(field,value.data(),static_cast<int>(value.size())));
                EndDialog(dialog,IDOK);return TRUE;
            }
            if (msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(w)==IDCANCEL))
            {EndDialog(dialog,IDCANCEL);return TRUE;}
            return FALSE;
        };
        if (DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(6401),GetActiveWindow(),proc,
            reinterpret_cast<LPARAM>(&lines))!=IDOK) return false;
        wi::helper::StringConvert(lines,text);
        return true;
    }
}
