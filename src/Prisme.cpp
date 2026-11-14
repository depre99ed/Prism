#include "MainDlg.h"
#include <windowsx.h>

void MainDlg::DrawPrisme()
{
    if(!_prismeReady || !_hwnd)return;
    _view.busy=_injecting;
    _view.canRevealZip=!_lastExportZip.empty();
    _view.autoCS2=true;_view.target=L"Counter-Strike 2";
    DWORD pid=WaitingForGame()?_loader.pid():_lastAutoPid;
    _view.connected=pid!=0 && pid!=MAXDWORD;
    _view.detail=_restart.active()?L"Closing current session...":(_view.connected?L"Running  \u00b7  PID "+std::to_wstring(pid):L"Launch via Steam");
    _view.dlls.clear();
    for(size_t i=0;i<_imagePaths.size();++i)
    {
        PrismeDll dll;
        dll.name=i<_cheatTitles.size()&&!_cheatTitles[i].empty()?_cheatTitles[i]:blackbone::Utils::StripPath(_imagePaths[i]);
        dll.enabled=static_cast<int>(i)==_selectedCheat;
        dll.missing=!_images[i] || GetFileAttributesW(_imagePaths[i].c_str())==INVALID_FILE_ATTRIBUTES;
        dll.detail=dll.missing?L"DLL not found":blackbone::Utils::StripPath(_imagePaths[i]);
        if(i<_versions.size() && !_versions[i].items.empty())dll.detail=_versions[i].items[_versions[i].active].label+L"  \u00b7  "+dll.detail;
        if(i<_cheatArtwork.size())dll.artwork=_cheatArtwork[i];
        _view.dlls.push_back(dll);
    }
    if(_view.overlay!=3)_view.selected=_selectedCheat;
    bool available=_selectedCheat>=0 && _selectedCheat<(int)_images.size() &&
        _images[_selectedCheat] && !_view.dlls[_selectedCheat].missing &&
        !_images[_selectedCheat]->isExe() && !_images[_selectedCheat]->pureIL() && _images[_selectedCheat]->mType()==blackbone::mt_mod64;
    _view.canInject=(!_injecting && available) || WaitingForGame();
    _view.primaryLabel=WaitingForGame()?L"Cancel":(_injecting?L"Loading...":(_view.connected?L"Restart & Load":L"Load"));
    if(_bundleBusy)_view.primaryLabel=std::wstring(_bundleBusy==3?L"Preview ":(_bundleBusy==2?L"Import ":L"Export "))+std::to_wstring(_bundlePercent)+L" %";
    _view.blocked=!available && !_injecting;
    _view.status=_imagePaths.empty()?L"Add your first cheat.":(available?L"Select a cheat, then Load.":L"Select an available native 64-bit DLL.");
    if(available && _selectedCheat<(int)_cheatTitles.size())_view.status=L"Selected: "+_cheatTitles[_selectedCheat];
    if(!_loaderStatus.empty())_view.status=_loaderStatus;
    if(!_injecting && _view.hover==PrismeSurface::Inject && _view.connected)_view.status=L"CS2 will close normally, then restart.";
    if(!_injecting && _view.hover==PrismeSurface::SaveProfile)_view.status=L"Export all DLLs and images as a ZIP.";
    TickPrisme();
}

void MainDlg::TickPrisme()
{
    if(!_prismeReady || !_hwnd)return;
    bool caret=_view.overlay==4 && _view.editorFocused;
    if(caret && !_caretTicking){_view.caretVisible=true;SetTimer(_hwnd,3,530,nullptr);_caretTicking=true;}
    else if(!caret && _caretTicking){KillTimer(_hwnd,3);_caretTicking=false;}
    _surface.Present(_hwnd,_view);
    if(_surface.Animating()){if(!_ticking){SetTimer(_hwnd,2,15,nullptr);_ticking=true;}}
    else if(_ticking){KillTimer(_hwnd,2);_ticking=false;}
}

