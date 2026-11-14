#include "CatalogueArchive.h"
#include "CatalogueModel.h"
#include "../vendor/miniz/miniz.h"
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <map>
#include <sstream>
#include <iomanip>
#include <memory>
#pragma comment(lib,"bcrypt.lib")
namespace fs=std::filesystem;

namespace
{
    constexpr uint64_t MaxFile=512ull*1024*1024,MaxTotal=2ull*1024*1024*1024;
    struct Failure {std::wstring text;};
    void Need(bool test,const std::wstring& error){if(!test)throw Failure{error};}
    std::string Utf8(const std::wstring& s)
    {
        int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
        Need(n>0 || s.empty(),L"Invalid filename.");std::string out(n,0);
        if(n)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),&out[0],n,nullptr,nullptr);return out;
    }
    std::wstring Wide(const std::string& s)
    {
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
        Need(n>0,L"Invalid ZIP entry name.");std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),&out[0],n);return out;
    }
    std::wstring Key(const std::wstring& name)
    {
        std::wstring out(name.size(),0);LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,name.data(),static_cast<int>(name.size()),&out[0],static_cast<int>(out.size()),nullptr,nullptr,0);return out;
    }
    bool SafeRelative(const std::wstring& name)
    {
        if(name.empty() || name.size()>220 || name.front()==L'/' || name.find(L'\\')!=name.npos ||
            name.find(L':')!=name.npos || name.find_first_of(L"\r\n\t")!=name.npos)return false;
        std::wistringstream stream(name);std::wstring part;
        while(std::getline(stream,part,L'/'))
        {
            if(part.empty() || part==L"." || part==L".." || part.back()==L'.' || part.back()==L' ')return false;
            for(wchar_t c:part)if(c<32 || c==L'<' || c==L'>' || c==L'"' || c==L'|' || c==L'?' || c==L'*')return false;
            auto stem=Key(part.substr(0,part.find(L'.')));
            if(stem==L"con" || stem==L"prn" || stem==L"aux" || stem==L"nul" ||
                (stem.size()==4 && (stem.substr(0,3)==L"com" || stem.substr(0,3)==L"lpt") && stem[3]>=L'1' && stem[3]<=L'9'))return false;
        }
        return name.back()!=L'/';
    }
    struct Zip
    {
        mz_zip_archive zip={};FILE* file=nullptr;bool reader=false,opened=false;
        ~Zip(){Close();}
        void Close(){if(opened){if(reader)mz_zip_reader_end(&zip);else mz_zip_writer_end(&zip);opened=false;}if(file){fclose(file);file=nullptr;}}
        void Open(const std::wstring& path,bool read)
        {
            reader=read;file=_wfopen(path.c_str(),read?L"rb":L"wb");Need(file!=nullptr,L"Unable to open the ZIP file.");
            if(read)Need(fs::file_size(path)<=MaxTotal,L"The ZIP exceeds 2 GB.");
            opened=read?mz_zip_reader_init_cfile(&zip,file,0,0)!=0:mz_zip_writer_init_cfile(&zip,file,0)!=0;
            Need(opened,L"The ZIP is invalid or could not be created.");
        }
    };
    std::vector<unsigned char> Read(const fs::path& path,uint64_t limit=MaxFile)
    {
        Need(fs::is_regular_file(path),L"File not found: "+path.filename().wstring());
        auto n=fs::file_size(path);Need(n<=limit,L"File too large: "+path.filename().wstring());
        std::vector<unsigned char> bytes(static_cast<size_t>(n));std::ifstream in(path,std::ios::binary);
        Need(in.good(),L"Unable to read: "+path.filename().wstring());
        if(n){in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(n));Need(in.good(),L"Incomplete read: "+path.filename().wstring());}return bytes;
    }
    std::string Hash(const std::vector<unsigned char>& data)
    {
        BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
        Need(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,L"SHA-256 is unavailable.");
        DWORD size=0,got=0;BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&got,0);
        std::vector<unsigned char> object(size);unsigned char result[32]={};
        bool okay=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
        if(okay && !data.empty())okay=BCryptHashData(hash,const_cast<PUCHAR>(data.data()),static_cast<ULONG>(data.size()),0)>=0;
        if(okay)okay=BCryptFinishHash(hash,result,32,0)>=0;
        if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);Need(okay,L"Unable to calculate SHA-256.");
        std::ostringstream out;for(unsigned char byte:result)out<<std::hex<<std::setfill('0')<<std::setw(2)<<static_cast<unsigned>(byte);return out.str();
    }
    fs::path NewDirectory(const fs::path& root)
    {
        fs::create_directories(root);GUID id;Need(SUCCEEDED(CoCreateGuid(&id)),L"Unable to create a temporary directory.");
        wchar_t name[64];StringFromGUID2(id,name,64);fs::path path=root/name;
        Need(fs::create_directory(path),L"Unable to create the catalogue directory.");return path;
    }
    void Cleanup(const fs::path& path,const fs::path& root)
    {
        if(path.empty())return;
        try
        {
            // Never recursively remove a root or a directory outside the owned staging parent.
            if(fs::weakly_canonical(path).parent_path()==fs::weakly_canonical(root) &&
                !(GetFileAttributesW(path.c_str())&FILE_ATTRIBUTE_REPARSE_POINT))fs::remove_all(path);
        }catch(...){}
    }
    void WriteNew(const fs::path& path,const std::vector<unsigned char>& bytes,const fs::path& stage)
    {
        Need(fs::absolute(path).lexically_normal().wstring().find(fs::absolute(stage).lexically_normal().wstring()+L"\\")==0,L"Invalid extraction path.");
        fs::path folder=stage;
        for(const auto& segment:path.parent_path().lexically_relative(stage))
        {
            folder/=segment;fs::create_directory(folder);
            Need(!(GetFileAttributesW(folder.c_str())&FILE_ATTRIBUTE_REPARSE_POINT),L"Directory link rejected.");
        }
        HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        Need(file!=INVALID_HANDLE_VALUE,L"Unable to extract: "+path.filename().wstring());
        DWORD written=0;bool okay=bytes.empty() || (WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size());CloseHandle(file);
        Need(okay,L"Incomplete write.");
    }
    std::vector<unsigned char> Extract(Zip& zip,mz_uint index,uint64_t limit)
    {
        mz_zip_archive_file_stat stat={};Need(mz_zip_reader_file_stat(&zip.zip,index,&stat)!=0,L"Invalid ZIP entry.");
        Need(stat.m_uncomp_size<=limit,L"A ZIP entry is too large.");
        std::vector<unsigned char> bytes(static_cast<size_t>(stat.m_uncomp_size));unsigned char empty=0;
        Need(mz_zip_reader_extract_to_mem(&zip.zip,index,bytes.empty()?&empty:bytes.data(),bytes.size(),0)!=0,L"The ZIP is corrupted.");return bytes;
    }
}

