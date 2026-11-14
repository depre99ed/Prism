#include "MainDlg.h"

#include <thread>
#include <future>
#include <shellapi.h>
#include <dwmapi.h>
#include "FirstRunNotice.h"
#pragma comment(lib,"dwmapi.lib")

MainDlg::MainDlg( MainDlg::StartAction action, const std::wstring& defConfig /*= L""*/ )
    : Dialog( IDD_MAIN )
    , _core( _hwnd )
    , _action( action )
    , _defConfig( defConfig )
{
    _messages[WM_TIMER] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnTimer);
    _messages[WM_NOTIFY] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnNotify);
    _customKeyboard=true;
    _messages[WM_BUNDLE_PROGRESS] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnBundleProgress);
    _messages[WM_BUNDLE_DONE] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnBundleDone);
    _messages[WM_INJECTION_DONE] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnInjectionDone);
    for (auto msg : {WM_PAINT,WM_ERASEBKGND,WM_LBUTTONDOWN,WM_LBUTTONDBLCLK,WM_LBUTTONUP,WM_CAPTURECHANGED,WM_NCCALCSIZE,WM_NCHITTEST,WM_MOUSEMOVE,WM_MOUSELEAVE,WM_MOUSEWHEEL,WM_KEYDOWN,WM_CHAR,WM_GETDLGCODE,WM_ACTIVATE,WM_SETCURSOR,WM_SETFOCUS,WM_KILLFOCUS})
        _messages[msg] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnPrismeInput);
    _events[IDC_AUTO_CS2] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnAutoCS2);
    _events[IDC_MENU] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnMenu);
    _events[IDC_LOCATE_MOD] = static_cast<Dialog::fnDlgProc>(&MainDlg::OnLocateImage);
    _messages[WM_INITDIALOG]        = static_cast<Dialog::fnDlgProc>(&MainDlg::OnInit);
    _messages[WM_COMMAND]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnCommand);
    _messages[WM_CLOSE]             = static_cast<Dialog::fnDlgProc>(&MainDlg::OnClose);
    _messages[WM_DROPFILES]         = static_cast<Dialog::fnDlgProc>(&MainDlg::OnDragDrop);

    _events[IDC_EXECUTE]            = static_cast<Dialog::fnDlgProc>(&MainDlg::OnExecute);
    _events[IDC_SETTINGS]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSettings);
    _events[IDC_NEW_PROC]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelectExecutable);
    _events[IDC_EXISTING_PROC]      = static_cast<Dialog::fnDlgProc>(&MainDlg::OnExistingProcess);
    _events[IDC_AUTO_PROC]          = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelectExecutable);
    _events[IDC_SELECT_PROC]        = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelectExecutable);
    _events[IDC_ADD_MOD]            = static_cast<Dialog::fnDlgProc>(&MainDlg::OnLoadImage);
    _events[IDC_REMOVE_MOD]         = static_cast<Dialog::fnDlgProc>(&MainDlg::OnRemoveImage);
    _events[IDC_CLEAR_MODS]         = static_cast<Dialog::fnDlgProc>(&MainDlg::OnClearImages);
    _events[CBN_DROPDOWN]           = static_cast<Dialog::fnDlgProc>(&MainDlg::OnDropDown);
    _events[CBN_SELCHANGE]          = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSelChange);
    _events[ID_PROFILES_OPEN]       = static_cast<Dialog::fnDlgProc>(&MainDlg::OnLoadProfile);
    _events[ID_PROFILES_SAVE]       = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSaveProfile);
    _events[ID_TOOLS_PROTECT]       = static_cast<Dialog::fnDlgProc>(&MainDlg::OnProtectSelf);
    _events[ID_TOOLS_EJECTMODULES]  = static_cast<Dialog::fnDlgProc>(&MainDlg::OnEjectModules);

    // Accelerators
    _events[ID_ACCEL_OPEN]          = static_cast<Dialog::fnDlgProc>(&MainDlg::OnLoadProfile);
    _events[ID_ACCEL_SAVE]          = static_cast<Dialog::fnDlgProc>(&MainDlg::OnSaveProfile);
}

