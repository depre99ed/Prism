#pragma once
#include "ProfileMgr.h"
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#pragma comment(lib,"bcrypt.lib")
namespace VersionStore
{
    inline std::wstring Id()
    {GUID id;wchar_t text[40];if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("GUID unavailable");StringFromGUID2(id,text,40);return text;}
    inline std::wstring Digest(const std::wstring& path)
    {
        std::ifstream file(std::filesystem::path(path),std::ios::binary);if(!file)throw std::runtime_error("DLL unreadable");
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::wstring result;
        if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA-256 unavailable");
        bool okay=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
        unsigned char buffer[65536],digest[32];
        while(okay && file){file.read(reinterpret_cast<char*>(buffer),sizeof(buffer));auto n=file.gcount();if(n)okay=BCryptHashData(hash,buffer,(ULONG)n,0)>=0;}
        okay=okay && file.eof();if(okay)okay=BCryptFinishHash(hash,digest,32,0)>=0;
        if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
        if(!okay)throw std::runtime_error("DLL hashing failed");
        std::wostringstream text;for(auto byte:digest)text<<std::hex<<std::setw(2)<<std::setfill(L'0')<<(int)byte;return text.str();
    }
    inline ProfileMgr::DllVersion Snapshot(const std::wstring& source,const std::wstring& root,const std::wstring& label)
    {
        namespace fs=std::filesystem;
        if(!fs::is_regular_file(source) || fs::file_size(source)>512ull*1024*1024)throw std::runtime_error("DLL missing or too large");
        ProfileMgr::DllVersion record;record.id=Id();record.label=label;
        fs::create_directories(root);
        if(GetFileAttributesW(root.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Version root link rejected");
        fs::path stage=fs::path(root)/record.id;fs::create_directory(stage);
        if(GetFileAttributesW(stage.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Version directory link rejected");
        auto destination=stage/fs::path(source).filename();
        try
        {
            auto original=Digest(source);
            if(!CopyFileW(source.c_str(),destination.c_str(),TRUE))throw std::runtime_error("Version copy failed");
            record.path=destination.wstring();record.sha256=Digest(record.path);
            if(record.sha256!=original)throw std::runtime_error("DLL changed while copying");
            return record;
        }
        catch(...){DeleteFileW(destination.c_str());RemoveDirectoryW(stage.c_str());throw;}
    }
}
