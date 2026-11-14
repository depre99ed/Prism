#include "MainDlg.h"
#include "PrismePaths.h"
#include "VersionStore.h"
#include "ManagedCopies.h"
#include <filesystem>

void MainDlg::PreserveVersions(int index)
{
    if(index<0 || index>=(int)_versions.size())return;
    const auto root=PrismeDataDirectory();bool changed=false;
    for(auto& version:_versions[index].items)
    {
        if(!std::filesystem::is_regular_file(version.path))continue;
        auto path=std::filesystem::absolute(version.path).lexically_normal().wstring();
        auto stored=root+L"\\Versions\\",imported=root+L"\\Catalogues\\";
        if((path.size()>=stored.size() && _wcsnicmp(path.c_str(),stored.c_str(),stored.size())==0) ||
            (path.size()>=imported.size() && _wcsnicmp(path.c_str(),imported.c_str(),imported.size())==0))continue;
        if(!ValidateCheat(version.path,false))continue;
        auto saved=VersionStore::Snapshot(version.path,root+L"\\Versions",version.label);
        version=std::move(saved);changed=true;
    }
    if(changed)
    {
        const auto active=_versions[index].items[_versions[index].active].path;
        if(active!=_imagePaths[index] && !ActivateVersion(index,_versions[index].active,false))throw std::runtime_error("Stored version could not be read");
        if(SaveConfig()!=ERROR_SUCCESS)throw std::runtime_error("Version catalogue could not be saved");
    }
}

void MainDlg::ShowVersions(int index)
{
    if(index<0 || index>=(int)_imagePaths.size())return;
    SyncCatalogue();
    try{PreserveVersions(index);}catch(...){Message::ShowError(_hwnd,L"Unable to preserve an existing DLL version. Check the file and available disk space.");return;}
    _versionCheat=index;_versionChoice=_versions[index].active;RefreshVersionPanel();_view.overlay=8;_view.panelFirst=0;
}

void MainDlg::RefreshVersionPanel()
{
    if(_versionCheat<0 || _versionCheat>=(int)_versions.size())return;
    _view.versionItems.clear();const auto& group=_versions[_versionCheat];_view.versionTitle=_cheatTitles[_versionCheat];
    for(size_t i=0;i<group.items.size();++i)
    {
        const auto& version=group.items[i];std::wstring detail=std::filesystem::path(version.path).filename().wstring();
        if(!std::filesystem::is_regular_file(version.path))detail+=L"  \u00b7  missing";
        else if((int)i==group.active)detail+=L"  \u00b7  active";
        _view.versionItems.push_back({version.label,detail});
    }
    _view.versionChoice=_versionChoice;
}

bool MainDlg::ActivateVersion(int cheat,int version,bool save)
{
    if(cheat<0 || cheat>=(int)_versions.size() || version<0 || version>=(int)_versions[cheat].items.size())return false;
    const auto path=_versions[cheat].items[version].path;
    auto image=std::make_shared<blackbone::pe::PEImage>();
    if(!NT_SUCCESS(image->Load(path)) || image->isExe() || image->pureIL() || image->mType()!=blackbone::mt_mod64)
    {if(save)Message::ShowError(_hwnd,L"This DLL version is missing or is not a valid native x64 DLL.");return false;}
    blackbone::pe::vecExports exports;image->GetExports(exports);image->Release(true);
    _images[cheat]=std::move(image);_exports[cheat]=std::move(exports);_imagePaths[cheat]=path;_versions[cheat].active=version;
    auto filename=blackbone::Utils::StripPath(path);ListView_SetItemText(_modules.hwnd(),cheat,0,const_cast<LPWSTR>(filename.c_str()));
    ListView_SetItemText(_modules.hwnd(),cheat,1,const_cast<LPWSTR>(L"64 bit"));
    if(save && SaveConfig()!=ERROR_SUCCESS){Message::ShowError(_hwnd,L"Unable to save the selected version.");return false;}
    ClearLoaderStatus();RefreshLibrary();return true;
}

