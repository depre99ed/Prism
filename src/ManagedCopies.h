#pragma once
#include <windows.h>
#include <filesystem>
#include <vector>
#include <string>
namespace ManagedCopies
{
    enum class Result {Removed,Missing,Referenced,External,Failed};
    inline std::wstring Normal(std::wstring text)
    {
        if(text.compare(0,8,L"\\\\?\\UNC\\")==0)text=L"\\\\"+text.substr(8);
        else if(text.compare(0,4,L"\\\\?\\")==0)text.erase(0,4);
        return text;
    }
    inline bool Equal(const std::wstring& left,const std::wstring& right)
    {return CompareStringOrdinal(left.c_str(),(int)left.size(),right.c_str(),(int)right.size(),TRUE)==CSTR_EQUAL;}
    inline bool Prefix(const std::wstring& value,const std::wstring& prefix)
    {return value.size()>=prefix.size() && CompareStringOrdinal(value.c_str(),(int)prefix.size(),prefix.c_str(),(int)prefix.size(),TRUE)==CSTR_EQUAL;}
    inline bool Owned(const std::wstring& path,const std::wstring& dataRoot)
    {
        auto versions=dataRoot+L"\\Versions\\",catalogues=dataRoot+L"\\Catalogues\\";
        if(Prefix(path,versions))
        {
            auto rest=path.substr(versions.size());auto separator=rest.find(L'\\');
            return separator!=rest.npos && separator>0 && separator+1<rest.size() && rest.find(L'\\',separator+1)==rest.npos;
        }
        if(Prefix(path,catalogues))
        {
            auto rest=path.substr(catalogues.size());auto separator=rest.find(L'\\');
            return separator!=rest.npos && separator>0 && Prefix(rest.substr(separator+1),L"DLLs\\");
        }
        return false;
    }
    // Used only after an explicit catalogue removal has been saved successfully.
    // The opened handle pins the file; the final resolved path prevents junction escapes.
    inline Result Remove(const std::wstring& path,const std::wstring& dataRoot,const std::vector<std::wstring>& references)
    {
        namespace fs=std::filesystem;std::error_code error;
        auto candidate=Normal(fs::weakly_canonical(path,error).wstring());if(error)return Result::Failed;
        auto root=Normal(fs::weakly_canonical(dataRoot,error).wstring());if(error || !Owned(candidate,root))return Result::External;
        for(const auto& reference:references)
        {
            error.clear();auto retained=Normal(fs::weakly_canonical(reference,error).wstring());
            if(!error && Equal(candidate,retained))return Result::Referenced;
        }
        HANDLE file=CreateFileW(path.c_str(),DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(file==INVALID_HANDLE_VALUE)
        {auto code=GetLastError();return code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND?Result::Missing:Result::Failed;}
        FILE_ATTRIBUTE_TAG_INFO attributes={};
        if(!GetFileInformationByHandleEx(file,FileAttributeTagInfo,&attributes,sizeof(attributes)) ||
            (attributes.FileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))
        {CloseHandle(file);return Result::External;}
        DWORD size=GetFinalPathNameByHandleW(file,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
        std::vector<wchar_t> buffer(size+1);DWORD count=size?GetFinalPathNameByHandleW(file,buffer.data(),(DWORD)buffer.size(),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS):0;
        if(!count || count>=buffer.size()){CloseHandle(file);return Result::Failed;}
        auto final=Normal(std::wstring(buffer.data(),count));
        if(!Owned(final,root)){CloseHandle(file);return Result::External;}
        for(const auto& reference:references)
        {
            error.clear();auto retained=Normal(fs::weakly_canonical(reference,error).wstring());
            if(!error && Equal(final,retained)){CloseHandle(file);return Result::Referenced;}
        }
        FILE_DISPOSITION_INFO disposition={TRUE};bool removed=SetFileInformationByHandle(file,FileDispositionInfo,&disposition,sizeof(disposition))!=FALSE;
        CloseHandle(file);
        if(removed)RemoveDirectoryW(fs::path(final).parent_path().c_str());
        return removed?Result::Removed:Result::Failed;
    }
}
