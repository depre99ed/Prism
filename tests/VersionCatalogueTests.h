#pragma once
#include "../src/CatalogueModel.h"
#include "../src/VersionStore.h"
#include "../src/RestartFlow.h"
static std::vector<std::pair<std::string,std::string>> ReadZipEntries(const std::filesystem::path& path)
{
    mz_zip_archive zip={};FILE* file=_wfopen(path.c_str(),L"rb");require(file!=nullptr,"open test ZIP");
    require(mz_zip_reader_init_cfile(&zip,file,0,0)!=0,"read test ZIP");std::vector<std::pair<std::string,std::string>> entries;
    for(mz_uint i=0;i<mz_zip_reader_get_num_files(&zip);++i)
    {
        mz_zip_archive_file_stat stat={};mz_zip_reader_file_stat(&zip,i,&stat);size_t size=0;
        void* bytes=mz_zip_reader_extract_to_heap(&zip,i,&size,0);require(bytes!=nullptr,"read ZIP payload");
        entries.push_back({stat.m_filename,std::string((char*)bytes,size)});mz_free(bytes);
    }
    mz_zip_reader_end(&zip);fclose(file);return entries;
}
static void TestVersionCatalogue(const std::wstring& folder)
{
    namespace fs=std::filesystem;auto root=fs::path(folder)/L"versions"/VersionStore::Id();fs::create_directories(root);
    auto source=root/L"source/mod.dll";Put(source,"FIRST VERSION");
    auto first=VersionStore::Snapshot(source.wstring(),(root/L"stored").wstring(),L"v1 \u00e9t\u00e9 & stable");
    Put(source,"SECOND VERSION");auto second=VersionStore::Snapshot(source.wstring(),(root/L"stored").wstring(),L"v2");
    require(first.path!=second.path && Bytes(first.path)=="FIRST VERSION" && Bytes(second.path)=="SECOND VERSION","same source filename preserves immutable versions");
    require(first.sha256!=second.sha256 && Bytes(source)=="SECOND VERSION","version copy does not alter source");
    ProfileMgr profile;auto& cfg=profile.config();cfg.images={second.path};cfg.imageTitles={L"Aurora"};auto artwork=root/L"image.png";Put(artwork,"OPTIONAL ARTWORK");cfg.imageArtwork={artwork.wstring()};cfg.selectedCheat=0;
    cfg.versionGroups={{L"stable-cheat-id",{first,second},1}};
    auto path=root/L"versions.xpr";require(profile.Save(path.wstring()),"save version catalogue");ProfileMgr restored;require(restored.Load(path.wstring()),"load version catalogue");
    require(restored.config().versionGroups[0].items.size()==2 && restored.config().versionGroups[0].active==1 && restored.config().images[0]==second.path,"version catalogue retains active choice");
    require(restored.config().versionGroups[0].items[0].label==first.label,"version labels preserve Unicode and XML entities");
    auto selected=CatalogueModel::Select(cfg,{{0,0},{0,1}},{true,false},false);
    require(selected.images[0]==first.path && selected.versionGroups[0].items.size()==1 && selected.versionGroups[0].active==0 && selected.imageArtwork[0].empty(),"export selection retains only chosen version with valid active fallback");
    std::wstring error,installed;auto zip=root/L"all.zip";
    require(CatalogueArchive::Export(zip.wstring(),cfg,error),"export all versions");CatalogueArchive::PreviewInfo preview;
    require(CatalogueArchive::Preview(zip.wstring(),preview,error) && preview.items[0].versions==2,"ZIP preview counts versions");
    require(CatalogueArchive::Import(zip.wstring(),(root/L"imports").wstring(),installed,error),"import versioned ZIP");
    ProfileMgr imported;require(imported.Load(installed),"load installed versions");auto importedCfg=imported.config();
    require(importedCfg.versionGroups[0].items.size()==2 && importedCfg.versionGroups[0].active==1,"portable versions retain selection");
    require(Bytes(importedCfg.versionGroups[0].items[0].path)=="FIRST VERSION","old version bytes survive ZIP roundtrip");
    Put(source,"THIRD VERSION");auto third=VersionStore::Snapshot(source.wstring(),(root/L"stored").wstring(),L"v3");
    auto incoming=cfg;incoming.images={third.path};incoming.versionGroups={{L"stable-cheat-id",{third},0}};
    ProfileMgr::ConfigData merged;int added=0,duplicates=0,versions=0;
    require(CatalogueArchive::Merge(importedCfg,incoming,merged,added,duplicates,error,&versions) && added==0 && versions==1 && merged.versionGroups[0].items.size()==3,"new versions merge into matching cheat");
    require(merged.images[0]==importedCfg.images[0] && merged.versionGroups[0].active==1,"import does not silently switch current version");
    auto after=merged;require(CatalogueArchive::Merge(after,incoming,merged,added,duplicates,error,&versions) && versions==0 && duplicates==1,"duplicate version content is skipped");
    auto distinct=cfg;distinct.versionGroups[0].id=L"different-cheat-id";distinct.imageTitles[0]=L"Another cheat";
    require(CatalogueArchive::Merge(importedCfg,distinct,merged,added,duplicates,error,&versions) && added==1 && merged.images.size()==2,"distinct versioned cheat identities are not merged merely because bytes match");
    auto broken=importedCfg;broken.versionGroups[0].items[0].path=(root/L"missing.dll").wstring();
    auto repair=CatalogueModel::Select(cfg,{{0,0},{0,1}},{true,false},false);
    require(CatalogueArchive::Merge(broken,repair,merged,added,duplicates,error,&versions) && versions==1 && merged.versionGroups[0].items[0].path==first.path,"reimport repairs a missing stored version");
    auto only=root/L"selected.zip";require(CatalogueArchive::Export(only.wstring(),selected,error),"export selected version");
    std::string combined;for(const auto& item:ReadZipEntries(only)){combined+=item.second;require(item.first.find("Images/")!=0,"unselected artwork absent from ZIP");}
    require(combined.find("FIRST VERSION")!=combined.npos && combined.find("SECOND VERSION")==combined.npos,"unselected DLL bytes absent from ZIP");
    require(combined.find(Diagnostics::Utf8(root.wstring()))==combined.npos,"versioned ZIP contains no absolute local paths");
    const auto raw=L"DLL: C:\\Users\\Alice\\Desktop\\my cheat.dll\nCode: 0xC0000135\nTrace: 'D:/private/build.dll'\nUNC: \\\\server\\private\\file.dll";
    require(Diagnostics::HidePaths(L"URL: steam://rungameid/730")==L"URL: steam://rungameid/730","path hiding preserves Steam URI");
    auto hidden=Diagnostics::HidePaths(raw);require(hidden.find(L"Alice")==hidden.npos && hidden.find(L"server")==hidden.npos && hidden.find(L"0xC0000135")!=hidden.npos,"diagnostic export hides paths and retains error code");
    auto reports=root/L"reports.zip";require(CatalogueArchive::ExportReports(reports.wstring(),{{Diagnostics::Utf8(hidden)}},error),"export selected diagnostic report");
    auto entries=ReadZipEntries(reports);int reportCount=0;for(const auto& item:entries)if(item.first.find("Reports/")==0){++reportCount;require(item.second.find("Alice")==item.second.npos && item.second.find("0xC0000135")!=item.second.npos,"diagnostic ZIP contains redacted report");}
    require(reportCount==1,"only selected reports are exported");
    // A genuine legacy v1 ZIP contains a flat manifest and no version groups.
    auto legacyManifest=root/L"legacy.xml";Put(legacyManifest,"<XenosConfig><imagePath>DLLs/1/legacy.dll</imagePath><cheatTitle>Legacy</cheatTitle></XenosConfig>");
    auto legacyDll=root/L"legacy.dll";Put(legacyDll,"LEGACY DLL");
    auto a=VersionStore::Digest(legacyManifest.wstring()),b=VersionStore::Digest(legacyDll.wstring());
    std::string sums=Diagnostics::Utf8(a)+"  catalogue.xpr\n"+Diagnostics::Utf8(b)+"  DLLs/1/legacy.dll\n";
    auto legacyZip=root/L"legacy.zip";ZipFixture(legacyZip,{{"bundle-format.txt","PRISME-CATALOGUE/1\n"},{"catalogue.xpr",Bytes(legacyManifest)},{"DLLs/1/legacy.dll",Bytes(legacyDll)},{"checksums.sha256",sums}});
    require(CatalogueArchive::Preview(legacyZip.wstring(),preview,error) && preview.items[0].versions==1,"legacy v1 ZIP remains readable");
}
static void TestRestartFlow()
{
    RestartFlow flow;using E=RestartFlow::Event;flow.Start(1000);
    require(flow.Observe(5000,false)==E::None,"never relaunch while old process lives");
    require(flow.Observe(6000,true)==E::None && flow.Observe(7499,true)==E::None,"allow Steam cleanup after normal close");
    require(flow.Observe(7500,true)==E::Launch && flow.Observe(9000,true)==E::None,"fresh launch occurs once after close");
    flow.Start(0);require(flow.Observe(30000,false)==E::Timeout && flow.Observe(35000,true)==E::None,"close timeout never falls through to loading");
    flow.Start(0);flow.Cancel();require(flow.Observe(10000,true)==E::None,"cancellation prevents relaunch");
}
