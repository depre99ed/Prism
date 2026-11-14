#pragma once
#include "../src/NormalClose.h"
static unsigned testCloseCount=0;
static LRESULT CALLBACK CloseFixtureProc(HWND window,UINT message,WPARAM key,LPARAM data)
{
    if(message==WM_CLOSE){++testCloseCount;return 0;}return DefWindowProcW(window,message,key,data);
}
static void TestNormalClose()
{
    WNDCLASSW type={};type.lpfnWndProc=CloseFixtureProc;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"PrismCloseFixture";
    RegisterClassW(&type);auto window=CreateWindowW(type.lpszClassName,L"Close fixture",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,type.hInstance,nullptr);
    require(window!=nullptr,"normal-close test owns its window");
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,GetCurrentProcessId());require(process!=nullptr,"normal-close test pins its own process");
    std::vector<std::pair<HANDLE,DWORD>> targets={{process,GetCurrentProcessId()}};std::set<HWND> sent;NormalClose::Request request={targets,sent};
    testCloseCount=0;NormalClose::Window(window,reinterpret_cast<LPARAM>(&request));NormalClose::Window(window,reinterpret_cast<LPARAM>(&request));
    MSG message;while(PeekMessageW(&message,window,0,0,PM_REMOVE))DispatchMessageW(&message);
    require(testCloseCount==1 && request.error==0,"normal close posts WM_CLOSE once per window");
    require(WaitForSingleObject(process,0)==WAIT_TIMEOUT,"normal close does not terminate its process forcibly");
    DestroyWindow(window);CloseHandle(process);UnregisterClassW(type.lpszClassName,type.hInstance);
}
