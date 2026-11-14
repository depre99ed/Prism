#pragma once
#include <windows.h>
#include <vector>
#include <set>
#include <utility>
namespace NormalClose
{
    struct Request {const std::vector<std::pair<HANDLE,DWORD>>& targets;std::set<HWND>& sent;DWORD error=0;};
    inline BOOL CALLBACK Window(HWND window,LPARAM value)
    {
        auto& request=*reinterpret_cast<Request*>(value);if(request.sent.count(window) || GetWindow(window,GW_OWNER))return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        for(const auto& target:request.targets)
            if(pid==target.second && GetProcessId(target.first)==pid && WaitForSingleObject(target.first,0)==WAIT_TIMEOUT)
            {
                if(PostMessageW(window,WM_CLOSE,0,0))request.sent.insert(window);else request.error=GetLastError();break;
            }
        return TRUE;
    }
}
