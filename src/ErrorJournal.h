#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <string>

namespace Diagnostics
{
    inline std::wstring HidePaths(const std::wstring& text)
    {
        std::wstring out;
        for(size_t i=0;i<text.size();)
        {
            bool boundary=i==0 || (!iswalnum(text[i-1]) && text[i-1]!=L'_');
            bool drive=boundary && i+2<text.size() && iswalpha(text[i]) && text[i+1]==L':' && (text[i+2]==L'\\' || text[i+2]==L'/');
            bool unc=i+1<text.size() && text[i]==L'\\' && text[i+1]==L'\\';
            if(drive || unc)
            {
                size_t end=i;while(end<text.size() && text[end]!=L'\r' && text[end]!=L'\n' && text[end]!=L'\t' && text[end]!=L'"' && text[end]!=L'\'' && text[end]!=L'<' && text[end]!=L'>')++end;
                out+=L"[path hidden]";i=end;
            }
            else out+=text[i++];
        }
        return out;
    }
    struct Entry {std::wstring time,title,report;};
    inline std::string Utf8(const std::wstring& text)
    {
        int n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),nullptr,0,nullptr,nullptr);
        std::string result(n,0);if(n)WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),result.data(),n,nullptr,nullptr);return result;
    }
    inline std::wstring Wide(const std::string& text,UINT page=CP_UTF8)
    {
        int n=MultiByteToWideChar(page,0,text.data(),(int)text.size(),nullptr,0);
        std::wstring result(n,0);if(n)MultiByteToWideChar(page,0,text.data(),(int)text.size(),result.data(),n);return result;
    }
    inline std::wstring Hex(unsigned long code)
    {wchar_t text[16];swprintf_s(text,L"0x%08lX",code);return text;}
    inline std::wstring SystemMessage(unsigned long code,bool native)
    {
        wchar_t* buffer=nullptr;
        DWORD flags=FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS;
        auto module=native?GetModuleHandleW(L"ntdll.dll"):nullptr;
        if(native)flags|=FORMAT_MESSAGE_FROM_HMODULE;else flags|=FORMAT_MESSAGE_FROM_SYSTEM;
        DWORD n=FormatMessageW(flags,module,code,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),reinterpret_cast<wchar_t*>(&buffer),0,nullptr);
        std::wstring result=n?std::wstring(buffer,n):L"Windows does not provide a description for this code.";
        if(buffer)LocalFree(buffer);
        while(!result.empty() && iswspace(result.back()))result.pop_back();return result;
    }
    inline std::wstring Cause(unsigned long code,bool native)
    {
        if(!code)return L"No system error code was provided for this incident. Only the observations below describe what happened.";
        if((native && code==0xC0000022ul) || (!native && code==ERROR_ACCESS_DENIED))
            return L"Confirmed: Windows denied the requested access. The code does not identify the rule that caused the denial.";
        if((native && code==0xC000007Bul) || (!native && code==ERROR_BAD_EXE_FORMAT))
            return L"Confirmed: incompatible or invalid image format. A different architecture or incompatible dependency is a possible explanation.";
        if((native && code==0xC0000135ul) || (!native && code==ERROR_MOD_NOT_FOUND))
            return L"Confirmed: a module was not found. It may be the DLL or one of its dependencies; this code alone does not identify its name.";
        if((native && code==0xC0000139ul) || (!native && code==ERROR_PROC_NOT_FOUND))
            return L"Confirmed: a required entry point was not found. An incompatible dependency version is possible, but is not confirmed by this code alone.";
        if(native && code==0xC0000142ul)
            return L"Confirmed: DLL initialization failed. The code does not distinguish an initialization refusal from an internal error.";
        if(native && (code==0xC000010Aul || code==0xC000000Bul))
            return L"Confirmed: the target process is terminating or is no longer valid.";
        if(!native && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND))
            return L"Confirmed: the file or its directory does not exist at this location.";
        if(!native && code==WAIT_TIMEOUT)
            return L"Confirmed: the wait timed out. The last observed readiness criteria are included in the context below.";
        if((native && (code==0xC0000017ul || code==0xC000009Aul)) || (!native && code==ERROR_NOT_ENOUGH_MEMORY))
            return L"Confirmed: Windows reported insufficient memory or resources.";
        return L"This code does not provide the exact cause. Review the stage, Windows message and traces below. No additional cause is inferred automatically.";
    }
    inline Entry Make(const std::wstring& title,const std::wstring& stage,unsigned long code,bool native,
        unsigned long pid,unsigned long long identity,const std::wstring& path,const std::wstring& facts,const std::vector<std::string>& trace={})
    {
        SYSTEMTIME now;GetLocalTime(&now);wchar_t stamp[40];
        swprintf_s(stamp,L"%04u-%02u-%02u %02u:%02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
        Entry entry{stamp,title,L""};std::wostringstream out;
        bool substituted=std::any_of(trace.begin(),trace.end(),[](const std::string& line){return line.find("Invalid failure status")!=line.npos;});
        out<<title<<L"\n"<<stamp<<L" (local time)\n\n";
        if(substituted)out<<L"Exact cause unknown: the engine replaced an unspecified status with the code below. This code does not prove that a dependency is missing.";
        else out<<Cause(code,native);
        out<<L"\n\n\u00c9tape : "<<stage;
        if(code)out<<L"\nCode "<<(native?L"NTSTATUS":L"Win32")<<L" : "<<Hex(code)<<L" ("<<code<<L")\nWindows code description: "<<SystemMessage(code,native);
        else out<<L"\nSystem code: not provided";
        if(native && code)
        {
            using Convert=ULONG (WINAPI*)(LONG);
            auto convert=reinterpret_cast<Convert>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlNtStatusToDosError"));
            auto mapped=convert?convert(static_cast<LONG>(code)):ERROR_MR_MID_NOT_FOUND;
            if(mapped!=ERROR_MR_MID_NOT_FOUND)out<<L"\nWin32 equivalent (conversion): "<<mapped<<L" / "<<Hex(mapped)<<L"\n"<<SystemMessage(mapped,false);
        }
        out<<L"\n\nProcess: cs2.exe\nPID : "<<pid<<L"\nCreation identity (FILETIME): "<<identity<<L"\nDLL : "<<path;
        std::error_code ec;
        if(!path.empty())
        {
            auto size=std::filesystem::file_size(path,ec);
            if(!ec)out<<L"\nTaille : "<<size<<L" bytes";else out<<L"\nFile was inaccessible at the time of this report.";
        }
        out<<L"\nRequested mode: standard loading, native x64 DLL\nApp: Prism\nVersion: 1.1.1.1.2.7\n";
        if(!facts.empty())out<<L"\nObservations:\n"<<facts<<L"\n";
        out<<L"\nTraces from this attempt:\n";
        if(trace.empty())out<<L"No engine traces were captured for this stage.\n";
        else for(const auto& line:trace)out<<Wide(line,CP_ACP)<<L"\n";
        entry.report=out.str();if(entry.report.size()>60000)entry.report=entry.report.substr(0,60000)+L"\n[Report truncated]\n";
        return entry;
    }
    inline std::vector<std::wstring> Lines(const std::wstring& report,size_t width=44)
    {
        std::vector<std::wstring> lines;std::wistringstream input(report);std::wstring line;
        while(std::getline(input,line))
        {
            if(!line.empty() && line.back()==L'\r')line.pop_back();
            while(line.size()>width)
            {
                size_t split=line.rfind(L' ',width);if(split==line.npos || split<width/3)split=width;
                lines.push_back(line.substr(0,split));line.erase(0,split);if(!line.empty() && line.front()==L' ')line.erase(0,1);
            }
            lines.push_back(line);
        }
        return lines;
    }
    class Journal
    {
        std::filesystem::path _root;
    public:
        std::vector<Entry> entries;
        void Load(const std::wstring& root)
        {
            _root=root;entries.clear();std::error_code ec;
            if(!std::filesystem::is_directory(_root,ec))return;
            std::vector<std::filesystem::path> files;
            for(const auto& item:std::filesystem::directory_iterator(_root,ec))if(item.is_regular_file(ec) && item.path().extension()==L".txt")files.push_back(item.path());
            std::sort(files.rbegin(),files.rend());
            for(const auto& file:files)
            {
                if(entries.size()>=100)break;
                auto size=std::filesystem::file_size(file,ec);if(ec || size>256*1024)continue;
                std::ifstream input(file,std::ios::binary);std::string time,title,report;
                if(!std::getline(input,time) || !std::getline(input,title))continue;
                report.assign(std::istreambuf_iterator<char>(input),{});
                if(time.size()!=19 || report.empty())continue;
                entries.push_back({Wide(time),Wide(title),Wide(report)});
            }
        }
        bool Add(Entry entry)
        {
            entries.insert(entries.begin(),entry);if(entries.size()>100)entries.resize(100);
            try
            {
                std::filesystem::create_directories(_root);GUID id;CoCreateGuid(&id);wchar_t suffix[40];StringFromGUID2(id,suffix,40);
                FILETIME now;GetSystemTimeAsFileTime(&now);auto ticks=(static_cast<unsigned long long>(now.dwHighDateTime)<<32)|now.dwLowDateTime;
                auto file=_root/(std::to_wstring(ticks)+L"-"+suffix+L".txt"),part=file;part+=L".tmp";
                {std::ofstream out(part,std::ios::binary);out<<Utf8(entry.time)<<'\n'<<Utf8(entry.title)<<'\n'<<Utf8(entry.report);out.flush();if(!out.good())return false;}
                if(!MoveFileExW(part.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
                std::vector<std::filesystem::path> files;
                for(const auto& item:std::filesystem::directory_iterator(_root))if(item.is_regular_file() && item.path().extension()==L".txt")files.push_back(item.path());
                std::sort(files.rbegin(),files.rend());for(size_t i=100;i<files.size();++i)std::filesystem::remove(files[i]);
                return true;
            }
            catch(...){return false;}
        }
    };
}
