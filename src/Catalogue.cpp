#include "MainDlg.h"
#include "PrismePaths.h"
#include "VersionStore.h"

static std::wstring DefaultTitle(const std::wstring& path)
{
    auto name=blackbone::Utils::StripPath(path);
    auto dot=name.rfind(L'.');if(dot!=std::wstring::npos)name.resize(dot);
    return name.empty()?L"Untitled":name;
}

void MainDlg::SyncCatalogue()
{
    _cheatTitles.resize(_imagePaths.size());_cheatArtwork.resize(_imagePaths.size());
    ProfileMgr::ConfigData model;model.images=_imagePaths;model.versionGroups=_versions;CatalogueModel::Normalize(model);_versions=std::move(model.versionGroups);
    for(auto& group:_versions)if(group.id.empty())group.id=VersionStore::Id();
    if(_imagePaths.empty())_selectedCheat=-1;
    else if(_selectedCheat<0 || _selectedCheat>=(int)_imagePaths.size())_selectedCheat=0;
    if(_selectedCheat>=0 && (_selectedCheat<_view.first || _selectedCheat>=_view.first+2))
        _view.first=(std::max)(0,_selectedCheat-1);
    bool old=_loadingLibrary;_loadingLibrary=true;
    _imageEnabled.resize(_imagePaths.size());
    for(size_t i=0;i<_imagePaths.size();++i)
    {
        if(_cheatTitles[i].empty())_cheatTitles[i]=DefaultTitle(_imagePaths[i]);
        _imageEnabled[i]=static_cast<int>(i)==_selectedCheat;
        ListView_SetCheckState(_modules.hwnd(),static_cast<int>(i),_imageEnabled[i]);
    }
    _loadingLibrary=old;
}

void MainDlg::ClearLoaderStatus()
{
    if(!_injecting){_loaderStatus.clear();_view.errorAvailable=false;_status.SetText(2,L"Ready");}
}

void MainDlg::SelectCheat(int index)
{
    if(_injecting || index<0 || index>=(int)_imagePaths.size())return;
    _selectedCheat=index;_view.selected=index;ClearLoaderStatus();SyncCatalogue();
    ListView_SetItemState(_modules.hwnd(),index,LVIS_SELECTED,LVIS_SELECTED);
    if(SaveConfig()!=ERROR_SUCCESS)_loaderStatus=L"Unable to save the catalogue.";
    UpdateInterface();
}

bool MainDlg::ValidateCheat(const std::wstring& path,bool showError)
{
    blackbone::pe::PEImage image;
    bool okay=NT_SUCCESS(image.Load(path)) && !image.isExe() && !image.pureIL() && image.mType()==blackbone::mt_mod64;
    image.Release();
    if(!okay && showError)Message::ShowError(_hwnd,L"Choose a valid native 64-bit DLL for CS2.");
    return okay;
}

void MainDlg::EditCheat(bool add)
{
    _view.editorVersion=false;
    if(_injecting)return;
    if(add)
    {
        std::wstring path;
        if(!OpenSaveDialog(L"DLL libraries (*.dll)\0*.dll\0",1,path))return;
        if(!ValidateCheat(path))return;
        for(size_t i=0;i<_imagePaths.size();++i)
            if(_wcsicmp(path.c_str(),_imagePaths[i].c_str())==0){SelectCheat(static_cast<int>(i));return;}
        _editingCheat=-1;_editDllPath=path;_editArtwork.clear();_view.editorTitle=DefaultTitle(path);
    }
    else
    {
        if(_view.selected<0 || _view.selected>=(int)_imagePaths.size())return;
        _editingCheat=_view.selected;_editDllPath=_imagePaths[_editingCheat];
        _editArtwork=_cheatArtwork[_editingCheat];_view.editorTitle=_cheatTitles[_editingCheat];
    }
    _view.editorSelectAll=true;_view.editorCaret=(int)_view.editorTitle.size();_view.editorFocused=true;_view.caretVisible=true;_view.editorDllName=blackbone::Utils::StripPath(_editDllPath);
    _view.editorArtwork=_editArtwork;_view.editorNew=add;_view.overlay=4;SetFocus(_hwnd);_view.focus=-1;DrawPrisme();
}

