#pragma once
#include "ProfileMgr.h"
#include <functional>

namespace CatalogueArchive
{
    using Progress=std::function<void(int)>;
    struct PreviewItem {std::wstring title,dllName;uint64_t bytes=0;bool artwork=false;unsigned versions=1;};
    struct PreviewInfo {std::vector<PreviewItem> items;uint64_t totalBytes=0;unsigned fileCount=0;std::string identity;};
    bool Preview(const std::wstring& zip,PreviewInfo& info,std::wstring& error,const Progress& progress={});
    // Both operations copy data only. They never load or execute a DLL.
    bool Export(const std::wstring& zip,const ProfileMgr::ConfigData& catalogue,
        std::wstring& error,const Progress& progress={});
    bool Import(const std::wstring& zip,const std::wstring& importsRoot,
        std::wstring& installedProfile,std::wstring& error,const Progress& progress={},const std::string& expectedIdentity={});
    bool Merge(const ProfileMgr::ConfigData& current,const ProfileMgr::ConfigData& incoming,
        ProfileMgr::ConfigData& result,int& added,int& duplicates,std::wstring& error,int* versionsAdded=nullptr);
    struct Report {std::string text;};
    bool ExportReports(const std::wstring& zip,const std::vector<Report>& reports,std::wstring& error,const Progress& progress={});
    void DiscardImport(const std::wstring& installedProfile,const std::wstring& importsRoot);
}
