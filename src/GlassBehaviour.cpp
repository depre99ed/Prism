#include "MainDlg.h"
#include <algorithm>

INT_PTR MainDlg::OnTheme(HWND,UINT,WPARAM,LPARAM) { return FALSE; }

INT_PTR MainDlg::OnCommand(HWND h,UINT m,WPARAM w,LPARAM l)
{
    // Keyboard accelerators also arrive here, even when menu buttons are disabled.
    if (_injecting || _view.overlay==4 || _view.overlay==10) return TRUE;
    return Dialog::OnCommand(h,m,w,l);
}

INT_PTR MainDlg::OnMenu(HWND,UINT,WPARAM,LPARAM)
{
    HMENU popup=CreatePopupMenu();
    AppendMenuW(popup,MF_STRING,ID_PROFILES_OPEN,L"Open profile\tCtrl+O");
    AppendMenuW(popup,MF_STRING,ID_PROFILES_SAVE,L"Save profile\tCtrl+S");
    AppendMenuW(popup,MF_SEPARATOR,0,nullptr);
    AppendMenuW(popup,MF_STRING,ID_TOOLS_EJECTMODULES,L"Process modules");
    AppendMenuW(popup,MF_STRING|(blackbone::Driver().loaded()?MF_ENABLED:MF_GRAYED),ID_TOOLS_PROTECT,L"Process protection");
    RECT rc; GetWindowRect(GetDlgItem(_hwnd,IDC_MENU),&rc);
    TrackPopupMenu(popup,TPM_RIGHTALIGN|TPM_TOPALIGN,rc.right,rc.bottom,0,_hwnd,nullptr);
    DestroyMenu(popup); return TRUE;
}

