#include "MainDlg.h"
#include <tlhelp32.h>
#include <shellapi.h>
#include "NormalClose.h"

namespace
{
    struct ProcessState {DWORD pid=0;uint64_t identity=0;bool ready=false,client=false,engine=false,visible=false;DWORD error=0,snapshotError=0;};
    struct WindowProbe {DWORD pid;bool visible=false;};
    BOOL CALLBACK FindGameWindow(HWND h,LPARAM value)
    {
        auto p=reinterpret_cast<WindowProbe*>(value);DWORD pid=0;GetWindowThreadProcessId(h,&pid);
        if(pid==p->pid && IsWindowVisible(h) && GetWindow(h,GW_OWNER)==nullptr && !IsHungAppWindow(h))
        {p->visible=true;return FALSE;}
        return TRUE;
    }
    ProcessState ProbeCS2(DWORD preferred)
    {
        ProcessState result;
        auto candidates=blackbone::Process::EnumByName(L"cs2.exe");
        if(candidates.empty())return result;
        DWORD pid=std::find(candidates.begin(),candidates.end(),preferred)!=candidates.end()?preferred:candidates.front();
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
        if(!process){result.error=GetLastError();return result;}
        FILETIME created,exit,kernel,user;DWORD code=0;
        bool queried=GetProcessTimes(process,&created,&exit,&kernel,&user)!=FALSE;
        if(!queried)result.error=GetLastError();
        if(queried && !GetExitCodeProcess(process,&code)){queried=false;result.error=GetLastError();}
        if(queried && code==STILL_ACTIVE)
        {
            result.pid=pid;result.identity=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
            bool client=false,engine=false;
            HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
            if(snapshot!=INVALID_HANDLE_VALUE)
            {
                MODULEENTRY32W module={};module.dwSize=sizeof(module);
                if(Module32FirstW(snapshot,&module))do
                {
                    client|=_wcsicmp(module.szModule,L"client.dll")==0;
                    engine|=_wcsicmp(module.szModule,L"engine2.dll")==0;
                }while(Module32NextW(snapshot,&module));
                CloseHandle(snapshot);
            }
            else result.snapshotError=GetLastError();
            result.client=client;result.engine=engine;
            WindowProbe windows={pid,false};EnumWindows(FindGameWindow,reinterpret_cast<LPARAM>(&windows));
            result.visible=windows.visible;result.ready=client && engine && windows.visible;
        }
        CloseHandle(process);return result;
    }
    std::wstring Observations(const ProcessState& state)
    {
        std::wstring facts=L"client.dll observed: "+std::wstring(state.client?L"yes":L"no")+
            L"\nengine2.dll observed: "+std::wstring(state.engine?L"yes":L"no")+
            L"\nVisible and responsive window: "+std::wstring(state.visible?L"yes":L"no");
        if(state.snapshotError)facts+=L"\nModule enumeration: "+Diagnostics::Hex(state.snapshotError)+L" "+Diagnostics::SystemMessage(state.snapshotError,false);
        if(state.error)facts+=L"\nProcess query: "+Diagnostics::Hex(state.error)+L" "+Diagnostics::SystemMessage(state.error,false);
        return facts;
    }
}