void MainDlg::EditVersion(bool add)
{
    if(_versionCheat<0 || _versionCheat>=(int)_versions.size())return;
    const auto& group=_versions[_versionCheat];
    _editingVersion=add?-1:_versionChoice;
    if(!add && (_editingVersion<0 || _editingVersion>=(int)group.items.size()))return;
    if(add)
    {
        size_t count=0;for(const auto& cheat:_versions)count+=cheat.items.size();
        if(group.items.size()>=100 || count>=1000){Message::ShowInfo(_hwnd,L"Keep at most 100 versions per cheat and 1000 versions in the catalogue.");return;}
        std::wstring path;if(!OpenSaveDialog(L"DLL libraries (*.dll)\0*.dll\0",1,path) || !ValidateCheat(path))return;
        _editVersionPath=path;_view.editorTitle=L"Version "+std::to_wstring(group.items.size()+1);
    }
    else {_editVersionPath=group.items[_editingVersion].path;_view.editorTitle=group.items[_editingVersion].label;}
    _view.editorVersion=true;_view.editorNew=add;_view.editorDllName=blackbone::Utils::StripPath(_editVersionPath);
    _view.editorCaret=(int)_view.editorTitle.size();_view.editorSelectAll=true;_view.editorFocused=true;_view.caretVisible=true;
    _view.focus=-1;_view.overlay=4;SetFocus(_hwnd);
}

void MainDlg::CommitVersion()
{
    if(_versionCheat<0 || _versionCheat>=(int)_versions.size())return;
    auto title=_view.editorTitle;auto start=title.find_first_not_of(L" \t\r\n"),end=title.find_last_not_of(L" \t\r\n");
    if(start==title.npos){Message::ShowInfo(_hwnd,L"Enter a version label.");return;}title=title.substr(start,end-start+1);
    auto& group=_versions[_versionCheat];
    try
    {
        if(_editingVersion<0)
        {
            auto hash=VersionStore::Digest(_editVersionPath);
            for(const auto& existing:group.items)if(existing.sha256==hash)
            {Message::ShowInfo(_hwnd,L"This DLL content is already present in this cheat.");return;}
            if(!ValidateCheat(_editVersionPath))return;
            auto version=VersionStore::Snapshot(_editVersionPath,PrismeDataDirectory()+L"\\Versions",title);
            group.items.push_back(std::move(version));_versionChoice=(int)group.items.size()-1;
            if(!ActivateVersion(_versionCheat,_versionChoice,false)){group.items.pop_back();return;}
        }
        else
        {
            if(_editingVersion>=(int)group.items.size())return;
            group.items[_editingVersion].label=title;_versionChoice=_editingVersion;
        }
        if(SaveConfig()!=ERROR_SUCCESS){Message::ShowError(_hwnd,L"The version is available in this session, but its catalogue could not be saved.");return;}
        _view.editorVersion=false;_view.overlay=8;RefreshVersionPanel();
        _view.panelFirst=(std::max)(0,_versionChoice-2);_loaderStatus=L"Version saved.";
    }
    catch(...){Message::ShowError(_hwnd,L"Unable to store the DLL version. Existing versions have been kept.");}
}

void MainDlg::RemoveVersion()
{
    if(_versionCheat<0 || _versionCheat>=(int)_versions.size())return;
    auto& group=_versions[_versionCheat];
    if(group.items.size()<=1){Message::ShowInfo(_hwnd,L"Keep at least one version. Remove the cheat to clear it entirely.");return;}
    if(_versionChoice<0 || _versionChoice>=(int)group.items.size())return;
    if(_versionChoice==group.active)
    {
        int replacement=_versionChoice==0?1:0;
        if(!ActivateVersion(_versionCheat,replacement,false)){Message::ShowInfo(_hwnd,L"Choose an available replacement before removing the active version.");return;}
    }
    auto removedPath=group.items[_versionChoice].path;
    int removed=_versionChoice;group.items.erase(group.items.begin()+removed);if(group.active>removed)--group.active;
    _versionChoice=group.active;
    if(SaveConfig()!=ERROR_SUCCESS)Message::ShowError(_hwnd,L"The version was removed for this session, but the catalogue could not be saved.");
    else DeleteStoredCopies({removedPath});
    RefreshVersionPanel();_view.panelFirst=0;
}

void MainDlg::DeleteStoredCopies(const std::vector<std::wstring>& candidates)
{
    std::vector<std::wstring> references;
    for(const auto& group:_versions)for(const auto& version:group.items)references.push_back(version.path);
    size_t failed=0;
    for(const auto& path:candidates)
    {
        try {if(ManagedCopies::Remove(path,PrismeDataDirectory(),references)==ManagedCopies::Result::Failed)++failed;}
        catch(...) {++failed;}
    }
    if(failed)Message::ShowInfo(_hwnd,L"The catalogue was updated, but some stored copies are still in use or could not be deleted. Original files were kept.");
}
