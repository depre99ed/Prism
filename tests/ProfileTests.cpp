#include "../src/stdafx.h"
#include "../src/ProfileMgr.h"
#include <BlackBone/src/BlackBone/Misc/Utils.h>
#include <iostream>
#include <fstream>

// Profile tests are isolated from the injection engine and never link Blackbone.
std::wstring blackbone::Utils::GetExeDirectory() { return L"."; }
std::wstring blackbone::Utils::UTF8ToWstring(const std::string& input)
{
    int size=MultiByteToWideChar(CP_UTF8,0,input.data(),(int)input.size(),nullptr,0);std::wstring output(size,0);
    if(size)MultiByteToWideChar(CP_UTF8,0,input.data(),(int)input.size(),output.data(),size);return output;
}
std::string blackbone::Utils::WstringToUTF8(const std::wstring& input)
{
    int size=WideCharToMultiByte(CP_UTF8,0,input.data(),(int)input.size(),nullptr,0,nullptr,nullptr);std::string output(size,0);
    if(size)WideCharToMultiByte(CP_UTF8,0,input.data(),(int)input.size(),output.data(),size,nullptr,nullptr);return output;
}
static void require(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
#include "LoaderFlowTests.h"
#include "ArchiveTests.h"
#include "ErrorJournalTests.h"
#include "DialogInputTests.h"
#include "TitleInputTests.h"
#include "VersionCatalogueTests.h"
#include "NormalCloseTests.h"
#include "FirstRunAndCopiesTests.h"
int wmain(int argc,wchar_t** argv)
{
    try
    {
        if(argc==3 && std::wstring(argv[1])==L"--first-run-probe")
        {
            auto result=FirstRunNotice::Claim(argv[2]);
            return result==FirstRunNotice::Result::Seen?0:(result==FirstRunNotice::Result::Show?10:20);
        }
        TestLoaderFlow();
        TestDialogInput();
        TestTitleInput();
        TestRestartFlow();
        TestNormalClose();
        require(argc==2,"test data directory required");
        const std::wstring folder=argv[1];
        CreateDirectoryW(folder.c_str(),nullptr);
        TestArchives(folder);
        TestErrorJournal(folder);
        TestVersionCatalogue(folder);
        TestFirstRunAndCopies(folder);
        const auto path=folder+L"\\roundtrip.xpr";
        ProfileMgr original;
        auto& cfg=original.config();
        cfg.images={L"C:\\DLL\\caf\u00e9 & test.dll",L"C:\\absent\\missing.dll"};
        cfg.imageTitles={L"Mon cheat \u00e9t\u00e9 & test",L"Deuxi\u00e8me"};
        cfg.imageArtwork={L"C:\\Images\\icone.png",L""};cfg.selectedCheat=1;
        cfg.imageEnabled={true,false}; cfg.autoTargetCs2=false;
        cfg.procName=L"other.exe";cfg.injectMode=0;cfg.processMode=0;cfg.delay=125;
        require(original.Save(path),"save profile");
        ProfileMgr restored;
        require(restored.Load(path),"load profile");
        require(restored.config().images==cfg.images,"unicode paths, escaped XML and missing paths round-trip");
        require(restored.config().imageEnabled==cfg.imageEnabled,"checkbox selection round-trip");
        require(!restored.config().autoTargetCs2,"free target choice round-trip");
        require(restored.config().imageTitles==cfg.imageTitles,"titles round-trip");
        require(restored.config().imageArtwork==cfg.imageArtwork,"optional images round-trip");
        require(restored.config().selectedCheat==1,"single selection round-trip");
        require(restored.config().delay==125,"original core options preserved");
        require(restored.Load(path) && restored.config().images.size()==2,"reloading does not duplicate paths");
        const auto malformed=folder+L"\\malformed.xpr";
        {std::wofstream file(malformed);file<<L"<XenosConfig><broken>";}
        require(!restored.Load(malformed),"malformed profile rejected");
        require(restored.config().images==cfg.images,"failed load preserves prior configuration");
        const auto legacy=folder+L"\\legacy.xpr";
        { std::wofstream file(legacy);file<<L"<XenosConfig><imagePath>C:\\legacy.dll</imagePath><procName>cs2.exe</procName><injectMode>0</injectMode></XenosConfig>"; }
        require(restored.Load(legacy),"legacy profile loads");
        require(restored.config().images.size()==1 && restored.config().imageEnabled.empty(),"old profiles default selected by UI");
        require(restored.config().autoTargetCs2,"old profiles default CS2 automatic selection");
        restored.config().images.clear();restored.config().imageEnabled.clear();
        require(restored.Save(path),"save cleared library");
        ProfileMgr empty;require(empty.Load(path) && empty.config().images.empty(),"removed DLLs stay removed after restart");
        require(GetFileAttributesW((path+L".bak").c_str())!=INVALID_FILE_ATTRIBUTES,"profile backup retained");
        std::cout<<"PASS: first-run notice across processes, independent stored copies, safe explicit deletion; immutable DLL versions, v1/v2 ZIP compatibility, selective exports, path redaction, native normal-close and restart sequencing; portable ZIP, exact DLL/image bytes, duplicates, missing files, tampering, unsafe paths; loader sequencing, cancellation, timeout, PID reuse; catalogue metadata; profile round-trip, Unicode/XML paths, missing paths, selection, legacy compatibility, repeated loading and cleared library\n";
        return 0;
    }
    catch(const std::exception& ex) {std::cerr<<"FAIL: "<<ex.what()<<"\n";return 1;}
}
