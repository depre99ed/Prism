#include "MainDlg.h"
#include "DriverExtract.h"

/// <summary>
/// Load configuration from file
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::LoadConfig( const std::wstring& path /*= L""*/ )
{
    auto& cfg = _profileMgr.config();
    if (_profileMgr.Load( path ))
    {
        // Image
        _loadingLibrary = true;
        for (size_t i = 0; i < cfg.images.size(); ++i)
        {
            const auto& path = cfg.images[i];
            bool enabled = i >= cfg.imageEnabled.size() || cfg.imageEnabled[i];
            if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES || LoadImageFile(path) != ERROR_SUCCESS)
            {
                _imagePaths.push_back(path);
                _imageEnabled.push_back(enabled);
                _images.push_back(nullptr);
                _exports.emplace_back();
                _modules.AddItem(blackbone::Utils::StripPath(path),0,{L"--",L"Missing"});
            }
            _imageEnabled.back() = enabled;
            ListView_SetCheckState(_modules.hwnd(),static_cast<int>(_imagePaths.size()-1),enabled);
        }
        _loadingLibrary = false;
        CheckDlgButton(_hwnd,IDC_AUTO_CS2,cfg.autoTargetCs2 ? BST_CHECKED : BST_UNCHECKED);
        _lastAutoPid = MAXDWORD;

        _cheatTitles=cfg.imageTitles;_cheatArtwork=cfg.imageArtwork;_versions=cfg.versionGroups;
        _selectedCheat=cfg.selectedCheat;
        if(_selectedCheat<0)
            for(size_t i=0;i<_imageEnabled.size();++i)if(_imageEnabled[i]){_selectedCheat=static_cast<int>(i);break;}
        SyncCatalogue();
        // The catalogue loader uses the native engine with an existing process.
        cfg.injectMode=Normal;cfg.processMode=Existing;cfg.autoTargetCs2=true;
        // Process
        if (!cfg.procName.empty())
        {
            if (cfg.processMode != Existing)
            {
                SetActiveProcess( 0, cfg.procName );
            }
            else
            {
                std::vector<DWORD> pidList = blackbone::Process::EnumByName( cfg.procName );
                if (!pidList.empty())
                {
                    cfg.pid = pidList.front();

                    auto idx = _procList.Add( cfg.procName + L" (" + std::to_wstring( pidList.front() ) + L")", pidList.front() );
                    _procList.selection( idx );

                    SetActiveProcess( pidList.front(), cfg.procName );
                }
            }
        }

        // Process mode
        _exProc.checked( false );
        _newProc.checked( false );
        _autoProc.checked( false );
        switch (cfg.processMode)
        {
            case Existing:
                _exProc.checked( true );
                break;
            case NewProcess:
                _newProc.checked( true );
                break;
            case ManualLaunch:
                _autoProc.checked( true );
                break;
        }

        // Fix injection method
        if (cfg.injectMode >= Kernel_Thread)
        {
            DriverExtract::Instance().Extract();
            if (!NT_SUCCESS( blackbone::Driver().EnsureLoaded(DriverExtract::Instance().Path()) ))
            {
                DriverExtract::Instance().Cleanup();
                cfg.injectMode = Normal;
            }
        }

        // Update profile name
        if (!_defConfig.empty())
            _status.SetText( 0, blackbone::Utils::StripPath( _defConfig ) );
        // Preserve existing valid entries before an external update can replace their source files.
        for(size_t i=0;i<_imagePaths.size();++i)if(_images[i])
        {
            try {PreserveVersions((int)i);}
            catch(...) {_loaderStatus=L"Some existing DLLs could not be copied into the version catalogue.";}
        }
    }
    // Default settings
    else
    {
        _exProc.checked( true );
    }

    UpdateInterface();
    return ERROR_SUCCESS;
}


