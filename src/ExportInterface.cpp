#include "MainDlg.h"
#include <filesystem>

void MainDlg::PrepareExport(bool diagnostics)
{
    if(_injecting)return;
    _exportDiagnostics=diagnostics;_view.exportDiagnostics=diagnostics;_view.exportOption=true;
    _view.selectionItems.clear();_view.selectionChecked.clear();_exportRows.clear();_exportReports.clear();
    if(diagnostics)
    {
        _exportReports=_errors.entries;
        for(const auto& report:_exportReports)_view.selectionItems.push_back({report.title,report.time});
    }
    else
    {
        SyncCatalogue();SaveConfig();_exportSnapshot=_profileMgr.config();CatalogueModel::Normalize(_exportSnapshot);
        for(size_t i=0;i<_exportSnapshot.versionGroups.size();++i)
            for(size_t v=0;v<_exportSnapshot.versionGroups[i].items.size();++v)
            {
                const auto& item=_exportSnapshot.versionGroups[i].items[v];
                _exportRows.push_back({(int)i,(int)v});
                _view.selectionItems.push_back({_cheatTitles[i]+L"  \u00b7  "+item.label,std::filesystem::path(item.path).filename().wstring()});
            }
    }
    if(_view.selectionItems.empty()){Message::ShowInfo(_hwnd,diagnostics?L"No diagnostic reports to export.":L"Add a cheat before exporting.");return;}
    _view.selectionChecked.assign(_view.selectionItems.size(),true);_view.panelFirst=0;_view.overlay=9;
}

void MainDlg::ConfirmExport()
{
    if(std::find(_view.selectionChecked.begin(),_view.selectionChecked.end(),true)==_view.selectionChecked.end())
    {Message::ShowInfo(_hwnd,L"Select at least one item to export.");return;}
    std::wstring path;
    if(!OpenSaveDialog(L"ZIP archive (*.zip)\0*.zip\0",1,path,true,L"zip"))return;
    if(_exportDiagnostics)
    {
        std::vector<CatalogueArchive::Report> reports;
        for(size_t i=0;i<_exportReports.size() && i<_view.selectionChecked.size();++i)if(_view.selectionChecked[i])
            reports.push_back({Diagnostics::Utf8(_view.exportOption?Diagnostics::HidePaths(_exportReports[i].report):_exportReports[i].report)});
        BeginReportExport(path,std::move(reports));
    }
    else
    {
        _exportCatalogue=CatalogueModel::Select(_exportSnapshot,_exportRows,_view.selectionChecked,_view.exportOption);
        BeginBundle(false,path);
    }
}
