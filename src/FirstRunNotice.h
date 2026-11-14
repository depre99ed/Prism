#pragma once
#include <windows.h>
#include <filesystem>
namespace FirstRunNotice
{
    enum class Result {Show,Seen,Error};
    // Stable across executable names and version updates; scoped to the user's data folder.
    inline Result Claim(const std::wstring& root)
    {
        std::error_code error;std::filesystem::create_directories(root,error);if(error)return Result::Error;
        auto marker=std::filesystem::path(root)/L"FirstRunNotice.seen";
        HANDLE file=CreateFileW(marker.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_EXISTS?Result::Seen:Result::Error;
        const char content[]="Prism first-run notice shown\r\n";DWORD written=0;
        bool okay=WriteFile(file,content,sizeof(content)-1,&written,nullptr) && written==sizeof(content)-1 && FlushFileBuffers(file);
        CloseHandle(file);if(!okay){DeleteFileW(marker.c_str());return Result::Error;}return Result::Show;
    }
}