void MainDlg::StartLoader()
{
    if(_injecting || _selectedCheat<0 || _selectedCheat>=(int)_imagePaths.size())return;
    try {PreserveVersions(_selectedCheat);}
    catch(...){Message::ShowError(_hwnd,L"Unable to preserve the selected DLL version. No loading was attempted.");return;}
    _loadingPath=_imagePaths[_selectedCheat];
    _attemptTrace.clear();_attemptFacts.clear();_view.errorAvailable=false;
    blackbone::pe::PEImage check;
    auto validation=check.Load(_loadingPath);
    std::wstring validationFacts;
    if(NT_SUCCESS(validation))validationFacts=L"Detected architecture: "+std::wstring(check.mType()==blackbone::mt_mod64?L"x64":(check.mType()==blackbone::mt_mod32?L"x86":L"other"))+
        L"\nDetected type: "+std::wstring(check.isExe()?L"executable":(check.pureIL()?L"DLL .NET pure IL":L"Native DLL"));
    if(NT_SUCCESS(validation) && (check.isExe() || check.pureIL() || check.mType()!=blackbone::mt_mod64))validation=STATUS_INVALID_IMAGE_FORMAT;
    check.Release();
    if(!NT_SUCCESS(validation)){RecordError(L"DLL cannot be loaded",L"Reading and validating the DLL",validation,true,validationFacts);DrawPrisme();return;}
    auto process=ProbeCS2(_lastAutoPid);
    if(process.error!=ERROR_SUCCESS)
    {RecordError(L"Process access denied",L"Opening the process",process.error,false,Observations(process),_lastAutoPid==MAXDWORD?0:_lastAutoPid);DrawPrisme();return;}
    _view.overlay=0;SetBusy(true);
    if(process.pid)
    {
        ReleaseRestartTargets();DWORD failure=0;
        for(DWORD pid:blackbone::Process::EnumByName(L"cs2.exe"))
        {
            HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
            if(handle)_restartTargets.push_back({handle,pid});
            else if(GetLastError()!=ERROR_INVALID_PARAMETER){failure=GetLastError();break;}
        }
        if(failure)
        {
            ReleaseRestartTargets();SetBusy(false);RecordError(L"Unable to restart CS2",L"Opening game sessions for a normal close",failure,false);DrawPrisme();return;
        }
        _restart.Start(GetTickCount64());_loaderStatus=L"Closing CS2 normally...";SetTimer(_hwnd,1,250,nullptr);TickRestart();DrawPrisme();
    }
    else LaunchFreshGame();
}

void MainDlg::ReleaseRestartTargets()
{
    for(const auto& target:_restartTargets)CloseHandle(target.first);_restartTargets.clear();_closeRequestedWindows.clear();
}

void MainDlg::LaunchFreshGame()
{
    ReleaseRestartTargets();_loader.Start(GetTickCount64());SetBusy(true);_loaderStatus=L"Launching CS2 via Steam...";
    auto result=ShellExecuteW(_hwnd,L"open",L"steam://rungameid/730",nullptr,nullptr,SW_SHOWNORMAL);
    if(reinterpret_cast<INT_PTR>(result)<=32)
    {
        _loader.Finish(false);SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
        RecordError(L"Steam launch failed",L"Opening the Steam protocol",0,false,L"ShellExecuteW returned "+std::to_wstring(reinterpret_cast<INT_PTR>(result))+L". No DLL was loaded.");DrawPrisme();return;
    }
    SetTimer(_hwnd,1,250,nullptr);DrawPrisme();
}

bool MainDlg::TickRestart()
{
    if(!_restart.active())return false;
    NormalClose::Request request={_restartTargets,_closeRequestedWindows};EnumWindows(NormalClose::Window,reinterpret_cast<LPARAM>(&request));
    if(request.error)
    {
        _restart.Cancel();ReleaseRestartTargets();SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
        RecordError(L"Normal game close failed",L"Sending WM_CLOSE to CS2",request.error,false,L"No force termination was attempted. No loading will follow.");DrawPrisme();return true;
    }
    bool stopped=true;for(const auto& target:_restartTargets)if(WaitForSingleObject(target.first,0)!=WAIT_OBJECT_0){stopped=false;break;}
    auto event=_restart.Observe(GetTickCount64(),stopped);
    if(event==RestartFlow::Event::Launch)LaunchFreshGame();
    else if(event==RestartFlow::Event::Timeout)
    {
        ReleaseRestartTargets();SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
        RecordError(L"CS2 did not close",L"Waiting for the current game session to exit",WAIT_TIMEOUT,false,L"A normal close was requested. CS2 did not exit within 30 seconds. Close it manually and try again. No loading was attempted.");DrawPrisme();
    }
    else {_loaderStatus=stopped?L"CS2 closed. Preparing a fresh launch...":L"Closing CS2 normally...";DrawPrisme();}
    return true;
}

void MainDlg::CancelLoader()
{
    if(!WaitingForGame())return;
    bool restarting=_restart.active();_restart.Cancel();ReleaseRestartTargets();
    _loader.Cancel();SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
    _loaderStatus=restarting?L"Restart cancelled. No loading will follow.":L"Loading cancelled. The game remains open.";DrawPrisme();
}

