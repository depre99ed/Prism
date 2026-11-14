#pragma once

#include "../src/ErrorJournal.h"

static void TestErrorJournal(const std::wstring& folder)

{

    GUID run;CoCreateGuid(&run);wchar_t runId[40];StringFromGUID2(run,runId,40);

    auto root=std::filesystem::path(folder)/L"journal"/runId;

    Diagnostics::Journal journal;journal.Load(root.wstring());

    auto entry=Diagnostics::Make(L"\u00c9chec \u00e9t\u00e9",L"Chargement Windows",0xC0000135,true,123,456,

        L"C:\\DLL\\\u00e9t\u00e9.dll",L"Fen\u00eatre observ\u00e9e",{"engine returned 0xC0000135"});

    require(entry.report.find(L"123")!=entry.report.npos && entry.report.find(L"0xC0000135")!=entry.report.npos,"diagnostic keeps raw code and PID");

    require(entry.report.find(L"engine returned")!=entry.report.npos,"diagnostic keeps attempt trace");

    require(Diagnostics::Cause(0xC0000135,true).find(L"does not identify its name")!=std::wstring::npos,"missing dependency name is not invented");

    require(Diagnostics::Cause(0,false).find(L"No system error code")!=std::wstring::npos,"missing system code is explicit");

    require(journal.Add(entry),"diagnostic persists");

    Diagnostics::Journal restored;restored.Load(root.wstring());

    require(!restored.entries.empty() && restored.entries[0].report==entry.report && restored.entries[0].title==entry.title,"Unicode report survives restart");

    auto lines=Diagnostics::Lines(entry.report);require(lines.size()>10,"long diagnostic can be paged");

    for(const auto& line:lines)require(line.size()<=44,"diagnostic lines fit compact reader");

    for(int i=0;i<102;++i){entry.title=L"Erreur "+std::to_wstring(i);require(journal.Add(entry),"history append");}

    restored.Load(root.wstring());require(restored.entries.size()==100,"persistent history is bounded");

    require(restored.entries[0].title==L"Erreur 101","latest error sorts first after restart");

}

