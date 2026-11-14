#pragma once
#include "../src/FirstRunNotice.h"
#include "../src/ManagedCopies.h"
static void TestFirstRunAndCopies(const std::wstring& folder)
{
    namespace fs=std::filesystem;auto root=fs::path(folder)/L"first-run"/VersionStore::Id();fs::create_directories(root);
    auto user=root/L"user-data";
    require(FirstRunNotice::Claim(user.wstring())==FirstRunNotice::Result::Show,"first launch shows the notice");
    require(FirstRunNotice::Claim(user.wstring())==FirstRunNotice::Result::Seen,"second launch does not repeat the notice");
    wchar_t executable[32768]={};GetModuleFileNameW(nullptr,executable,32768);
    auto command=L"\""+std::wstring(executable)+L"\" --first-run-probe \""+user.wstring()+L"\"";
    STARTUPINFOW startup={sizeof(startup)};PROCESS_INFORMATION process={};
    require(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"start independent notice probe");
    require(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0,"independent notice probe completes");DWORD code=99;GetExitCodeProcess(process.hProcess,&code);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);require(code==0,"notice remains dismissed across processes and updates");
    require(FirstRunNotice::Claim((root/L"another-user").wstring())==FirstRunNotice::Result::Show,"a separate user data folder gets its own first notice");
    Put(root/L"blocked","not a directory");require(FirstRunNotice::Claim((root/L"blocked/subfolder").wstring())==FirstRunNotice::Result::Error,"preference errors are not mistaken for an already-seen notice");
    auto source=root/L"original.dll";Put(source,"USER DLL BYTES");
    auto saved=VersionStore::Snapshot(source.wstring(),(user/L"Versions").wstring(),L"v1");fs::remove(source);
    require(Bytes(saved.path)=="USER DLL BYTES","stored copy survives deletion of the original file");
    using R=ManagedCopies::Result;
    require(ManagedCopies::Remove(saved.path,user.wstring(),{saved.path})==R::Referenced && fs::exists(saved.path),"copies referenced by another version are retained");
    HANDLE locked=CreateFileW(saved.path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);require(locked!=INVALID_HANDLE_VALUE,"hold stored copy open");
    require(ManagedCopies::Remove(saved.path,user.wstring(),{})==R::Failed && fs::exists(saved.path),"a locked copy is not forcibly deleted");CloseHandle(locked);
    require(ManagedCopies::Remove(saved.path,user.wstring(),{})==R::Removed && !fs::exists(saved.path),"explicit deletion removes the unused managed copy");
    require(ManagedCopies::Remove(saved.path,user.wstring(),{})==R::Missing,"already missing copies need no recreation");
    Put(source,"ORIGINAL");require(ManagedCopies::Remove(source.wstring(),user.wstring(),{})==R::External && fs::exists(source),"original files outside Prism data are never deleted");
    auto runtime=user/L"Runtime/msdia140.dll";Put(runtime,"RUNTIME");
    require(ManagedCopies::Remove(runtime.wstring(),user.wstring(),{})==R::External && fs::exists(runtime),"runtime dependencies are protected from catalogue deletion");
    auto imported=user/L"Catalogues/bundle/DLLs/1/1/mod.dll";Put(imported,"IMPORTED");
    require(ManagedCopies::Remove(imported.wstring(),user.wstring(),{})==R::Removed && !fs::exists(imported),"imported DLL copies are removed on explicit catalogue deletion");
}