MainDlg::~MainDlg()
{
    if (_worker.joinable()) _worker.join();
    ReleaseRestartTargets();
}

INT_PTR MainDlg::OnInit( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    Dialog::OnInit( hDlg, message, wParam, lParam );

    //
    // Setup controls
    //
    _procList.Attach( _hwnd, IDC_COMBO_PROC );
    _exProc.Attach( _hwnd, IDC_EXISTING_PROC );
    _newProc.Attach( _hwnd, IDC_NEW_PROC );
    _autoProc.Attach(_hwnd, IDC_AUTO_PROC );
    _selectProc.Attach( _hwnd, IDC_SELECT_PROC );
    _modules.Attach( _hwnd, IDC_MODS );
    _inject.Attach( _hwnd, IDC_EXECUTE );

    // Setup modules view
    RECT listRect; GetClientRect( _modules.hwnd(), &listRect );
    const int width = listRect.right;
    _modules.AddColumn( L"DLL", width * 52 / 100, 0 );
    _modules.AddColumn( L"Architecture", width * 20 / 100, 1 );
    _modules.AddColumn( L"Status", width * 27 / 100, 2 );

    ListView_SetExtendedListViewStyle( _modules.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES | LVS_EX_INFOTIP );

    // Set dialog title
    SetWindowTextW( _hwnd, L"Prism" );

    // UAC drop-down bypass
    ChangeWindowMessageFilterEx( hDlg, WM_DROPFILES, MSGFLT_ADD, nullptr );
    ChangeWindowMessageFilterEx( hDlg, 0x49/*WM_COPYGLOBALDATA*/, MSGFLT_ADD, nullptr );

    // Protection option
    if (blackbone::Driver().loaded())
        EnableMenuItem( GetMenu( _hwnd ), ID_TOOLS_PROTECT, MF_ENABLED );

    // Set icon
    HICON hIcon = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_ICON1 ) );
    SendMessage( hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
    SendMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
    DestroyIcon( hIcon );

    // Setup status bar
    _status.Attach( CreateWindowW ( STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 10, hDlg, NULL, GetModuleHandle( NULL ), NULL ) );

    _status.SetParts( { 150, 200, -1 } );
    _status.SetText( 0, L"Current profile" );

#ifdef USE64
    _status.SetText( 1, L"x64" );
#else
    _status.SetText( 1, L"x86" );
#endif
    _status.SetText( 2, L"Ready" );

    EnumChildWindows(_hwnd,[](HWND child,LPARAM)->BOOL {ShowWindow(child,SW_HIDE);return TRUE;},0);
    HDC screen=GetDC(_hwnd);_uiScale=GetDeviceCaps(screen,LOGPIXELSX)/96.0f;ReleaseDC(_hwnd,screen);
    RECT desktop;SystemParametersInfoW(SPI_GETWORKAREA,0,&desktop,0);
    int surfaceWidth=static_cast<int>(PrismeSurface::Width*_uiScale),surfaceHeight=static_cast<int>(PrismeSurface::Height*_uiScale);
    SetWindowPos(_hwnd,nullptr,(desktop.right-surfaceWidth)/2,(desktop.bottom-surfaceHeight)/2,surfaceWidth,surfaceHeight,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    // The window is its own drawing: no system frame, shadow or rounding around the glass.
    LONG_PTR style=GetWindowLongPtrW(_hwnd,GWL_STYLE);SetWindowLongPtrW(_hwnd,GWL_STYLE,style&~static_cast<LONG_PTR>(WS_CAPTION));
    LONG_PTR exStyle=GetWindowLongPtrW(_hwnd,GWL_EXSTYLE);SetWindowLongPtrW(_hwnd,GWL_EXSTYLE,exStyle&~static_cast<LONG_PTR>(WS_EX_WINDOWEDGE|WS_EX_DLGMODALFRAME|WS_EX_CLIENTEDGE));
    { DWMNCRENDERINGPOLICY policy=DWMNCRP_DISABLED;DwmSetWindowAttribute(_hwnd,DWMWA_NCRENDERING_POLICY,&policy,sizeof(policy));
      DWORD corners=1/*DWMWCP_DONOTROUND*/;DwmSetWindowAttribute(_hwnd,33/*DWMWA_WINDOW_CORNER_PREFERENCE*/,&corners,sizeof(corners)); }
    SetWindowPos(_hwnd,nullptr,(desktop.right-surfaceWidth)/2,(desktop.bottom-surfaceHeight)/2,surfaceWidth,surfaceHeight,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    _errors.Load(PrismeDataDirectory()+L"\\Diagnostics");
    _prismeReady=_surface.Init(_uiScale);
    if(!_prismeReady){MessageBoxW(_hwnd,L"Unable to initialize the renderer.",L"Prism",MB_ICONERROR);PostMessageW(_hwnd,WM_CLOSE,0,0);}
    LoadConfig( _defConfig );
    CheckDlgButton( _hwnd, IDC_AUTO_CS2, _profileMgr.config().autoTargetCs2 ? BST_CHECKED : BST_UNCHECKED );
    RefreshTarget();
    RefreshLibrary();
    SetTimer( _hwnd, 1, 1000, nullptr );
    if(_prismeReady)
    {
        auto notice=FirstRunNotice::Claim(PrismeDataDirectory());
        if(notice!=FirstRunNotice::Result::Seen)_view.overlay=10;
        if(notice==FirstRunNotice::Result::Error)_loaderStatus=L"Unable to save the first-run notice preference.";
    }
    DrawPrisme();
    SetFocus(_hwnd);
    return FALSE;
}

INT_PTR MainDlg::OnClose( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if(WaitingForGame())CancelLoader();
    if (_injecting) return TRUE;
    KillTimer( _hwnd, 1 );
    SaveConfig();

    return Dialog::OnClose( hDlg, message, wParam, lParam );
}

INT_PTR MainDlg::OnDropDown( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (LOWORD( wParam ) == IDC_COMBO_PROC)
    {
        DWORD pid = _lastAutoPid;
        FillProcessList();
        if (_profileMgr.config().autoTargetCs2 && pid && pid != MAXDWORD)
            for(int i=0;i<ComboBox_GetCount(_procList.hwnd());++i)
                if(static_cast<DWORD>(_procList.itemData(i))==pid) _procList.selection(i);
    }

    return TRUE;
}

INT_PTR MainDlg::OnSelChange( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    // Process selection
    if (LOWORD( wParam ) == IDC_COMBO_PROC)
    {
        if (_procList.selection() < 0) return TRUE;
        _profileMgr.config().autoTargetCs2 = false;
        CheckDlgButton( _hwnd, IDC_AUTO_CS2, BST_UNCHECKED );
        DWORD pid = (DWORD)_procList.itemData( _procList.selection() );
        auto path = _procList.selectedText().substr( 0, _procList.selectedText().find( L'(' ) - 1 );

        SetActiveProcess( pid, path );
        SetDlgItemTextW(_hwnd,IDC_TARGET_STATE,L"Manual process selection");
        UpdateInterface();
    }

    return TRUE;
}

INT_PTR MainDlg::OnLoadImage( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    std::wstring path;
    EnableWindow( GetDlgItem( hDlg, IDC_ADD_MOD ), FALSE );
    bool res = OpenSaveDialog(
        L"All (*.*)\0*.*\0Dynamic link library (*.dll)\0*.dll\0System driver (*.sys)\0*.sys\0",
        (_profileMgr.config().injectMode == Kernel_DriverMap) ? 3 : 2,
        path 
        );

    // Reset init routine upon load
    if (res && LoadImageFile( path ) == ERROR_SUCCESS)
        _profileMgr.config().initRoutine = L"";

    EnableWindow( GetDlgItem( hDlg, IDC_ADD_MOD ), TRUE );
    SaveConfig();
    RefreshLibrary();
    return TRUE;
}

INT_PTR MainDlg::OnRemoveImage( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    auto idx = _modules.selection();
    if (idx >= 0)
    {
        std::vector<std::wstring> copies;if(idx<(int)_versions.size())for(const auto& item:_versions[idx].items)copies.push_back(item.path);
        _modules.RemoveItem( idx );
        _images.erase( _images.begin() + idx );
        _exports.erase( _exports.begin() + idx );
        if(idx<(int)_versions.size())_versions.erase(_versions.begin()+idx);
        if(idx<(int)_cheatTitles.size())_cheatTitles.erase(_cheatTitles.begin()+idx);
        if(idx<(int)_cheatArtwork.size())_cheatArtwork.erase(_cheatArtwork.begin()+idx);
        if(_selectedCheat>idx)--_selectedCheat;
        else if(_selectedCheat==idx)_selectedCheat=-1;
        _imagePaths.erase( _imagePaths.begin() + idx );
        _imageEnabled.erase( _imageEnabled.begin() + idx );
        SyncCatalogue();
        if(SaveConfig()==ERROR_SUCCESS)DeleteStoredCopies(copies);
        else Message::ShowError(_hwnd,L"The entry was removed for this session, but its catalogue could not be saved. Stored copies were kept.");
        RefreshLibrary();
    }

    // Disable inject if no images
    if (_images.empty())
        _inject.disable();

    return TRUE;
}

INT_PTR MainDlg::OnClearImages( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    std::vector<std::wstring> copies;for(const auto& group:_versions)for(const auto& item:group.items)copies.push_back(item.path);
    _profileMgr.config().images.clear();
    _imagePaths.clear();
    _imageEnabled.clear();
    _cheatTitles.clear();_cheatArtwork.clear();_versions.clear();_selectedCheat=-1;
    _images.clear();
    _exports.clear();
    _modules.reset();
    _inject.disable();
    if(SaveConfig()==ERROR_SUCCESS)DeleteStoredCopies(copies);
    else Message::ShowError(_hwnd,L"The catalogue could not be saved. Stored copies were kept.");
    RefreshLibrary();

    return TRUE;
}

INT_PTR MainDlg::OnExecute(HWND,UINT,WPARAM,LPARAM)
{
    StartLoader();return TRUE;
}

INT_PTR MainDlg::OnSettings( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    blackbone::pe::vecExports diff, tmp;

    // Get mutual exports for all modules
    bool firstSelected = true;
    for (size_t i = 0; i < _exports.size(); ++i)
    {
        if (!_imageEnabled[i] || !_images[i]) continue;
        if (firstSelected)
        {
            diff = tmp = _exports[i];
            firstSelected = false;
            continue;
        }
        diff.clear();
        std::set_intersection( _exports[i].begin(), _exports[i].end(), tmp.begin(), tmp.end(), std::inserter( diff, diff.begin() ) );
        tmp = diff;
    }

    DlgSettings dlg( _profileMgr );
    dlg.SetExports( diff );

    _profileMgr.config().processMode = (_exProc ? Existing : (_newProc ? NewProcess : ManualLaunch));

    dlg.RunModal( hDlg );
    UpdateInterface();
    return TRUE;
}

INT_PTR MainDlg::OnExistingProcess( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    _profileMgr.config().processMode = Existing;
    FillProcessList();
    UpdateInterface();
    return TRUE;
}

INT_PTR MainDlg::OnSelectExecutable( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    std::wstring path;
    _profileMgr.config().autoTargetCs2 = false;
    CheckDlgButton(_hwnd,IDC_AUTO_CS2,BST_UNCHECKED);
    SetDlgItemTextW(_hwnd,IDC_TARGET_STATE,L"Manual process selection");
    EnableWindow( GetDlgItem( hDlg, IDC_SELECT_PROC ), FALSE );
    if (OpenSaveDialog( L"All (*.*)\0*.*\0Executable image (*.exe)\0*.exe\0", 2, path ))
        SetActiveProcess( 0, path );
    else
        _procList.reset();

    _profileMgr.config().processMode = _newProc.checked() ? NewProcess : ManualLaunch;
    UpdateInterface();
    EnableWindow( GetDlgItem( hDlg, IDC_SELECT_PROC ), TRUE );
    return TRUE;
}

INT_PTR MainDlg::OnDragDrop(HWND,UINT,WPARAM wp,LPARAM)
{
    HDROP drop=reinterpret_cast<HDROP>(wp);wchar_t path[MAX_PATH]={};
    bool found=DragQueryFileW(drop,0,path,MAX_PATH)!=0;DragFinish(drop);
    if(_injecting || !found)return TRUE;
    std::wstring dropped=path;auto dot=dropped.rfind(L'.');
    if(dot!=dropped.npos && _wcsicmp(dropped.substr(dot).c_str(),L".zip")==0){PreviewBundle(dropped);return TRUE;}
    if(!ValidateCheat(path))return TRUE;
    size_t versionCount=0;for(const auto& group:_versions)versionCount+=group.items.size();
    if(versionCount>=1000){Message::ShowInfo(_hwnd,L"The catalogue can contain up to 1000 DLL versions.");return TRUE;}
    if(LoadImageFile(path)==ERROR_SUCCESS)
    {
        SyncCatalogue();SelectCheat(static_cast<int>(_imagePaths.size())-1);
        try{PreserveVersions(_selectedCheat);}catch(...){_loaderStatus=L"DLL added, but its version could not be copied yet.";}
        EditCheat(false);
    }
    return TRUE;
}

INT_PTR MainDlg::OnLoadProfile(HWND,UINT,WPARAM,LPARAM)
{
    if(_injecting)return TRUE;
    std::wstring path;
    if(OpenSaveDialog(L"Prism catalogues (*.zip;*.xpr;*.xpr64)\0*.zip;*.xpr;*.xpr64\0",1,path))
    {
        auto dot=path.rfind(L'.');
        if(dot!=std::wstring::npos && _wcsicmp(path.substr(dot).c_str(),L".zip")==0)PreviewBundle(path);
        else if(ApplyCatalogue(path)){_loaderStatus=L"Catalogue imported.";DrawPrisme();}
    }
    return TRUE;
}

INT_PTR MainDlg::OnSaveProfile(HWND,UINT,WPARAM,LPARAM)
{
    if(_injecting)return TRUE;
    if(_imagePaths.empty()){_loaderStatus=L"Add a cheat before exporting.";DrawPrisme();return TRUE;}
    PrepareExport(false);DrawPrisme();
    return TRUE;
}

INT_PTR MainDlg::OnProtectSelf( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    NTSTATUS status = STATUS_SUCCESS;

    // Protect current process
    if (blackbone::Driver().loaded())
    {
        //status = blackbone::Driver().ProtectProcess( GetCurrentProcessId(), true );
        status |= blackbone::Driver().UnlinkHandleTable( GetCurrentProcessId() );
    }

    if (!NT_SUCCESS( status ))
        Message::ShowError( hDlg, L"Failed to protect current process.\n" + blackbone::Utils::GetErrorDescription( status ) );
    else
        Message::ShowInfo( hDlg, L"Successfully protected" );

    return TRUE;
}


INT_PTR MainDlg::OnEjectModules( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    auto pid = _procList.itemData( _procList.selection() );

    // Try attaching to current selection
    if (pid > 0 && _core.process().pid() != pid)
    {
        auto status = _core.process().Attach( pid );
        if (status != STATUS_SUCCESS)
        {
            std::wstring errmsg = L"Can not attach to process.\n" + blackbone::Utils::GetErrorDescription( status );
            Message::ShowError( hDlg, errmsg );
            return TRUE;
        }
    }

    if (!_core.process().valid())
    {
        Message::ShowError( hDlg, L"Please select valid process before unloading modules" );
        return TRUE;
    }

    ModulesDlg dlg( _core.process() );
    return dlg.RunModal( hDlg );
}