bool CatalogueArchive::Export(const std::wstring& target,const ProfileMgr::ConfigData& source,std::wstring& error,const Progress& progress)
{
    fs::path stage,stageRoot;std::wstring temporary;
    try
    {
        Need(Key(fs::path(target).extension().wstring())==L".zip",L"Choose a filename ending in .zip.");
        Need(!source.images.empty(),L"The catalogue is empty.");Need(source.images.size()<=1000,L"The catalogue contains too many entries.");
        stageRoot=fs::temp_directory_path()/L"Prisme-export";stage=NewDirectory(stageRoot);
        temporary=target+L".prisme-partial";Zip zip;zip.Open(temporary,false);
        auto normalized=source;CatalogueModel::Normalize(normalized);
        size_t versionCount=0;for(const auto& group:normalized.versionGroups){Need(group.items.size()<=100,L"A cheat contains too many versions.");versionCount+=group.items.size();}
        Need(versionCount<=1000,L"The catalogue contains too many DLL versions.");
        ProfileMgr portable;auto& cfg=portable.config();cfg.selectedCheat=source.selectedCheat;
        std::ostringstream hashes;uint64_t total=0;
        auto add=[&](const std::wstring& member,const std::vector<unsigned char>& bytes)
        {
            Need(SafeRelative(member),L"Filename incompatible with ZIP.");total+=bytes.size();Need(total<=MaxTotal,L"The catalogue exceeds 2 GB.");
            auto name=Utf8(member);Need(mz_zip_writer_add_mem(&zip.zip,name.c_str(),bytes.data(),bytes.size(),6)!=0,L"Unable to add a file to the ZIP.");
            hashes<<Hash(bytes)<<"  "<<name<<"\n";
        };
        for(size_t i=0;i<source.images.size();++i)
        {
            fs::path dll=source.images[i];std::wstring prefix=std::to_wstring(i+1);
            auto group=normalized.versionGroups[i];
            for(size_t v=0;v<group.items.size();++v)
            {
                auto& record=group.items[v];auto bytes=Read(record.path);Need(!bytes.empty(),L"Empty DLL version.");
                auto member=L"DLLs/"+prefix+L"/"+std::to_wstring(v+1)+L"/"+fs::path(record.path).filename().wstring();
                add(member,bytes);record.path=member;record.sha256=Wide(Hash(bytes));
            }
            cfg.images.push_back(group.items[group.active].path);cfg.versionGroups.push_back(std::move(group));
            cfg.imageTitles.push_back(i<source.imageTitles.size()?source.imageTitles[i]:dll.stem().wstring());
            cfg.imageEnabled.push_back(static_cast<int>(i)==source.selectedCheat);
            std::wstring image;
            if(i<source.imageArtwork.size() && !source.imageArtwork[i].empty())
            {
                fs::path original=source.imageArtwork[i];image=L"Images/"+prefix+original.extension().wstring();add(image,Read(original,16*1024*1024));
            }
            cfg.imageArtwork.push_back(image);if(progress)progress(static_cast<int>((i+1)*85/source.images.size()));
        }
        Need(portable.Save((stage/L"catalogue.xpr").wstring()),L"Unable to prepare the catalogue.");
        add(L"catalogue.xpr",Read(stage/L"catalogue.xpr",4*1024*1024));
        std::string sums=hashes.str();Need(mz_zip_writer_add_mem(&zip.zip,"checksums.sha256",sums.data(),sums.size(),6)!=0,L"Unable to add checksums.");
        const char* readme="Prism catalogue ZIP v1\r\nOpen Prism > Options > Import ZIP / catalogue.\r\nAll catalogue DLLs, titles and optional pictures are included.\r\nImport only copies files; it does not load any DLL.\r\n";
        const char* format="PRISME-CATALOGUE/2\n";
        Need(mz_zip_writer_add_mem(&zip.zip,"bundle-format.txt",format,strlen(format),6)!=0,L"Unable to add format information.");
        Need(mz_zip_writer_add_mem(&zip.zip,"README.txt",readme,strlen(readme),6)!=0,L"Unable to complete the ZIP.");
        Need(mz_zip_writer_finalize_archive(&zip.zip)!=0,L"Unable to finalize the ZIP.");zip.Close();
        Need(MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,L"Unable to save the final ZIP.");temporary.clear();
        Cleanup(stage,stageRoot);if(progress)progress(100);return true;
    }
    catch(const Failure& ex){error=ex.text;}
    catch(const std::exception&){error=L"Export failed: inaccessible file or insufficient disk space.";}
    if(!temporary.empty())DeleteFileW(temporary.c_str());Cleanup(stage,stageRoot);return false;
}

