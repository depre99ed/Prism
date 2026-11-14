#include "MainDlg.h"
#include "CatalogueArchive.h"
#include "PrismePaths.h"
#include <filesystem>
#include <shlobj.h>

struct BundleResult
{
    bool success=false,importing=false,previewing=false;
    CatalogueArchive::PreviewInfo preview;
    int added=0,duplicates=0,versionsAdded=0;
    std::wstring path,error,profile;
};

bool MainDlg::ApplyCatalogue(const std::wstring& path)
{
    ProfileMgr candidate;
    if(!candidate.Load(path)){Message::ShowError(_hwnd,L"The catalogue is invalid or unreadable.");return false;}
    _loadingLibrary=true;
    _cheatTitles.clear();_cheatArtwork.clear();_versions.clear();_selectedCheat=-1;
    _imagePaths.clear();_imageEnabled.clear();_images.clear();_exports.clear();_modules.reset();
    _loadingLibrary=false;LoadConfig(path);
    if(SaveConfig()!=ERROR_SUCCESS){Message::ShowError(_hwnd,L"Unable to save the imported catalogue.");return false;}
    return true;
}

void MainDlg::PreviewBundle(const std::wstring& path)
{
    if(_injecting)return;
    _pendingZip.clear();_pendingPreview={};
    if(_worker.joinable())_worker.join();
    _bundleBusy=3;_bundlePercent=0;SetBusy(true);_view.overlay=0;
    _loaderStatus=L"Verifying ZIP...";DrawPrisme();
    try
    {
        _worker=std::thread([this,path]()
        {
            auto result=std::make_unique<BundleResult>();result->previewing=true;result->path=path;
            try{result->success=CatalogueArchive::Preview(path,result->preview,result->error,[this](int n){PostMessageW(_hwnd,WM_BUNDLE_PROGRESS,n,0);});}
            catch(...){result->error=L"Unable to read the ZIP preview.";}
            if(PostMessageW(_hwnd,WM_BUNDLE_DONE,0,reinterpret_cast<LPARAM>(result.get())))result.release();
        });
    }
    catch(...){_bundleBusy=0;SetBusy(false);_loaderStatus=L"Unable to prepare the preview.";DrawPrisme();}
}

void MainDlg::BeginBundle(bool importing,const std::wstring& path)
{
    if(_injecting)return;
    if(!importing && _imagePaths.empty()){_loaderStatus=L"Add a cheat before exporting.";DrawPrisme();return;}
    SaveConfig();const auto snapshot=importing?_profileMgr.config():_exportCatalogue;
    if(_worker.joinable())_worker.join();
    _bundleBusy=importing?2:1;_bundlePercent=0;SetBusy(true);_view.overlay=0;
    _loaderStatus=importing?L"Importing ZIP...":L"Exporting ZIP...";DrawPrisme();
    const auto root=PrismeDataDirectory()+L"\\Catalogues";
    const auto identity=importing?_pendingPreview.identity:std::string();
    try
    {
        _worker=std::thread([this,importing,path,snapshot,root,identity]()
        {
            auto result=std::make_unique<BundleResult>();result->importing=importing;result->path=path;
            try
            {
                auto progress=[this](int n){PostMessageW(_hwnd,WM_BUNDLE_PROGRESS,static_cast<WPARAM>(n),0);};
                if(importing)
                {
                    result->success=CatalogueArchive::Import(path,root,result->profile,result->error,progress,identity);
                    if(result->success)
                    {
                        ProfileMgr incoming,merged;
                        result->success=incoming.Load(result->profile);
                        if(result->success)
                        {
                            CatalogueModel::Normalize(incoming.config());
                            for(const auto& group:incoming.config().versionGroups)for(const auto& version:group.items)
                                if(!ValidateCheat(version.path,false)){result->success=false;result->error=L"The ZIP contains an unsupported DLL version: "+std::filesystem::path(version.path).filename().wstring();}
                        }
                        if(result->success)result->success=CatalogueArchive::Merge(snapshot,incoming.config(),merged.config(),result->added,result->duplicates,result->error,&result->versionsAdded);
                        if(result->success && (result->added || result->versionsAdded))result->success=merged.Save(result->profile);
                        if(!result->success || (!result->added && !result->versionsAdded)){CatalogueArchive::DiscardImport(result->profile,root);result->profile.clear();}
                        if(!result->success && result->error.empty())result->error=L"Unable to install the catalogue.";
                    }
                }
                else result->success=CatalogueArchive::Export(path,snapshot,result->error,progress);
            }
            catch(...){result->success=false;result->error=L"The ZIP operation failed.";}
            if(PostMessageW(_hwnd,WM_BUNDLE_DONE,0,reinterpret_cast<LPARAM>(result.get())))result.release();
        });
    }
    catch(...){_bundleBusy=0;SetBusy(false);_loaderStatus=L"Unable to start the ZIP operation.";DrawPrisme();}
}