bool MainDlg::HasReadySelection() const
{
    bool selected = false;
    for (size_t i=0; i<_images.size(); ++i)
        if (_imageEnabled[i])
        {
            selected = true;
            if (!_images[i] || GetFileAttributesW(_imagePaths[i].c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        }
    return selected;
}

void MainDlg::RefreshLibrary()
{
    unsigned selected=0, unavailable=0;
    for (size_t i=0; i<_imagePaths.size(); ++i)
    {
        bool exists = GetFileAttributesW(_imagePaths[i].c_str()) != INVALID_FILE_ATTRIBUTES;
        const wchar_t* state = !exists ? L"Missing" : (!_images[i] ? L"Reload required" : L"Available");
        ListView_SetItemText(_modules.hwnd(),static_cast<int>(i),2,const_cast<LPWSTR>(state));
        if (_imageEnabled[i]) { ++selected; if (!exists || !_images[i]) ++unavailable; }
    }
    std::wstring text;
    if (_imagePaths.empty()) text=L"Add a DLL or drag it into this window.";
    else text=std::to_wstring(selected)+L" selected  /  "+std::to_wstring(_imagePaths.size())+L" saved";
    if (unavailable) text+=L"  -  file unavailable: use Locate.";
    SetDlgItemTextW(_hwnd,IDC_LIBRARY_STATE,text.c_str());
    UpdateInterface();
}

void MainDlg::RefreshTarget()
{
    if (_injecting || !_profileMgr.config().autoTargetCs2) return;
    auto found=blackbone::Process::EnumByName(L"cs2.exe");
    DWORD pid=found.empty() ? 0 : found.front();
    // Preserve a stable selected instance while it still exists.
    if (std::find(found.begin(),found.end(),_lastAutoPid)!=found.end()) pid=_lastAutoPid;
    if (_lastAutoPid!=pid)
    {
        _lastAutoPid=pid;
        _exProc.checked(true); _newProc.checked(false); _autoProc.checked(false);
        _profileMgr.config().processMode=Existing;
        _processPath=L"cs2.exe";
        FillProcessList();
        if (pid) {
            for(int i=0;i<ComboBox_GetCount(_procList.hwnd());++i)
                if(static_cast<DWORD>(_procList.itemData(i))==pid) { _procList.selection(i); break; }
        }
        else _procList.selection(-1);
    }
    std::wstring state=pid ? L"CS2 detected  -  PID "+std::to_wstring(pid)+L"  -  manual loading" : L"Waiting for CS2";
    SetDlgItemTextW(_hwnd,IDC_TARGET_STATE,state.c_str());
    UpdateInterface();
}

INT_PTR MainDlg::OnTimer(HWND,UINT,WPARAM id,LPARAM)
{
    if (id==2) { TickPrisme(); return TRUE; }
    if (id==3) {_view.caretVisible=GetFocus()==_hwnd?!_view.caretVisible:false;TickPrisme();return TRUE;}
    TickLoader();
    if (!_injecting) { RefreshTarget(); RefreshLibrary(); }
    return TRUE;
}

INT_PTR MainDlg::OnAutoCS2(HWND,UINT,WPARAM,LPARAM)
{
    bool enabled=IsDlgButtonChecked(_hwnd,IDC_AUTO_CS2)==BST_CHECKED;
    _profileMgr.config().autoTargetCs2=enabled;
    _lastAutoPid=MAXDWORD;
    if (enabled) RefreshTarget();
    else SetDlgItemTextW(_hwnd,IDC_TARGET_STATE,L"Manual process selection");
    SaveConfig();
    return TRUE;
}

INT_PTR MainDlg::OnNotify(HWND,UINT,WPARAM,LPARAM lp)
{
    auto hdr=reinterpret_cast<NMHDR*>(lp);
    if(hdr->idFrom!=IDC_MODS) return FALSE;
    if(hdr->code==LVN_GETINFOTIPW)
    {
        auto tip=reinterpret_cast<NMLVGETINFOTIPW*>(lp);
        if(tip->iItem>=0 && static_cast<size_t>(tip->iItem)<_imagePaths.size())
            lstrcpynW(tip->pszText,_imagePaths[tip->iItem].c_str(),tip->cchTextMax);
        return TRUE;
    }
    if(hdr->code==LVN_ITEMCHANGED && !_loadingLibrary && !_injecting)
    {
        auto item=reinterpret_cast<NMLISTVIEW*>(lp);
        if(item->iItem>=0 && static_cast<size_t>(item->iItem)<_imageEnabled.size() &&
            ((item->uNewState^item->uOldState)&LVIS_STATEIMAGEMASK))
        {
            _imageEnabled[item->iItem]=ListView_GetCheckState(_modules.hwnd(),item->iItem)!=FALSE;
            SaveConfig(); RefreshLibrary();
        }
    }
    return FALSE;
}

INT_PTR MainDlg::OnLocateImage(HWND,UINT,WPARAM,LPARAM)
{
    int index=_modules.selection();
    if(index<0) return TRUE;
    std::wstring path;
    if(!OpenSaveDialog(L"Images PE (*.dll;*.exe;*.sys)\0*.dll;*.exe;*.sys\0All files\0*.*\0",1,path)) return TRUE;
    if(!ValidateCheat(path))return TRUE;
    for(size_t i=0;i<_imagePaths.size();++i)
        if(static_cast<int>(i)!=index && _wcsicmp(_imagePaths[i].c_str(),path.c_str())==0)
        { Message::ShowInfo(_hwnd,L"This file is already in the library."); return TRUE; }
    // Validate before replacing the old entry. Append uses the unchanged PE parsing path.
    const size_t before=_images.size();
    const std::wstring oldPath=_imagePaths[index];
    _imagePaths[index]=L"";
    _loadingLibrary=true;
    if(LoadImageFile(path)!=ERROR_SUCCESS)
    {
        _imagePaths[index]=oldPath;
        _loadingLibrary=false;
        return TRUE;
    }
    _images[index]=_images.back(); _images.pop_back();
    _exports[index]=_exports.back(); _exports.pop_back();
    _imagePaths[index]=path; _imagePaths.pop_back();
    _imageEnabled.pop_back();
    _modules.RemoveItem(static_cast<int>(before));
    ListView_SetItemText(_modules.hwnd(),index,0,const_cast<LPWSTR>(blackbone::Utils::StripPath(path).c_str()));
    const wchar_t* arch=_images[index]->mType()==blackbone::mt_mod64 ? L"64 bit" : L"32 bit";
    ListView_SetItemText(_modules.hwnd(),index,1,const_cast<LPWSTR>(arch));
    _loadingLibrary=false;
    SaveConfig(); RefreshLibrary(); return TRUE;
}

void MainDlg::SetBusy(bool busy)
{
    _injecting=busy;
    for(int id : {IDC_COMBO_PROC,IDC_EXISTING_PROC,IDC_NEW_PROC,IDC_AUTO_PROC,IDC_SELECT_PROC,IDC_AUTO_CS2,
        IDC_MODS,IDC_ADD_MOD,IDC_REMOVE_MOD,IDC_CLEAR_MODS,IDC_LOCATE_MOD,IDC_SETTINGS,IDC_EXECUTE,IDC_MENU})
        EnableWindow(GetDlgItem(_hwnd,id),!busy);
    EnableMenuItem(GetMenu(_hwnd),0,MF_BYPOSITION|(busy?MF_GRAYED:MF_ENABLED));
    EnableMenuItem(GetMenu(_hwnd),1,MF_BYPOSITION|(busy?MF_GRAYED:MF_ENABLED));
    SetDlgItemTextW(_hwnd,IDC_EXECUTE,busy ? L"Loading..." : L"Load");
    if(!busy) UpdateInterface();
}

INT_PTR MainDlg::OnInjectionDone(HWND,UINT,WPARAM success,LPARAM status)
{
    if(_worker.joinable())_worker.join();
    _loader.Finish(success!=FALSE);
    if(success)_loadedCheats.insert({_loader.identity(),_loadingPath});
    SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
    if(success){_loaderStatus=L"Cheat loaded into CS2.";_view.errorAvailable=false;}
    else
    {
        for(const auto& line:_attemptTrace)
        {
            if(line.find("Can not attach")!=std::string::npos)_attemptStage=L"Opening the process in the engine";
            if(line.find("Context validation failed")!=std::string::npos)_attemptStage=L"Engine context validation";
            if(line.find("Failed to inject image using default")!=std::string::npos)_attemptStage=L"Standard Windows DLL loading";
        }
        RecordError(L"Loading failed",_attemptStage,static_cast<unsigned long>(status),true,_attemptFacts,_loader.pid(),_loader.identity());
    }
    DrawPrisme();return TRUE;
}


void MainDlg::ShowErrors()
{
    _view.errorItems.clear();
    for(const auto& entry:_errors.entries)_view.errorItems.push_back({entry.title,entry.time});
    _view.panelFirst=0;_view.overlay=6;
}

void MainDlg::ShowErrorDetail(int index)
{
    if(index<0 || index>=(int)_errors.entries.size())return;
    _view.errorIndex=index;_view.errorHeading=_errors.entries[index].title;_view.reportCopy=0;
    _view.errorLines=Diagnostics::Lines(_errors.entries[index].report);_view.panelFirst=0;_view.overlay=7;
}

void MainDlg::RecordError(const std::wstring& title,const std::wstring& stage,unsigned long code,bool native,
    const std::wstring& facts,DWORD pid,uint64_t identity)
{
    auto entry=Diagnostics::Make(title,stage,code,native,pid,identity,_loadingPath,facts,_attemptTrace);
    bool saved=_errors.Add(std::move(entry));
    _loaderStatus=code?Diagnostics::Hex(code)+L" \u00b7 view report":L"Incident - view report";
    if(!saved)_loaderStatus=L"Report available \u00b7 unable to save";
    _view.errorAvailable=true;ShowErrorDetail(0);
}

void MainDlg::CopyError()
{
    int i=_view.errorIndex;if(i<0 || i>=(int)_errors.entries.size())return;
    const auto& report=_errors.entries[i].report;SIZE_T bytes=(report.size()+1)*sizeof(wchar_t);
    _view.reportCopy=2;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory)return;
    auto destination=GlobalLock(memory);if(!destination){GlobalFree(memory);return;}
    memcpy(destination,report.c_str(),bytes);GlobalUnlock(memory);
    if(OpenClipboard(_hwnd))
    {
        if(EmptyClipboard() && SetClipboardData(CF_UNICODETEXT,memory)){memory=nullptr;_view.reportCopy=1;_loaderStatus=L"Report copied.";}
        CloseClipboard();
    }
    if(memory){GlobalFree(memory);_loaderStatus=L"Clipboard unavailable.";}
}
