#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>

inline std::wstring PrismeDataDirectory()
{
    wchar_t path[MAX_PATH]={};
    if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,path)))
        return L".";
    std::wstring dir=std::wstring(path)+L"\\Prisme";
    CreateDirectoryW(dir.c_str(),nullptr);
    return dir;
}

inline bool PrismeStandaloneRuntime()
{
    const auto folder=PrismeDataDirectory()+L"\\Runtime";
    CreateDirectoryW(folder.c_str(),nullptr);
    const struct {int id;const wchar_t* name;} libraries[]={{160,L"msdia140.dll"},{161,L"symsrv.dll"}};
    for(const auto& library:libraries)
    {
        auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(library.id),L"RUNTIME");
        if(!resource)return false;
        DWORD size=SizeofResource(nullptr,resource);
        auto bytes=LockResource(LoadResource(nullptr,resource));
        const auto path=folder+L"\\"+library.name;
        HANDLE existing=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        bool same=false;
        if(existing!=INVALID_HANDLE_VALUE)
        {
            if(GetFileSize(existing,nullptr)==size)
            {
                std::vector<BYTE> buffer(size);DWORD read=0;
                same=ReadFile(existing,buffer.data(),size,&read,nullptr)&&read==size&&memcmp(buffer.data(),bytes,size)==0;
            }
            CloseHandle(existing);
        }
        if(same)continue;
        HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;bool okay=WriteFile(file,bytes,size,&written,nullptr)&&written==size;CloseHandle(file);
        if(!okay)return false;
    }
    // Loaded before constructing the unchanged process engine; no COM registration.
    return LoadLibraryExW((folder+L"\\symsrv.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH)!=nullptr &&
        LoadLibraryExW((folder+L"\\msdia140.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH)!=nullptr;
}