void MainDlg::ChooseArtwork()
{
    std::wstring path;
    if(!OpenSaveDialog(L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.ico)\0*.png;*.jpg;*.jpeg;*.bmp;*.ico\0",1,path))return;
    Gdiplus::Bitmap source(path.c_str());
    if(source.GetLastStatus()!=Gdiplus::Ok || !source.GetWidth() || !source.GetHeight() || source.GetWidth()>16384 || source.GetHeight()>16384)
    {Message::ShowError(_hwnd,L"This image could not be read.");return;}
    Gdiplus::Bitmap result(256,256,PixelFormat32bppPARGB);
    {Gdiplus::Graphics g(&result);g.Clear(Gdiplus::Color(0,0,0,0));g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        float side=static_cast<float>((std::min)(source.GetWidth(),source.GetHeight()));
        g.DrawImage(&source,Gdiplus::RectF(0,0,256,256),(source.GetWidth()-side)/2,(source.GetHeight()-side)/2,side,side,Gdiplus::UnitPixel);}
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> data(size);
    auto encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(data.data());Gdiplus::GetImageEncoders(count,size,encoders);CLSID png={};
    for(UINT i=0;i<count;++i)if(wcscmp(encoders[i].MimeType,L"image/png")==0)png=encoders[i].Clsid;
    GUID id;CoCreateGuid(&id);wchar_t name[64];StringFromGUID2(id,name,64);
    auto directory=PrismeDataDirectory()+L"\\Artwork";CreateDirectoryW(directory.c_str(),nullptr);
    auto target=directory+L"\\"+name+L".png";
    if(result.Save(target.c_str(),&png)!=Gdiplus::Ok){Message::ShowError(_hwnd,L"Unable to save the image.");return;}
    _editArtwork=target;_view.editorArtwork=target;DrawPrisme();
}

void MainDlg::CommitCheat()
{
    if(_view.editorVersion){CommitVersion();return;}
    auto title=_view.editorTitle;
    auto first=title.find_first_not_of(L" \t\r\n"),last=title.find_last_not_of(L" \t\r\n");
    if(first==std::wstring::npos){Message::ShowInfo(_hwnd,L"Enter a title for this entry.");return;}
    title=title.substr(first,last-first+1);
    if(_editingCheat<0)
    {
        size_t count=0;for(const auto& group:_versions)count+=group.items.size();
        if(count>=1000){Message::ShowInfo(_hwnd,L"The catalogue can contain up to 1000 DLL versions.");return;}
        if(!ValidateCheat(_editDllPath))return;
        ProfileMgr::DllVersion initial;
        try{initial=VersionStore::Snapshot(_editDllPath,PrismeDataDirectory()+L"\\Versions",L"Initial version");}
        catch(...){Message::ShowError(_hwnd,L"Unable to store the initial DLL version.");return;}
        if(LoadImageFile(initial.path)!=ERROR_SUCCESS)return;
        _versions.resize(_imagePaths.size());_versions.back().id=VersionStore::Id();_versions.back().items={initial};_versions.back().active=0;
        _cheatTitles.resize(_imagePaths.size());_cheatArtwork.resize(_imagePaths.size());
    ProfileMgr::ConfigData model;model.images=_imagePaths;model.versionGroups=_versions;CatalogueModel::Normalize(model);_versions=std::move(model.versionGroups);
    for(auto& group:_versions)if(group.id.empty())group.id=VersionStore::Id();
        _editingCheat=static_cast<int>(_imagePaths.size())-1;
    }
    if(_editingCheat>=(int)_imagePaths.size())return;
    _cheatTitles[_editingCheat]=title;_cheatArtwork[_editingCheat]=_editArtwork;
    _view.overlay=0;SelectCheat(_editingCheat);RefreshLibrary();
}