static bool ReadBundle(const std::wstring& file,const std::wstring& root,std::wstring& installedProfile,std::wstring& error,
    const CatalogueArchive::Progress& progress,CatalogueArchive::PreviewInfo* preview,const std::string& expectedIdentity)
{
    fs::path stage;installedProfile.clear();
    try
    {
        Zip zip;zip.Open(file,true);auto count=mz_zip_reader_get_num_files(&zip.zip);Need(count>0 && count<=4096,L"Invalid ZIP entry count.");
        std::map<std::wstring,mz_uint> members;std::map<std::wstring,std::wstring> actualNames;uint64_t total=0;
        for(mz_uint i=0;i<count;++i)
        {
            mz_zip_archive_file_stat stat={};Need(mz_zip_reader_file_stat(&zip.zip,i,&stat)!=0,L"Unreadable ZIP.");
            std::wstring name=Wide(stat.m_filename);
            bool directory=stat.m_is_directory!=0;if(directory && !name.empty() && name.back()==L'/')name.pop_back();
            Need(SafeRelative(name),L"The ZIP contains a disallowed path.");
            Need(!stat.m_is_encrypted && ((stat.m_external_attr>>16)&0170000)!=0120000,L"Encrypted entry or link rejected.");
            auto key=Key(name);Need(!members.count(key),L"The ZIP contains duplicate filenames.");
            members[key]=i;actualNames[key]=name;total+=stat.m_uncomp_size;
            Need(stat.m_uncomp_size<=MaxFile && total<=MaxTotal,L"The unpacked ZIP is too large.");
        }
        Need(members.count(L"catalogue.xpr") && members.count(L"checksums.sha256"),L"This ZIP is not a Prism catalogue.");
        Need(members.count(L"bundle-format.txt")!=0,L"The ZIP format is not recognized.");
        auto format=Extract(zip,members.at(L"bundle-format.txt"),128);
        Need((std::string(format.begin(),format.end())=="PRISME-CATALOGUE/1\n" || std::string(format.begin(),format.end())=="PRISME-CATALOGUE/2\n"),L"Unsupported ZIP catalogue version.");
        auto sumBytes=Extract(zip,members.at(L"checksums.sha256"),512*1024);
        std::istringstream lines(std::string(sumBytes.begin(),sumBytes.end()));std::string line;std::map<std::wstring,std::string> hashes;
        while(std::getline(lines,line))
        {
            if(!line.empty() && line.back()=='\r')line.pop_back();if(line.empty())continue;
            Need(line.size()>66 && line.substr(64,2)=="  " && line.substr(0,64).find_first_not_of("0123456789abcdef")==std::string::npos,L"Invalid ZIP checksums.");
            auto name=Wide(line.substr(66));Need(SafeRelative(name),L"Invalid checksum path.");auto key=Key(name);
            Need(!hashes.count(key),L"Duplicate checksum.");hashes[key]=line.substr(0,64);
        }
        auto verified=[&](const std::wstring& name,uint64_t limit)
        {
            auto key=Key(name);Need(SafeRelative(name) && members.count(key) && hashes.count(key),L"Missing file in ZIP: "+name);
            auto bytes=Extract(zip,members.at(key),limit);Need(Hash(bytes)==hashes.at(key),L"Checksum mismatch: "+name);return bytes;
        };
        auto manifest=verified(L"catalogue.xpr",4*1024*1024);
        const auto identity=Hash(manifest)+Hash(sumBytes);
        Need(expectedIdentity.empty() || identity==expectedIdentity,L"The ZIP changed since the preview. Open a new preview.");
        if(preview){preview->identity=identity;preview->totalBytes=total;preview->fileCount=count;}
        stage=NewDirectory(root);
        WriteNew(stage/L"catalogue.xpr",manifest,stage);ProfileMgr profile;
        Need(profile.Load((stage/L"catalogue.xpr").wstring()),L"The ZIP catalogue is invalid.");auto& cfg=profile.config();
        Need(!cfg.images.empty() && cfg.images.size()<=1000,L"The ZIP catalogue is empty or too large.");
        std::set<std::wstring> extracted={L"catalogue.xpr"};
        auto install=[&](std::wstring& name,bool image)
        {
            if(name.empty()){Need(image,L"A DLL has no path.");return;}
            auto key=Key(name);Need(SafeRelative(name) && key.find(image?L"images/":L"dlls/")==0,L"Invalid asset path.");
            if(!image)Need(Key(fs::path(name).extension().wstring())==L".dll",L"The catalogue references a file that is not a DLL.");
            if(!extracted.count(key)){auto bytes=verified(name,image?16*1024*1024:MaxFile);if(!preview)WriteNew(stage/fs::path(name),bytes,stage);extracted.insert(key);}
            if(!preview)name=(stage/fs::path(name)).wstring();
        };
        cfg.imageArtwork.resize(cfg.images.size());CatalogueModel::Normalize(cfg);
        for(size_t i=0;i<cfg.images.size();++i)
        {
            auto& group=cfg.versionGroups[i];uint64_t bytes=0;
            const auto displayName=fs::path(cfg.images[i]).filename().wstring();
            for(auto& version:group.items)
            {
                auto key=Key(version.path);Need(members.count(key)!=0,L"DLL version missing from ZIP.");
                mz_zip_archive_file_stat stat={};Need(mz_zip_reader_file_stat(&zip.zip,members.at(key),&stat)!=0,L"Unreadable DLL version.");bytes+=stat.m_uncomp_size;
                install(version.path,false);
                auto expected=Wide(hashes.at(key));Need(version.sha256.empty() || version.sha256==expected,L"Version metadata checksum mismatch.");version.sha256=expected;
            }
            cfg.images[i]=group.items[group.active].path;install(cfg.imageArtwork[i],true);
            if(preview)preview->items.push_back({i<cfg.imageTitles.size()&&!cfg.imageTitles[i].empty()?cfg.imageTitles[i]:fs::path(displayName).stem().wstring(),
                displayName,bytes,!cfg.imageArtwork[i].empty(),(unsigned)group.items.size()});
            if(progress)progress(static_cast<int>((i+1)*90/cfg.images.size()));
        }
        if(preview){Cleanup(stage,fs::path(root));if(progress)progress(100);return true;}
        // Imported settings cannot enable an operation. Only catalogue data is retained.
        ProfileMgr clean;clean.config().images=cfg.images;clean.config().imageTitles=cfg.imageTitles;
        clean.config().imageArtwork=cfg.imageArtwork;clean.config().selectedCheat=cfg.selectedCheat;
        clean.config().imageEnabled=cfg.imageEnabled;clean.config().versionGroups=cfg.versionGroups;
        Need(clean.Save((stage/L"installed.xpr").wstring()),L"Unable to install the catalogue.");
        installedProfile=(stage/L"installed.xpr").wstring();if(progress)progress(100);return true;
    }
    catch(const Failure& ex){error=ex.text;}
    catch(const std::exception&){error=L"Import failed: invalid file or insufficient disk space.";}
    Cleanup(stage,fs::path(root));return false;
}