void MainDlg::PrismeAction(int action)
{
    using UI=PrismeSurface;
    if(action==UI::Close){OnClose(_hwnd,WM_CLOSE,0,0);return;}
    if(action==UI::Minimize){ShowWindow(_hwnd,SW_MINIMIZE);return;}
    if(action==UI::Inject && WaitingForGame()){CancelLoader();return;}
    if(action==UI::Shield || _injecting)return;
    _view.focus=-1;
    if(_view.overlay==4 && action!=UI::EditTitle)_view.editorFocused=false;
    if(action<0){if(_view.overlay){_view.overlay=0;DrawPrisme();}return;}
    if(action==UI::Menu)_view.overlay=_view.overlay?0:1;
    else if(action==UI::Back)
    {
        if(_view.overlay==4 && _view.editorVersion){_view.editorVersion=false;_view.overlay=8;RefreshVersionPanel();}
        else {_view.overlay=0;_pendingZip.clear();_pendingPreview={};}
    }
    else if(action==UI::Versions || action==UI::Locate)ShowVersions(_view.selected);
    else if(action==UI::AddVersion)EditVersion(true);
    else if(action==UI::RenameVersion)EditVersion(false);
    else if(action==UI::UseVersion){if(ActivateVersion(_versionCheat,_versionChoice)){SelectCheat(_versionCheat);_view.overlay=0;}}
    else if(action==UI::RemoveVersion)RemoveVersion();
    else if(action==UI::Remove)
    {
        int selected=_view.selected;
        if(selected>=0 && selected<(int)_imagePaths.size())
        {
            ListView_SetItemState(_modules.hwnd(),-1,0,LVIS_SELECTED);
            ListView_SetItemState(_modules.hwnd(),selected,LVIS_SELECTED,LVIS_SELECTED);
            _view.overlay=0;OnRemoveImage(_hwnd,0,0,0);ClearLoaderStatus();
        }
    }
    else if(action>=UI::VersionBase && action<UI::VersionBase+1000){_versionChoice=action-UI::VersionBase;RefreshVersionPanel();}
    else if(action==UI::ExportReports)PrepareExport(true);
    else if(action==UI::ConfirmExport)ConfirmExport();
    else if(action==UI::ExportOption)_view.exportOption=!_view.exportOption;
    else if(action==UI::SelectAll || action==UI::SelectNone)std::fill(_view.selectionChecked.begin(),_view.selectionChecked.end(),action==UI::SelectAll);
    else if(action>=UI::ExportBase && action<UI::ExportBase+1000)
    {int i=action-UI::ExportBase;if(i<(int)_view.selectionChecked.size())_view.selectionChecked[i]=!_view.selectionChecked[i];}
    else if(action==UI::History)ShowErrors();
    else if(action>=UI::ErrorBase && action<UI::ErrorBase+100)ShowErrorDetail(action-UI::ErrorBase);
    else if(action==UI::CopyReport)CopyError();
    else if(action==UI::ConfirmImport){if(!_pendingZip.empty())BeginBundle(true,_pendingZip);}
    else if(action==UI::PanelPrevious || action==UI::PanelNext)
    {
        int page=PanelSize();
        int count=PanelCount();
        _view.panelFirst=(std::clamp)(_view.panelFirst+(action==UI::PanelNext?page:-page),0,(std::max)(0,count-page));
    }
    else if(action==UI::Add){_view.overlay=0;DrawPrisme();EditCheat(true);}
    else if(action==UI::Inject){if(_view.canInject)StartLoader();else ++_view.nudge;}
    else if(action>=UI::OptionsBase && action<UI::PickBase)
    {
        _view.selected=action-UI::OptionsBase;
        if(_view.selected<(int)_imagePaths.size())
        {
            ListView_SetItemState(_modules.hwnd(),_view.selected,LVIS_SELECTED,LVIS_SELECTED);
            _view.overlay=3;
        }
    }
    else if(action>=UI::SelectBase && action<UI::OptionsBase)SelectCheat(action-UI::SelectBase);
    else if(action==UI::EditEntry)EditCheat(false);
    else if(action==UI::PickArtwork)ChooseArtwork();
    else if(action==UI::ClearArtwork){_editArtwork.clear();_view.editorArtwork.clear();}
    else if(action==UI::SaveEntry)CommitCheat();
    else if(action==UI::EditTitle){_view.editorFocused=true;_view.caretVisible=true;}
    else
    {
        _view.overlay=0;DrawPrisme();
        if(action==UI::OpenProfile)OnLoadProfile(_hwnd,0,0,0);
        else if(action==UI::RevealExport)RevealZip();
        else if(action==UI::PreviousPage){_view.first=(std::max)(0,_view.first-2);}
        else if(action==UI::NextPage){_view.first=(std::min)((std::max)(0,(int)_imagePaths.size()-2),_view.first+2);}
        else if(action==UI::SaveProfile)OnSaveProfile(_hwnd,0,0,0);
        else if(action==UI::Locate){OnLocateImage(_hwnd,0,0,0);ClearLoaderStatus();}
        else if(action==UI::Remove){OnRemoveImage(_hwnd,0,0,0);ClearLoaderStatus();}
        else if(action==UI::Clear){OnClearImages(_hwnd,0,0,0);ClearLoaderStatus();}
        else if(action==UI::Transparency){_view.opacity=_view.opacity==226?190:(_view.opacity==190?150:226);_view.overlay=1;}
    }
    UpdateInterface();DrawPrisme();
}

