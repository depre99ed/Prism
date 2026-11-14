#pragma once
#include "../src/DialogInput.h"
static INT_PTR CALLBACK KeyboardTestProc(HWND window,UINT message,WPARAM key,LPARAM data)
{
    if(message==WM_INITDIALOG){SetWindowLongPtrW(window,DWLP_USER,data);return TRUE;}
    if(message==WM_CHAR)
    {auto text=reinterpret_cast<std::wstring*>(GetWindowLongPtrW(window,DWLP_USER));text->push_back((wchar_t)key);return TRUE;}
    if(message==WM_KEYDOWN)return TRUE;
    return FALSE;
}
static void TestDialogInput()
{
    struct Template {DLGTEMPLATE dialog;WORD menu=0,windowClass=0,title=0;};
    Template resource={};resource.dialog.style=WS_POPUP|DS_CONTROL;resource.dialog.cx=100;resource.dialog.cy=60;
    std::wstring result;
    HWND root=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),&resource.dialog,nullptr,KeyboardTestProc,reinterpret_cast<LPARAM>(&result));
    require(root!=nullptr,"keyboard test creates a real Windows dialog");
    HWND hidden=CreateWindowExW(0,L"BUTTON",L"Hidden backend control",WS_CHILD,0,0,10,10,root,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(hidden!=nullptr,"keyboard test creates a hidden legacy control");
    const std::wstring expected=L"Mon titre \u00e9t\u00e9 2026";
    // Exercise the actual main-window dispatch, even if Windows retains focus on a hidden child.
    for(wchar_t c:expected)PostMessageW(hidden,WM_CHAR,c,0);
    MSG message={};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchDialogInput(root,message,nullptr,true);
    require(result==expected,"printable keys, spaces and accents reach the painted title editor");
    PostMessageW(root,WM_CHAR,L'!',0);
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchDialogInput(root,message,nullptr,true);
    require(result==expected+L"!","typing also works with focus on the painted dialog itself");
    DestroyWindow(root);
}
