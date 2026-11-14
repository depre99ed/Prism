#define UNICODE
#define _UNICODE
#include "../src/PrismeSurface.h"
#include <shellapi.h>
#include <fstream>
#include <sstream>
#pragma comment(lib,"shell32.lib")

// Headless checks of the renderer: PNG exports at several Windows scales, transitions,
// and hit-test geometry (minimum target size, centre of every target reaches itself).
static int failures=0;
static std::ostringstream notes;
static void Expect(bool ok,const std::string& what){if(!ok){++failures;notes<<"FAIL: "<<what<<"\n";}}

static PrismeState Library()
{
    PrismeState state;
    state.connected=true;state.detail=L"Running  \u00b7  PID 19256";state.canInject=true;
    state.status=L"1 DLL selected";
    state.dlls={{L"Aurora.dll",L"64 bits  \u00b7  Ready",true,false},{L"Legacy.dll",L"File not found",false,true}};
    return state;
}

static void CheckHits(PrismeSurface& surface,float scale,const std::string& name)
{
    for(size_t i=0;i<surface.hits.size();++i)
    {
        const auto& hit=surface.hits[i];
        if(hit.action==PrismeSurface::Shield || hit.action>=PrismeSurface::SelectBase && hit.action<PrismeSurface::OptionsBase)continue;
        Expect(hit.rect.Width>=28 && hit.rect.Height>=28,name+": target "+std::to_string(hit.action)+" is smaller than 28 px");
        // The centre of a target must reach that target (nothing drawn later may steal it).
        float cx=(hit.rect.X+hit.rect.Width/2)*scale,cy=(hit.rect.Y+hit.rect.Height/2)*scale;
        Expect(surface.HitTest(cx,cy)==hit.action,name+": centre of target "+std::to_string(hit.action)+" resolves to "+std::to_string(surface.HitTest(cx,cy)));
        // And so must each corner, one device pixel inside.
        float xs[2]={hit.rect.X*scale+1,hit.rect.GetRight()*scale-1},ys[2]={hit.rect.Y*scale+1,hit.rect.GetBottom()*scale-1};
        for(float x:xs)for(float y:ys)
        {
            int got=surface.HitTest(x,y);
            Expect(got>=0,name+": corner of target "+std::to_string(hit.action)+" is dead");
        }
    }
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int)
{
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc<2)return 2;
    std::wstring folder=argv[1];LocalFree(argv);CreateDirectoryW(folder.c_str(),nullptr);
    bool okay=true;
    {
        PrismeSurface surface;if(!surface.Init())return 3;
        Expect(surface.HasBrandMark(),"transparent brand mark is embedded and decoded");
        PrismeState state;
        okay&=surface.Export((folder+L"\\prisme-empty.png").c_str(),state);
        state.blocked=true;state.status=L"Add a DLL to get started.";
        okay&=surface.Export((folder+L"\\prisme-empty-fixture.png").c_str(),state,true);
        CheckHits(surface,1,"empty");
        state=Library();
        okay&=surface.Export((folder+L"\\prisme-library.png").c_str(),state,true);
        CheckHits(surface,1,"library");
        state.dlls.push_back({L"Troisieme.dll",L"32 bits  \u00b7  Ready",true,false});
        okay&=surface.Export((folder+L"\\prisme-library-scroll.png").c_str(),state,true);
        state=Library();state.hover=PrismeSurface::Inject;
        okay&=surface.Export((folder+L"\\prisme-hover-inject.png").c_str(),state,true);
        state.pressed=PrismeSurface::Inject;
        okay&=surface.Export((folder+L"\\prisme-press-inject.png").c_str(),state,true);
        state=Library();state.hover=PrismeSurface::OptionsBase;state.selected=0;
        okay&=surface.Export((folder+L"\\prisme-hover-row.png").c_str(),state,true);
        state=Library();state.canInject=false;state.blocked=true;state.status=L"Choose a target process.";state.connected=false;
        okay&=surface.Export((folder+L"\\prisme-blocked.png").c_str(),state,true);
        state=Library();state.busy=true;state.canInject=false;state.status=L"Please wait.";
        okay&=surface.Export((folder+L"\\prisme-busy.png").c_str(),state,true);
        state=Library();state.overlay=2;state.processes={{L"cs2.exe",19256},{L"notepad.exe",1640},{L"explorer.exe",456}};
        okay&=surface.Export((folder+L"\\prisme-processes.png").c_str(),state,true);
        CheckHits(surface,1,"processes");
        state.query=L"note";
        okay&=surface.Export((folder+L"\\prisme-processes-search.png").c_str(),state,true);
        state.query.clear();
        state.overlay=1;okay&=surface.Export((folder+L"\\prisme-menu.png").c_str(),state,true);
        CheckHits(surface,1,"menu");
        state.canRevealZip=true;
        okay&=surface.Export((folder+L"\\prisme-menu-zip-ready.png").c_str(),state,true);
        CheckHits(surface,1,"ZIP ready menu");
        state.canRevealZip=false;
        // A click on dead glass inside the card must be swallowed, not dismiss the menu.
        Expect(surface.HitTest(60,108)==PrismeSurface::Shield,"menu: card padding should be a shield");
        Expect(surface.HitTest(5,5)==-1,"menu: veil outside the card should be -1");
        state.hover=PrismeSurface::Add;okay&=surface.Export((folder+L"\\prisme-menu-hover.png").c_str(),state,true);
        state.hover=-1;state.overlay=3;state.selected=0;
        okay&=surface.Export((folder+L"\\prisme-dll-options.png").c_str(),state,true);
        CheckHits(surface,1,"dll options");
        state=Library();state.overlay=4;state.editorTitle=L"Aurora";state.editorDllName=L"Aurora.dll";state.editorNew=true;
        okay&=surface.Export((folder+L"\\prisme-catalogue-editor.png").c_str(),state,true);
        CheckHits(surface,1,"catalogue editor");

        state.editorSelectAll=false;state.editorCaret=3;state.caretVisible=true;
        okay&=surface.Export((folder+L"\\prism-editor-caret-on.png").c_str(),state,true);
        int nearest=surface.TitleCaretAt(62,state);
        Expect(nearest==0,"title click at leading edge places caret at start");
        state.caretVisible=false;
        okay&=surface.Export((folder+L"\\prism-editor-caret-off.png").c_str(),state,true);
        state.caretVisible=true;
        state.editorTitle=std::wstring(80,L'W');state.editorCaret=80;
        okay&=surface.Export((folder+L"\\prism-editor-long-title.png").c_str(),state,true);
        Expect(surface.TitleCaretAt(330,state)>=78,"long title scrolls to keep its caret visible");
        state.editorTitle=L"Aurora";state.editorCaret=3;
        okay&=surface.Export((folder+L"\\prism-short-title-after-long.png").c_str(),state,true);
        Expect(surface.TitleCaretAt(62,state)==0,"short title restores its leading edge after a long title");

        state.editorArtwork=folder+L"\\prisme-empty.png";
        okay&=surface.Export((folder+L"\\prisme-catalogue-artwork.png").c_str(),state,true);
        CheckHits(surface,1,"catalogue image");
        state=Library();state.selected=0;state.dlls[0].artwork=folder+L"\\prisme-empty.png";
        okay&=surface.Export((folder+L"\\prisme-catalogue.png").c_str(),state,true);
        CheckHits(surface,1,"selected catalogue");
        state.dlls[0].artwork=folder+L"\\prisme-empty-fixture.png";
        state.selected=0;
        okay&=surface.Export((folder+L"\\thumbnail-selected.png").c_str(),state);
        state.selected=-1;
        okay&=surface.Export((folder+L"\\thumbnail-unselected.png").c_str(),state);
        for(const auto& hit:surface.hits)Expect(!(hit.action>=PrismeSurface::ToggleBase && hit.action<PrismeSurface::SelectBase),"no multi-selection checkbox in catalogue");

        state=Library();state.overlay=5;state.previewName=L"Catalogue ami.zip";
        state.previewSummary=L"4 cheats \u00b7 2400 Ko au total";
        state.previewItems={{L"Aurora",L"aurora.dll \u00b7 1200 Ko \u00b7 image"},{L"Minimal",L"minimal.dll \u00b7 600 Ko"},{L"Entra\u00eenement",L"practice.dll \u00b7 500 Ko"},{L"Autre",L"autre.dll \u00b7 100 Ko"}};
        okay&=surface.Export((folder+L"\\prisme-zip-preview.png").c_str(),state,true);CheckHits(surface,1,"ZIP preview");
        state.panelFirst=1;okay&=surface.Export((folder+L"\\prisme-zip-preview-next.png").c_str(),state,true);CheckHits(surface,1,"ZIP preview next");
        state=Library();state.overlay=6;state.errorItems={{L"Module introuvable",L"2026-11-03 15:42:11"},{L"Acc\u00e8s refus\u00e9",L"2026-11-03 15:39:04"},{L"D\u00e9lai d\u00e9pass\u00e9",L"2026-11-03 15:36:00"},{L"Processus ferm\u00e9",L"2026-11-03 15:20:00"},{L"DLL incompatible",L"2026-11-03 15:10:00"}};
        okay&=surface.Export((folder+L"\\prisme-error-history.png").c_str(),state,true);CheckHits(surface,1,"error history");
        state.overlay=7;state.errorHeading=L"Module introuvable";
        state.errorLines={L"Cause confirm\u00e9e : un module n'a pas \u00e9t\u00e9",L"trouv\u00e9. Il peut s'agir de la DLL ou",L"d'une de ses d\u00e9pendances ; ce code seul",L"ne donne pas son nom.",L"",L"\u00c9tape : chargement standard Windows",L"Code NTSTATUS : 0xC0000135",L"",L"PID : 19256",L"DLL : C:\\Catalogue\\aurora.dll",L"Traces de cette tentative :",L"engine returned 0xC0000135"};
        okay&=surface.Export((folder+L"\\prisme-error-detail.png").c_str(),state,true);CheckHits(surface,1,"error detail");
        state=Library();state.overlay=6;
        okay&=surface.Export((folder+L"\\prisme-error-empty.png").c_str(),state,true);CheckHits(surface,1,"empty error history");


        state=Library();state.overlay=8;state.versionTitle=L"Aurora";state.versionChoice=1;
        state.versionItems={{L"v1.0 stable",L"aurora.dll"},{L"v1.1",L"aurora.dll \u00b7 active"},{L"v1.2 beta",L"aurora.dll"},{L"v0.9",L"legacy.dll"}};
        okay&=surface.Export((folder+L"\\prism-versions.png").c_str(),state,true);CheckHits(surface,1,"version catalogue");
        state.overlay=4;state.editorVersion=true;state.editorNew=true;state.editorTitle=L"v1.3 preview";state.editorDllName=L"aurora-new.dll";
        state.editorCaret=3;state.caretVisible=true;
        okay&=surface.Export((folder+L"\\prism-version-editor.png").c_str(),state,true);CheckHits(surface,1,"version editor");
        state=Library();state.overlay=9;state.selectionItems={{L"Aurora \u00b7 v1.0 stable",L"aurora.dll"},{L"Aurora \u00b7 v1.1",L"aurora.dll"},{L"Minimal \u00b7 Initial version",L"minimal.dll"}};
        state.selectionChecked={true,false,true};state.exportOption=true;
        okay&=surface.Export((folder+L"\\prism-export-selection.png").c_str(),state,true);CheckHits(surface,1,"selective catalogue export");
        state.exportDiagnostics=true;state.selectionItems={{L"Module not found",L"2026-11-05 17:42:00"},{L"Process access denied",L"2026-11-05 17:39:00"},{L"Wait timed out",L"2026-11-05 17:35:00"}};
        okay&=surface.Export((folder+L"\\prism-diagnostic-export.png").c_str(),state,true);CheckHits(surface,1,"selective diagnostic export");

        state=Library();state.overlay=10;
        okay&=surface.Export((folder+L"\\prism-first-run-notice.png").c_str(),state,true);CheckHits(surface,1,"first-run notice");
        // Open transition: halfway frame, then fully open. Hits must belong to the menu from the first frame.
        state=Library();PrismeState idle=state;surface.Advance(idle,100000.f);
        state.overlay=1;surface.Advance(state,95);
        Expect(surface.Animating(),"menu transition should be animating");
        okay&=surface.Export((folder+L"\\prisme-menu-mid.png").c_str(),state,true,false);
        Expect(surface.HitTest(60,130)==PrismeSurface::Add || surface.HitTest(150,130)==PrismeSurface::Add,"menu hits live during transition");
        surface.Advance(state,400);okay&=surface.Export((folder+L"\\prisme-menu-end.png").c_str(),state,true,false);
        Expect(!surface.Animating(),"menu transition should settle");
        // Closing keeps drawing the menu while it fades, with the page's own hits restored.
        state.overlay=0;surface.Advance(state,60);okay&=surface.Export((folder+L"\\prisme-menu-closing.png").c_str(),state,true,false);
        Expect(surface.Animating(),"menu closing should be animating");
        Expect(surface.HitTest(360,230)>=0,"page hits are back as soon as the menu starts closing");
    }
    // Windows display scales: the scene is rendered at device resolution and hits stay aligned.
    for(float scale:{1.25f,1.5f,1.75f,2.f})
    {
        PrismeSurface surface;if(!surface.Init(scale))return 3;
        PrismeState state=Library();state.dlls.push_back({L"Troisieme.dll",L"32 bits  \u00b7  Ready",true,false});
        std::wstring suffix=std::to_wstring((int)(scale*100));
        okay&=surface.Export((folder+L"\\prisme-scale-"+suffix+L".png").c_str(),state,true);
        CheckHits(surface,scale,"scale "+std::to_string((int)(scale*100)));
        state.overlay=1;okay&=surface.Export((folder+L"\\prisme-scale-"+suffix+L"-menu.png").c_str(),state,true);
        CheckHits(surface,scale,"scale "+std::to_string((int)(scale*100))+" menu");
        for(int panel:{5,6,7,8,9,10})
        {
            state.overlay=panel;state.panelFirst=0;state.versionTitle=L"Aurora";state.versionItems={{L"v1",L"old.dll"},{L"v2",L"new.dll"},{L"v3",L"beta.dll"},{L"v4",L"preview.dll"}};
            state.selectionItems=state.versionItems;state.selectionChecked={true,false,true,false};state.previewName=L"catalogue.zip";state.previewSummary=L"4 entr\u00e9es";
            state.previewItems={{L"Un",L"un.dll"},{L"Deux",L"deux.dll"},{L"Trois",L"trois.dll"},{L"Quatre",L"quatre.dll"}};
            state.errorItems={{L"Erreur",L"2026-11-03 15:00:00"}};state.errorHeading=L"Erreur";state.errorLines.assign(30,L"Code NTSTATUS : 0xC0000135");
            okay&=surface.Export((folder+L"\\prisme-scale-"+suffix+L"-panel-"+std::to_wstring(panel)+L".png").c_str(),state,true);
            CheckHits(surface,scale,"scaled panel "+std::to_string(panel));
        }

    }
    HWND window=CreateWindowExW(WS_EX_LAYERED,L"STATIC",L"Prisme render test",WS_POPUP,0,0,PrismeSurface::Width,PrismeSurface::Height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    PrismeSurface surface;surface.Init();PrismeState state=Library();
    bool native=window && surface.Present(window,state);
    DWORD code=native?0:GetLastError();
    if(window)DestroyWindow(window);
    std::ofstream report(folder+L"\\prisme-render-test.json");
    report<<"{\"png_exports\":"<<(okay?"true":"false")<<",\"layered_window_present\":"<<(native?"true":"false")<<",\"win32_error\":"<<code<<",\"failures\":"<<failures<<",\"width\":392,\"height\":444}";
    std::ofstream details(folder+L"\\prisme-render-test.log");details<<notes.str();
    return okay&&native&&!failures?0:4;
}