INT_PTR MainDlg::OnPrismeInput(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_ERASEBKGND)return TRUE;
    if(msg==WM_SETFOCUS || msg==WM_KILLFOCUS){_view.caretVisible=msg==WM_SETFOCUS;TickPrisme();return FALSE;}
    if(msg==WM_SETCURSOR && LOWORD(lp)==HTCLIENT)
    {
        POINT cursor;GetCursorPos(&cursor);ScreenToClient(h,&cursor);
        if(_view.overlay==4 && _surface.HitTest((float)cursor.x,(float)cursor.y)==PrismeSurface::EditTitle)
        {SetCursor(LoadCursorW(nullptr,IDC_IBEAM));SetWindowLongPtrW(h,DWLP_MSGRESULT,TRUE);return TRUE;}
    }
    if(msg==WM_PAINT){ValidateRect(h,nullptr);DrawPrisme();return TRUE;}
    if(msg==WM_GETDLGCODE){SetWindowLongPtrW(h,DWLP_MSGRESULT,DLGC_WANTALLKEYS|DLGC_WANTCHARS|DLGC_WANTTAB);return TRUE;}
    if(msg==WM_ACTIVATE){if(LOWORD(wp)!=WA_INACTIVE)SetFocus(h);DrawPrisme();return FALSE;}
    if(msg==WM_NCCALCSIZE && wp)
    {
        // The dialog template has a caption: without this the client area starts 8 px right and 31 px below the drawing.
        SetWindowLongPtrW(h,DWLP_MSGRESULT,0);return TRUE;
    }
    if(msg==WM_NCHITTEST)
    {
        POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(h,&p);
        // Any empty glass drags the window (not the transparent margin, and not while a menu is open: there it dismisses).
        const float x=p.x/_uiScale,y=p.y/_uiScale;
        LRESULT where=(!_view.overlay && x>=10 && x<=382 && y>=10 && y<=PrismeSurface::Height-10 && _surface.HitTest((float)p.x,(float)p.y)<0)?HTCAPTION:HTCLIENT;
        SetWindowLongPtrW(h,DWLP_MSGRESULT,where);return TRUE;
    }
    if(msg==WM_MOUSEMOVE)
    {
        int hover=_surface.HitTest((float)GET_X_LPARAM(lp),(float)GET_Y_LPARAM(lp));
        if(hover==PrismeSurface::Shield || (_injecting && hover!=PrismeSurface::Minimize && !(hover==PrismeSurface::Inject && WaitingForGame())))hover=-1;
        bool live=hover>=0 && !(hover==PrismeSurface::Inject && !_view.canInject);
        SetCursor(LoadCursorW(nullptr,hover==PrismeSurface::EditTitle?IDC_IBEAM:(live?IDC_HAND:IDC_ARROW)));
        if(hover!=_view.hover){_view.hover=hover;TickPrisme();}
        TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,h,0};TrackMouseEvent(&track);return TRUE;
    }
    if(msg==WM_MOUSELEAVE){_view.hover=-1;TickPrisme();return TRUE;}
    if(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONDBLCLK)
    {
        // A click only counts when it is released on the target where it was pressed.
        SetFocus(h);
        _pressAction=_surface.HitTest((float)GET_X_LPARAM(lp),(float)GET_Y_LPARAM(lp));_pressDown=true;
        _view.pressed=_pressAction;SetCapture(h);TickPrisme();return TRUE;
    }
    if(msg==WM_LBUTTONUP)
    {
        if(!_pressDown)return TRUE;
        int target=_surface.HitTest((float)GET_X_LPARAM(lp),(float)GET_Y_LPARAM(lp)),started=_pressAction;
        _pressDown=false;_pressAction=-1;_view.pressed=-1;
        if(GetCapture()==h)ReleaseCapture();
        if(target==started)
        {
            PrismeAction(target);
            if(target==PrismeSurface::EditTitle){_view.editorCaret=_surface.TitleCaretAt((float)GET_X_LPARAM(lp),_view);_view.editorSelectAll=false;_view.caretVisible=true;TickPrisme();}
        }
        else TickPrisme();
        return TRUE;
    }
    if(msg==WM_CAPTURECHANGED)
    {
        if(_pressDown){_pressDown=false;_pressAction=-1;_view.pressed=-1;TickPrisme();}
        return TRUE;
    }
    if(msg==WM_MOUSEWHEEL)
    {
        int delta=GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1;
        if(_view.overlay>=5)
        {
            int page=PanelSize();
            int count=PanelCount();
            _view.panelFirst=(std::clamp)(_view.panelFirst+delta,0,(std::max)(0,count-page));
        }
        else if(_view.overlay==2)_view.firstProcess=(std::max)(0,_view.firstProcess+delta);
        else if(!_view.overlay)_view.first=(std::max)(0,(std::min)(_view.first+delta,(std::max)(0,(int)_imagePaths.size()-2)));
        TickPrisme();return TRUE;
    }
    if(msg==WM_CHAR && _view.overlay==4)
    {
        if(!_view.editorFocused)return TRUE;
        if(wp>=32 && wp<65536 && wp!=127)TitleInput::Insert(_view.editorTitle,_view.editorCaret,_view.editorSelectAll,std::wstring(1,(wchar_t)wp));
        else if(wp==8)TitleInput::Erase(_view.editorTitle,_view.editorCaret,_view.editorSelectAll,true);
        _view.caretVisible=true;SetTimer(h,3,530,nullptr);TickPrisme();return TRUE;
    }
    if(msg==WM_CHAR && _view.overlay==2)
    {
        if(wp>=32 && wp<65536 && _view.query.size()<60)_view.query+=(wchar_t)wp;
        else if(wp==8 && !_view.query.empty())_view.query.pop_back();
        _view.firstProcess=0;TickPrisme();return TRUE;
    }
    if(msg==WM_KEYDOWN)
    {
        if(_view.overlay==10 && wp==VK_RETURN){PrismeAction(PrismeSurface::Back);return TRUE;}
        if(_view.overlay==8 && (wp==VK_UP || wp==VK_DOWN))
        {
            _versionChoice=(std::clamp)(_versionChoice+(wp==VK_UP?-1:1),0,(std::max)(0,(int)_view.versionItems.size()-1));RefreshVersionPanel();
            if(_versionChoice<_view.panelFirst)_view.panelFirst=_versionChoice;if(_versionChoice>=_view.panelFirst+3)_view.panelFirst=_versionChoice-2;
            TickPrisme();return TRUE;
        }
        if(_view.overlay==8 && wp==VK_RETURN && _view.focus<0){PrismeAction(PrismeSurface::UseVersion);return TRUE;}
        if(!_view.overlay && !_injecting && (wp==VK_UP || wp==VK_DOWN))
        {SelectCheat((std::max)(0,(std::min)((int)_imagePaths.size()-1,_selectedCheat+(wp==VK_UP?-1:1))));return TRUE;}
        if(!_view.overlay && !_injecting && wp==VK_DELETE && _selectedCheat>=0)
        {_view.selected=_selectedCheat;_view.overlay=3;DrawPrisme();return TRUE;}
        if(wp=='V' && (GetKeyState(VK_CONTROL)&0x8000) && _view.overlay==4 && _view.editorFocused)
        {
            if(OpenClipboard(h))
            {
                auto data=GetClipboardData(CF_UNICODETEXT);
                if(data)
                {
                    auto text=static_cast<const wchar_t*>(GlobalLock(data));
                    if(text)
                    {
                        TitleInput::Insert(_view.editorTitle,_view.editorCaret,_view.editorSelectAll,std::wstring(text,(std::min)(size_t(80),wcsnlen_s(text,GlobalSize(data)/sizeof(wchar_t)))));
                        _view.caretVisible=true;GlobalUnlock(data);
                    }
                }
                CloseClipboard();
            }
            TickPrisme();return TRUE;
        }
        if(wp==VK_RETURN && _view.overlay==4 && _view.editorFocused){CommitCheat();return TRUE;}
        if(wp=='A' && (GetKeyState(VK_CONTROL)&0x8000) && _view.overlay==4 && _view.editorFocused){_view.editorSelectAll=true;_view.editorCaret=(int)_view.editorTitle.size();_view.caretVisible=true;TickPrisme();return TRUE;}
        if(_view.overlay==4 && _view.editorFocused && (wp==VK_LEFT || wp==VK_RIGHT || wp==VK_HOME || wp==VK_END || wp==VK_DELETE))
        {
            auto& text=_view.editorTitle;auto& caret=_view.editorCaret;
            if(wp==VK_DELETE)TitleInput::Erase(text,caret,_view.editorSelectAll,false);
            else
            {
                caret=wp==VK_HOME?0:(wp==VK_END?(int)text.size():(wp==VK_LEFT?(_view.editorSelectAll?0:TitleInput::Previous(text,caret)):(_view.editorSelectAll?(int)text.size():TitleInput::Next(text,caret))));
                _view.editorSelectAll=false;
            }
            _view.caretVisible=true;SetTimer(h,3,530,nullptr);TickPrisme();return TRUE;
        }
        if(wp==VK_ESCAPE && WaitingForGame()){CancelLoader();return TRUE;}
        if(wp==VK_ESCAPE){PrismeAction(PrismeSurface::Back);return TRUE;}
        if(wp==VK_TAB)
        {
            auto order=_surface.FocusOrder();
            if(!order.empty())
            {
                int n=(int)order.size(),at=-1;for(int i=0;i<n;++i)if(order[i]==_view.focus)at=i;
                bool back=(GetKeyState(VK_SHIFT)&0x8000)!=0;
                at=at<0?(back?n-1:0):(at+(back?n-1:1))%n;
                _view.focus=order[at];if(_view.overlay==4){_view.editorFocused=_surface.hits[_view.focus].action==PrismeSurface::EditTitle;_view.caretVisible=true;}TickPrisme();return TRUE;
            }
        }
        if((wp==VK_RETURN||(wp==VK_SPACE && _view.overlay!=4)) && _view.focus>=0 && _view.focus<(int)_surface.hits.size())
        {PrismeAction(_surface.hits[_view.focus].action);return TRUE;}
        if(wp==VK_RETURN && _view.overlay==2)
        {for(auto& hit:_surface.hits)if(hit.action>=PrismeSurface::PickBase){PrismeAction(hit.action);break;}return TRUE;}
    }
    if(msg==WM_CHAR)return TRUE; // Never ask DefDlgProc to interpret text as a mnemonic.
    return FALSE;
}

int MainDlg::PanelSize() const {return _view.overlay==9?2:(_view.overlay==8 || _view.overlay==5?3:(_view.overlay==6?4:10));}
int MainDlg::PanelCount() const
{
    if(_view.overlay==9)return (int)_view.selectionItems.size();if(_view.overlay==8)return (int)_view.versionItems.size();
    if(_view.overlay==5)return (int)_view.previewItems.size();if(_view.overlay==6)return (int)_view.errorItems.size();return (int)_view.errorLines.size();
}