bool CatalogueArchive::Import(const std::wstring& file,const std::wstring& root,std::wstring& profile,std::wstring& error,
    const Progress& progress,const std::string& identity)
{return ReadBundle(file,root,profile,error,progress,nullptr,identity);}

bool CatalogueArchive::Preview(const std::wstring& file,PreviewInfo& info,std::wstring& error,const Progress& progress)
{
    info={};std::wstring ignored;PreviewInfo candidate;
    bool success=ReadBundle(file,(fs::temp_directory_path()/L"Prisme-preview").wstring(),ignored,error,progress,&candidate,{});
    if(success)info=std::move(candidate);return success;
}

bool CatalogueArchive::Merge(const ProfileMgr::ConfigData& current,const ProfileMgr::ConfigData& incoming,
    ProfileMgr::ConfigData& result,int& added,int& duplicates,std::wstring& error,int* versionsAdded)
{
    try
    {
        result=current;CatalogueModel::Normalize(result);auto other=incoming;CatalogueModel::Normalize(other);
        added=duplicates=0;int newVersions=0;
        result.imageTitles.resize(result.images.size());result.imageArtwork.resize(result.images.size());result.imageEnabled.resize(result.images.size());
        std::map<std::wstring,int> groupsById;std::map<std::string,int> known;
        std::vector<std::map<std::string,size_t>> versionsByHash(result.versionGroups.size());
        auto digestOf=[&](const ProfileMgr::DllVersion& version)
        {try{return Hash(Read(version.path));}catch(...){return version.sha256.empty()?std::string():Utf8(version.sha256);}};
        for(size_t i=0;i<result.versionGroups.size();++i)
        {
            const auto& group=result.versionGroups[i];if(!group.id.empty())groupsById[group.id]=(int)i;
            for(size_t v=0;v<group.items.size();++v){auto digest=digestOf(group.items[v]);if(!digest.empty()){known[digest]=(int)i;versionsByHash[i][digest]=v;}}
        }
        for(size_t i=0;i<other.images.size();++i)
        {
            auto group=other.versionGroups[i];std::vector<std::string> digests;
            for(const auto& version:group.items)digests.push_back(Hash(Read(version.path)));
            int at=-1;if(!group.id.empty() && groupsById.count(group.id))at=groupsById.at(group.id);
            if(at<0)for(const auto& digest:digests)if(known.count(digest))
            {
                int candidate=known.at(digest);
                if(group.id.empty() || result.versionGroups[candidate].id.empty()){at=candidate;break;}
            }
            if(at<0)
            {
                at=(int)result.images.size();result.images.push_back(group.items[group.active].path);result.versionGroups.push_back(group);versionsByHash.emplace_back();for(size_t v=0;v<digests.size();++v)versionsByHash.back()[digests[v]]=v;
                result.imageTitles.push_back(i<other.imageTitles.size()?other.imageTitles[i]:L"");result.imageArtwork.push_back(i<other.imageArtwork.size()?other.imageArtwork[i]:L"");
                result.imageEnabled.push_back(false);++added;newVersions+=(int)group.items.size();
                if(current.images.empty() && (int)i==incoming.selectedCheat)result.selectedCheat=at;
            }
            else
            {
                auto& destination=result.versionGroups[at];
                for(size_t v=0;v<group.items.size();++v)
                {
                    int duplicate=versionsByHash[at].count(digests[v])?(int)versionsByHash[at].at(digests[v]):-1;
                    if(duplicate>=0)
                    {
                        if(!fs::is_regular_file(destination.items[duplicate].path)){destination.items[duplicate].path=group.items[v].path;++newVersions;}
                        else ++duplicates;
                    }
                    else {Need(destination.items.size()<100,L"A cheat cannot contain more than 100 versions.");versionsByHash[at][digests[v]]=destination.items.size();destination.items.push_back(group.items[v]);++newVersions;}
                }
                result.images[at]=destination.items[destination.active].path;
            }
            if(!group.id.empty())groupsById[group.id]=at;for(const auto& digest:digests)known[digest]=at;
        }
        if(result.selectedCheat<0 && !result.images.empty())result.selectedCheat=0;
        size_t count=0;for(const auto& group:result.versionGroups)count+=group.items.size();
        Need(result.images.size()<=1000 && count<=1000,L"The merged catalogue would exceed 1000 DLL versions.");
        if(versionsAdded)*versionsAdded=newVersions;return true;
    }
    catch(const Failure& ex){error=ex.text;}
    catch(const std::exception&){error=L"Unable to merge the catalogues.";}
    return false;
}

