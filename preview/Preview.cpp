#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include "../src/GlassTheme.h"
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Presentation harness: no Blackbone, driver, registry or injection code linked.
static GlassTheme theme;
INT_PTR CALLBACK PreviewProc(HWND h,UINT msg,WPARAM w,LPARAM l)
{
    if(msg==WM_INITDIALOG)
    {
        SetWindowTextW(h,L"Xenos Glass - apercu visuel");
        HWND list=GetDlgItem(h,IDC_MODS); RECT r;GetClientRect(list,&r);
        const wchar_t* labels[]={L"DLL",L"Architecture",L"Etat"};
        int widths[]={r.right*52/100,r.right*20/100,r.right*27/100};
        for(int i=0;i<3;++i) { LVCOLUMNW c={};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=const_cast<LPWSTR>(labels[i]);c.cx=widths[i];ListView_InsertColumn(list,i,&c); }
        ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_CHECKBOXES);
        const wchar_t* names[]={L"Exemple.dll",L"Ancienne-version.dll"};
        for(int i=0;i<2;++i) {LVITEMW item={};item.mask=LVIF_TEXT;item.iItem=i;item.pszText=const_cast<LPWSTR>(names[i]);ListView_InsertItem(list,&item); ListView_SetItemText(list,i,1,const_cast<LPWSTR>(L"64 bit"));ListView_SetItemText(list,i,2,const_cast<LPWSTR>(i?L"Manquant":L"Disponible"));ListView_SetCheckState(list,i,!i);}
        CheckDlgButton(h,IDC_AUTO_CS2,BST_CHECKED);CheckDlgButton(h,IDC_EXISTING_PROC,BST_CHECKED);
        SendDlgItemMessageW(h,IDC_COMBO_PROC,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"cs2.exe  (exemple)"));
        SendDlgItemMessageW(h,IDC_COMBO_PROC,CB_SETCURSEL,0,0);
        SetDlgItemTextW(h,IDC_TARGET_STATE,L"Apercu visuel  -  aucune connexion a un processus");
        SetDlgItemTextW(h,IDC_LIBRARY_STATE,L"1 selectionnee  /  2 memorisees");
        EnableWindow(GetDlgItem(h,IDC_EXECUTE),FALSE);EnableWindow(GetDlgItem(h,IDC_SELECT_PROC),FALSE);
        theme.Apply(h);return TRUE;
    }
    if(msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(w)==IDCANCEL)) {EndDialog(h,0);return TRUE;}
    if(msg==WM_COMMAND) return TRUE;
    return theme.Handle(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int)
{
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    return static_cast<int>(DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_MAIN),nullptr,PreviewProc,0));
}