/// <summary>
/// Save Configuration.
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::SaveConfig( const std::wstring& path /*= L""*/ )
{
    auto& cfg = _profileMgr.config();

    cfg.procName = _processPath;
    cfg.images = _imagePaths;
    cfg.imageEnabled = _imageEnabled;
    cfg.imageTitles=_cheatTitles;cfg.imageArtwork=_cheatArtwork;cfg.selectedCheat=_selectedCheat;cfg.versionGroups=_versions;

    cfg.processMode = (_exProc ? Existing : (_newProc ? NewProcess : ManualLaunch));

    return _profileMgr.Save(path)?ERROR_SUCCESS:ERROR_WRITE_FAULT;
}

/// <summary>
/// Enumerate processes
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::FillProcessList()
{
    DWORD previous = _procList.selection() >= 0 ? static_cast<DWORD>(_procList.itemData(_procList.selection())) : 0;
    _procList.reset();

    auto found = blackbone::Process::EnumByNameOrPID( 0, L"" ).result( std::vector<blackbone::ProcessInfo>() );
    for (auto& proc : found)
    {
        wchar_t text[255] = { 0 };
        swprintf_s( text, L"%ls (%d)", proc.imageName.c_str(), proc.pid );

        int index = _procList.Add( text, proc.pid );
        if (proc.pid == previous) _procList.selection(index);
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Set current process
/// </summary>
/// <param name="pid">Target PID</param>
/// <param name="path">Process path.</param>
/// <returns>Error code</returns>
DWORD MainDlg::SetActiveProcess( DWORD pid, const std::wstring& path )
{
    _processPath = path;
    if (pid == 0)
    {
        // Update process list
        _procList.reset();
        _procList.Add( blackbone::Utils::StripPath( path ), 0 );
        _procList.selection( 0 );
    }

    return ERROR_SUCCESS;
}

/// <summary>
/// Async inject routine
/// </summary>
void MainDlg::Inject()
{
    _inject.disable();

    InjectContext context;
    auto& cfg = _profileMgr.config();
    NTSTATUS status = STATUS_SUCCESS;

    // Fill in context
    context.cfg = cfg;
    for (size_t i = 0; i < _images.size(); ++i)
        if (_imageEnabled[i] && _images[i]) context.images.push_back(_images[i]);
    context.pid = _procList.itemData( _procList.selection() );
    context.procPath = _processPath;
    context.skippedCount = 0;
    context.procList.clear();
    context.procDiff.clear();

    _status.SetText( 2, L"Loading..." );

    // Show wait dialog
    if (context.cfg.processMode == ManualLaunch)
    {
        DlgWait dlgWait( _core, context );
        dlgWait.RunModal( _hwnd );
        status = dlgWait.status();
    }
    else
    {
        status = _core.InjectMultiple( &context );
    }

    if (_hwnd)
        PostMessageW(_hwnd,WM_INJECTION_DONE,NT_SUCCESS(status) ? TRUE : FALSE,0);
}

/// <summary>
/// Update interface controls
/// </summary>
/// <returns>Error code</returns>
DWORD MainDlg::UpdateInterface( )
{
    // Reset controls state
    if (_exProc.checked())
    {
        _procList.enable();
        _selectProc.disable();
    }
    else
    {
        _procList.disable();
        _selectProc.enable();
    }

    if (_profileMgr.config().injectMode == Kernel_DriverMap)
    {
        _exProc.disable();
        _newProc.disable();
        _autoProc.disable();
        _procList.disable();
        _selectProc.disable();
    }
    else
    {
        _exProc.enable();
        _newProc.enable();
        _autoProc.enable();
    }

    // Disable inject button if no images available or no process selected
    if (_injecting || !HasReadySelection() ||
        (_profileMgr.config().injectMode != Kernel_DriverMap && _procList.selection() == -1))
        _inject.disable();
    else
        _inject.enable();
    DrawPrisme();
    return ERROR_SUCCESS;
}

/// <summary>
/// Invoke Open/Save file dialog
/// </summary>
/// <param name="filter">File filter</param>
/// <param name="defIndex">Default filter index</param>
/// <param name="selectedPath">Target file path</param>
/// <param name="bSave">true to Save file, false to open</param>
/// <param name="defExt">Default file extension for file save</param>
/// <returns>true on success, false if operation was canceled by user</returns>
bool MainDlg::OpenSaveDialog( 
    const wchar_t* filter, 
    int defIndex, 
    std::wstring& selectedPath,
    bool bSave /*= false*/, 
    const std::wstring& defExt /*= L""*/
    )
{
    OPENFILENAMEW ofn = { 0 };
    wchar_t path[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = _hwnd;
    ofn.lpstrFile = path;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = defIndex;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    if (!bSave)
    {
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    }
    else
    {
        ofn.Flags = OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = defExt.c_str();
    }

    auto res = bSave ? GetSaveFileNameW( &ofn ) : GetOpenFileNameW( &ofn );
    if (res)
        selectedPath = path;

    return res != FALSE;
}

/// <summary>
/// Load selected image and do some validation
/// </summary>
/// <param name="path">Full qualified image path</param>
/// <returns>Error code </returns>
DWORD MainDlg::LoadImageFile( const std::wstring& path )
{
    std::shared_ptr<blackbone::pe::PEImage> img( new blackbone::pe::PEImage );
    blackbone::pe::vecExports exports;

    // Check if image is already in the list
    if (std::find_if( _imagePaths.begin(), _imagePaths.end(),
        [&path]( const std::wstring& saved ) { return _wcsicmp(path.c_str(), saved.c_str()) == 0; } ) != _imagePaths.end())
    {
        Message::ShowInfo( _hwnd, L"Image '" + path + L"' is already in the list" );
        return ERROR_ALREADY_EXISTS;
    }

    // Check if image is a PE file
    if (!NT_SUCCESS( img->Load( path ) ))
    {
        std::wstring errstr = std::wstring( L"File \"" ) + path + L"\" is not a valid PE image";
        Message::ShowError( _hwnd, errstr.c_str() );

        img->Release();
        return ERROR_INVALID_IMAGE_HASH;
    }

    // In case of pure IL, list all methods
    if (img->pureIL() && img->net().Init( path ))
    {
        blackbone::ImageNET::mapMethodRVA methods;
        img->net().Parse( &methods );

        for (auto& entry : methods)
        {
            std::wstring name = entry.first.first + L"." + entry.first.second;
            exports.push_back( blackbone::pe::ExportData( blackbone::Utils::WstringToAnsi( name ), 0 ) );
        }
    }
    // Simple exports otherwise
    else
        img->GetExports( exports );

    
    // Add to internal lists
    AddToModuleList( img );
    _exports.emplace_back( exports );

    if (!_loadingLibrary) RefreshLibrary();

    img->Release( true );
    return ERROR_SUCCESS;
}

/// <summary>
/// Add module to module list
/// </summary>
/// <param name="path">Loaded image</param>
/// <param name="exports">Module exports</param>
void MainDlg::AddToModuleList( std::shared_ptr<blackbone::pe::PEImage>& img )
{
    wchar_t* platfom = nullptr;

    // Module platform
    if (img->mType() == blackbone::mt_mod32)
        platfom = L"32 bit";
    else if (img->mType() == blackbone::mt_mod64)
        platfom = L"64 bit";
    else
        platfom = L"Unknown";

    _images.emplace_back( img );
    _imagePaths.push_back(img->path());
    _imageEnabled.push_back(true);
    const bool wasLoading = _loadingLibrary;
    _loadingLibrary = true;
    int row = _modules.AddItem( blackbone::Utils::StripPath( img->path() ), 0, { platfom, L"Available" } );
    ListView_SetCheckState(_modules.hwnd(),row,TRUE);
    _loadingLibrary = wasLoading;
}