void MainDlg::BeginReportExport(const std::wstring& path,std::vector<CatalogueArchive::Report> reports)
{
    if(_injecting)return;if(_worker.joinable())_worker.join();
    _bundleBusy=4;_bundlePercent=0;SetBusy(true);_view.overlay=0;_loaderStatus=L"Exporting diagnostic reports...";DrawPrisme();
    try
    {
        _worker=std::thread([this,path,reports=std::move(reports)]()
        {
            auto result=std::make_unique<BundleResult>();result->path=path;
            try{result->success=CatalogueArchive::ExportReports(path,reports,result->error,[this](int n){PostMessageW(_hwnd,WM_BUNDLE_PROGRESS,n,0);});}
            catch(...){result->error=L"Diagnostic export failed.";}
            if(PostMessageW(_hwnd,WM_BUNDLE_DONE,0,reinterpret_cast<LPARAM>(result.get())))result.release();
        });
    }
    catch(...){_bundleBusy=0;SetBusy(false);_loaderStatus=L"Unable to start diagnostic export.";DrawPrisme();}
}

INT_PTR MainDlg::OnBundleProgress(HWND,UINT,WPARAM percent,LPARAM)
{
    _bundlePercent=static_cast<int>(percent);
    _loaderStatus=std::wstring(_bundleBusy==3?L"Verifying ZIP: ":(_bundleBusy==2?L"Importing ZIP: ":L"Exporting ZIP: "))+std::to_wstring(_bundlePercent)+L" %";
    DrawPrisme();return TRUE;
}

INT_PTR MainDlg::OnBundleDone(HWND,UINT,WPARAM,LPARAM value)
{
    std::unique_ptr<BundleResult> result(reinterpret_cast<BundleResult*>(value));
    if(_worker.joinable())_worker.join();_bundleBusy=0;SetBusy(false);
    if(result->success && result->previewing)
    {
        _pendingZip=result->path;_pendingPreview=std::move(result->preview);
        _view.previewItems.clear();
        for(const auto& item:_pendingPreview.items)
        {
            auto detail=item.dllName+L"  \u00b7  "+std::to_wstring((item.bytes+1023)/1024)+L" KB";
            detail+=L"  \u00b7  "+std::to_wstring(item.versions)+L" version(s)";
            if(item.artwork)detail+=L"  \u00b7  image";
            _view.previewItems.push_back({item.title,detail});
        }
        _view.previewName=std::filesystem::path(result->path).filename().wstring();
        _view.previewSummary=std::to_wstring(_pendingPreview.items.size())+L" cheat(s)  \u00b7  "+
            std::to_wstring((_pendingPreview.totalBytes+1023)/1024)+L" KB total";
        _view.panelFirst=0;_view.overlay=5;_loaderStatus=L"Preview ready. Confirm to import.";
    }
    else if(result->success && result->importing)
    {
        if((!result->added && !result->versionsAdded) || ApplyCatalogue(result->profile))
            _loaderStatus=std::to_wstring(result->added)+L" cheat(s), "+std::to_wstring(result->versionsAdded)+L" version(s) added; "+std::to_wstring(result->duplicates)+L" duplicate(s).";
        else _loaderStatus=L"The ZIP was not applied.";
    }
    else if(result->success)
    {
        _lastExportZip=result->path;_loaderStatus=L"Complete ZIP ready to share.";
    }
    else { _loaderStatus=L"ZIP operation failed.";Message::ShowError(_hwnd,result->error); }
    DrawPrisme();return TRUE;
}

void MainDlg::RevealZip()
{
    if(_lastExportZip.empty())return;
    PIDLIST_ABSOLUTE item=nullptr;
    if(SUCCEEDED(SHParseDisplayName(_lastExportZip.c_str(),nullptr,&item,0,nullptr)))
    {SHOpenFolderAndSelectItems(item,0,nullptr,0);CoTaskMemFree(item);}
}