bool CatalogueArchive::ExportReports(const std::wstring& target,const std::vector<Report>& reports,std::wstring& error,const Progress& progress)
{
    std::wstring temporary;
    try
    {
        Need(!reports.empty() && reports.size()<=100,L"Choose between 1 and 100 reports.");
        temporary=target+L".prisme-partial";Zip zip;zip.Open(temporary,false);std::ostringstream checksums;
        for(size_t i=0;i<reports.size();++i)
        {
            const auto& text=reports[i].text;Need(text.size()<=1024*1024,L"Diagnostic report too large.");
            auto member="Reports/"+std::to_string(i+1)+".txt";
            Need(mz_zip_writer_add_mem(&zip.zip,member.c_str(),text.data(),text.size(),6)!=0,L"Unable to write the report.");
            std::vector<unsigned char> bytes(text.begin(),text.end());checksums<<Hash(bytes)<<"  "<<member<<"\n";
            if(progress)progress((int)((i+1)*90/reports.size()));
        }
        const char format[]="PRISM-DIAGNOSTICS/1\n";auto sums=checksums.str();
        Need(mz_zip_writer_add_mem(&zip.zip,"bundle-format.txt",format,sizeof(format)-1,6)!=0,L"Unable to write ZIP metadata.");
        Need(mz_zip_writer_add_mem(&zip.zip,"checksums.sha256",sums.data(),sums.size(),6)!=0,L"Unable to write checksums.");
        Need(mz_zip_writer_finalize_archive(&zip.zip)!=0,L"Unable to finalize diagnostics ZIP.");zip.Close();
        Need(MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,L"Unable to save diagnostics ZIP.");
        temporary.clear();if(progress)progress(100);return true;
    }
    catch(const Failure& ex){error=ex.text;}catch(...){error=L"Unable to export the reports.";}
    if(!temporary.empty())DeleteFileW(temporary.c_str());return false;
}

void CatalogueArchive::DiscardImport(const std::wstring& installedProfile,const std::wstring& importsRoot)
{
    fs::path profile=installedProfile;
    if(profile.filename()==L"installed.xpr")Cleanup(profile.parent_path(),fs::path(importsRoot));
}