void MainDlg::TickLoader()
{
    if(TickRestart())return;
    if(!_loader.waiting())return;
    auto process=ProbeCS2(_loader.pid()?_loader.pid():_lastAutoPid);
    if(process.error!=ERROR_SUCCESS)
    {
        _loader.Finish(false);SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
        RecordError(L"Process access denied",L"Waiting for the process",process.error,false,Observations(process),_loader.pid(),_loader.identity());DrawPrisme();return;
    }
    auto event=_loader.Observe(GetTickCount64(),process.pid,process.identity,process.ready);
    if(event==LoaderFlow::Event::Timeout || event==LoaderFlow::Event::ProcessExited)
    {
        SetBusy(false);SetTimer(_hwnd,1,1000,nullptr);
        RecordError(event==LoaderFlow::Event::Timeout?L"Wait timed out":L"Process exited while waiting",
            L"Waiting for game readiness and stabilization",event==LoaderFlow::Event::Timeout?WAIT_TIMEOUT:ERROR_PROCESS_ABORTED,false,
            Observations(process)+L"\nMaximum wait: 180 s; required stabilization: 3 s.",_loader.pid(),_loader.identity());
        DrawPrisme();return;
    }
    if(event!=LoaderFlow::Event::Load)
    {
        _loaderStatus=process.pid?L"Initializing CS2...":L"Waiting for CS2...";
        if(_loader.phase()==LoaderFlow::Phase::Stabilizing)_loaderStatus=L"CS2 is ready. Stabilizing...";
        DrawPrisme();return;
    }
    _loaderStatus=L"Loading cheat...";DrawPrisme();
    if(_worker.joinable())_worker.join();
    const DWORD pid=_loader.pid();const uint64_t identity=_loader.identity();const auto path=_loadingPath;
    try
    {
        _worker=std::thread([this,pid,identity,path]()
        {
            NTSTATUS status=STATUS_UNSUCCESSFUL;
            xlog::Capture capture(_attemptTrace);_attemptStage=L"Checking the target session";
            try
            {
                auto current=ProbeCS2(pid);_attemptFacts=Observations(current);
                if(current.pid==pid && current.identity==identity && current.ready)
                {
                    _attemptStage=L"Reading and validating the DLL";
                    auto image=std::make_shared<blackbone::pe::PEImage>();
                    status=image->Load(path);
                    if(NT_SUCCESS(status) && !image->isExe() && !image->pureIL() && image->mType()==blackbone::mt_mod64)
                    {
                        image->Release(true);
                        InjectContext context;
                        context.cfg.processMode=Existing;context.cfg.injectMode=Normal;
                        context.pid=pid;context.cfg.pid=pid;context.procPath=L"cs2.exe";context.images.push_back(image);
                        _attemptStage=L"Loading through the engine (see traces)";
                        status=_core.InjectMultiple(&context);
                    }
                    else if(NT_SUCCESS(status))status=STATUS_INVALID_IMAGE_FORMAT;
                }
                else
                {
                    if(current.error==ERROR_ACCESS_DENIED)status=STATUS_ACCESS_DENIED;
                    else if(current.pid==pid && current.identity==identity)status=STATUS_UNSUCCESSFUL;
                    else status=STATUS_PROCESS_IS_TERMINATING;
                    _attemptFacts+=L"\nThe session or its readiness changed before loading. The engine was not called.";
                }
            }
            catch(...){status=STATUS_UNSUCCESSFUL;_attemptFacts+=L"\nA C++ exception interrupted this stage. Its exact origin is unavailable.";}
            PostMessageW(_hwnd,WM_INJECTION_DONE,NT_SUCCESS(status)?TRUE:FALSE,static_cast<LPARAM>(status));
        });
    }
    catch(...)
    {
        _loader.Finish(false);SetBusy(false);RecordError(L"Loading did not start",L"Creating the worker thread",0,false,L"Creating the C++ worker thread failed. The system did not provide an error code for this incident.");DrawPrisme();
    }
}
