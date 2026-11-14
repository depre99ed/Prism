#pragma once
#include "../src/CatalogueArchive.h"
#include "../vendor/miniz/miniz.h"
#include <filesystem>
namespace testfs=std::filesystem;
static std::string Bytes(const testfs::path& path)
{std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
static void Put(const testfs::path& path,const std::string& data)
{testfs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(data.data(),data.size());}
static void ZipFixture(const testfs::path& path,const std::vector<std::pair<std::string,std::string>>& entries)
{
    mz_zip_archive zip={};FILE* file=_wfopen(path.c_str(),L"wb");require(file!=nullptr,"fixture open");
    require(mz_zip_writer_init_cfile(&zip,file,0)!=0,"fixture ZIP init");
    for(size_t i=0;i<entries.size();++i)
    {
        // The writer rejects some malicious names itself. Forge them after a valid write.
        const auto& item=entries[i];std::string safe(item.first.size(),static_cast<char>('A'+i%26));
        require(mz_zip_writer_add_mem(&zip,safe.c_str(),item.second.data(),item.second.size(),6)!=0,"fixture ZIP entry");
    }
    require(mz_zip_writer_finalize_archive(&zip)!=0,"fixture ZIP finalize");mz_zip_writer_end(&zip);fclose(file);
    std::string bytes=Bytes(path);mz_zip_archive reader={};file=_wfopen(path.c_str(),L"rb");
    require(mz_zip_reader_init_cfile(&reader,file,0,0)!=0,"fixture readback");
    for(size_t i=0;i<entries.size();++i)
    {
        mz_zip_archive_file_stat stat={};require(mz_zip_reader_file_stat(&reader,static_cast<mz_uint>(i),&stat)!=0,"fixture offsets");
        bytes.replace(static_cast<size_t>(stat.m_local_header_ofs+30),entries[i].first.size(),entries[i].first);
        bytes.replace(static_cast<size_t>(reader.m_central_directory_file_ofs+stat.m_central_dir_ofs+46),entries[i].first.size(),entries[i].first);
    }
    mz_zip_reader_end(&reader);fclose(file);Put(path,bytes);
}
static void TestArchives(const std::wstring& folder)
{
    GUID run;CoCreateGuid(&run);wchar_t runId[40];StringFromGUID2(run,runId,40);
    testfs::path root=testfs::path(folder)/L"archive-tests"/runId;testfs::create_directories(root);
    const auto a=root/L"origin/A/mod.dll",b=root/L"origin/B/mod.dll",c=root/L"origin/\u00e9t\u00e9.dll",image=root/L"origin/\u00e9t\u00e9.png";
    Put(a,"MZ-FIRST-CONTENT");Put(b,"MZ-SECOND-CONTENT");Put(c,"MZ-UNICODE-CONTENT");Put(image,"PNG-OPTIONAL-IMAGE");
    ProfileMgr::ConfigData cfg;cfg.images={a.wstring(),b.wstring(),c.wstring()};
    cfg.imageTitles={L"Mon cheat \u00e9t\u00e9 & test",L"Second",L"Troisi\u00e8me"};cfg.imageArtwork={image.wstring(),L"",L""};cfg.selectedCheat=1;
    std::wstring error,installed;const auto archive=root/L"partage \u00e9t\u00e9.zip";
    int percent=0;require(CatalogueArchive::Export(archive.wstring(),cfg,error,[&](int p){percent=p;}),"ZIP export");require(percent==100,"export progress");
    std::string originalZip=Bytes(archive);
    require(originalZip.find("origin/A")==std::string::npos,"source absolute paths not stored as filenames");
    // Source files may be moved or absent on the recipient's PC.
    testfs::remove(a);testfs::remove(b);testfs::remove(c);testfs::remove(image);
    const auto imports=root/L"recipient";
    CatalogueArchive::PreviewInfo preview;
    require(CatalogueArchive::Preview(archive.wstring(),preview,error),"preview without source files");
    require(preview.items.size()==3 && preview.items[0].title==cfg.imageTitles[0],"preview lists titles and all entries");
    require(preview.items[0].dllName==L"mod.dll" && preview.items[0].bytes==16 && preview.items[0].artwork && !preview.items[1].artwork,"preview DLL bytes, names and optional artwork");
    require(!testfs::exists(imports) || testfs::is_empty(imports),"preview installs no catalogue assets");
    require(!preview.identity.empty(),"preview binds confirmation to manifest and checksums");
    require(!CatalogueArchive::Import(archive.wstring(),imports.wstring(),installed,error,{},"different-manifest"),"changed archive requires another preview");
    require(!testfs::exists(imports) || testfs::is_empty(imports),"changed archive creates no installed assets");
    require(CatalogueArchive::Import(archive.wstring(),imports.wstring(),installed,error),"ZIP import without original files");
    ProfileMgr loaded;require(loaded.Load(installed),"installed profile loads");const auto& got=loaded.config();
    require(got.images.size()==3 && got.images[0]!=got.images[1],"same DLL basenames stay distinct");
    require(Bytes(got.images[2])=="MZ-UNICODE-CONTENT" && testfs::path(got.images[2]).filename()==L"\u00e9t\u00e9.dll","Unicode ZIP member preserved");
    require(Bytes(got.images[0])=="MZ-FIRST-CONTENT" && Bytes(got.images[1])=="MZ-SECOND-CONTENT","DLL bytes preserved");
    require(Bytes(got.imageArtwork[0])=="PNG-OPTIONAL-IMAGE" && got.imageArtwork[1].empty(),"optional image preserved");
    require(got.imageTitles==cfg.imageTitles && got.selectedCheat==1,"metadata and selection preserved");
    ProfileMgr::ConfigData merged;int added=0,duplicates=0;
    require(CatalogueArchive::Merge(got,got,merged,added,duplicates,error) && added==0 && duplicates==3 && merged.images.size()==3,"reimport does not duplicate DLL content");
    ProfileMgr::ConfigData empty;
    require(CatalogueArchive::Merge(empty,got,merged,added,duplicates,error) && added==3 && merged.selectedCheat==1,"empty catalogue adopts imported selection");
    // Missing input must never replace an existing good ZIP with a partial archive.
    require(!CatalogueArchive::Export(archive.wstring(),cfg,error) && Bytes(archive)==originalZip,"failed export preserves existing ZIP");
    auto malicious=root/L"traversal.zip";
    ZipFixture(malicious,{{"../escape.dll","bad"}});
    std::wstring unused;
    require(!CatalogueArchive::Import(malicious.wstring(),imports.wstring(),unused,error) && !testfs::exists(root/L"escape.dll"),"ZIP traversal refused");
    for(const auto& name:{"/absolute.dll","C:/escape.dll","DLLs/1/CON.dll","DLLs/../escape.dll","DLLs\\escape.dll"})
    {
        ZipFixture(malicious,{{name,"bad"}});
        require(!CatalogueArchive::Import(malicious.wstring(),imports.wstring(),unused,error),"unsafe ZIP filename refused");
    }
    ZipFixture(malicious,{{"DLLs/a.dll","first"},{"dlls/A.dll","second"}});
    require(!CatalogueArchive::Import(malicious.wstring(),imports.wstring(),unused,error),"case-insensitive duplicate names refused");
    // Rebuild a ZIP with a valid CRC but changed DLL content; SHA-256 must detect it.
    mz_zip_archive reader={};FILE* file=_wfopen(archive.c_str(),L"rb");require(mz_zip_reader_init_cfile(&reader,file,0,0)!=0,"read exported ZIP");
    std::vector<std::pair<std::string,std::string>> entries;
    for(mz_uint i=0;i<mz_zip_reader_get_num_files(&reader);++i)
    {
        mz_zip_archive_file_stat stat={};mz_zip_reader_file_stat(&reader,i,&stat);size_t size=0;
        void* data=mz_zip_reader_extract_to_heap(&reader,i,&size,0);require(data!=nullptr,"read fixture entry");
        std::string bytes(static_cast<char*>(data),size);mz_free(data);
        if(std::string(stat.m_filename).find("DLLs/1/")==0)bytes+="TAMPERED";
        entries.push_back({stat.m_filename,bytes});
    }
    mz_zip_reader_end(&reader);fclose(file);ZipFixture(malicious,entries);
    auto before=std::distance(testfs::directory_iterator(imports),testfs::directory_iterator{});
    require(!CatalogueArchive::Import(malicious.wstring(),imports.wstring(),unused,error),"tampered content refused");
    require(!CatalogueArchive::Preview(malicious.wstring(),preview,error) && preview.items.empty(),"preview rejects bad SHA before confirmation");
    auto after=std::distance(testfs::directory_iterator(imports),testfs::directory_iterator{});
    require(before==after,"failed import staging cleaned");
    CatalogueArchive::DiscardImport(installed,imports.wstring());
    require(!testfs::exists(installed),"owned duplicate staging can be discarded");
}
