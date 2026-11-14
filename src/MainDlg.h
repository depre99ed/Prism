#pragma once

#include "resource.h"
#include "DlgModules.h"
#include "DlgWait.h"
#include "DlgSettings.h"
#include "ProfileMgr.h"
#include "ComboBox.hpp"
#include "EditBox.hpp"
#include "Button.hpp"
#include "StatusBar.hpp"
#include "Message.hpp"
#include "InjectionCore.h"

#include "Log.h"
#include "PrismeSurface.h"
#include "LoaderFlow.h"
#include "RestartFlow.h"
#include "CatalogueArchive.h"
#include "CatalogueModel.h"
#include "ErrorJournal.h"
#include <set>
#include <atomic>
#include <thread>

class MainDlg : public Dialog
{
public:
    enum StartAction
    {
        Nothing = 0,
        LoadProfile,
        RunProfile
    };

public:
    MainDlg( StartAction action, const std::wstring& defConfig = L"" );
    ~MainDlg();

    /// <summary>
    /// Run injection profile
    /// </summary>
    /// <returns>Error code</returns>
    int LoadAndInject()
    {
        LoadConfig( _defConfig );
        Inject();
        return 0;
    }

private:
    /// <summary>
    /// Load configuration from file
    /// </summary>
    /// <returns>Error code</returns>
    DWORD LoadConfig( const std::wstring& path = L"" );

    /// <summary>
    /// Save Configuration.
    /// </summary>
    /// <returns>Error code</returns>
    DWORD SaveConfig( const std::wstring& path = L"" );

    /// <summary>
    /// Async inject routine
    /// </summary>
    void Inject();

    /// <summary>
    /// Enumerate processes
    /// </summary>
    /// <returns>Error code</returns>
    DWORD FillProcessList();

    /// Set current process
    /// </summary>
    /// <param name="pid">Target PID</param>
    /// <param name="path">Process path.</param>
    /// <returns>Error code</returns>
    DWORD SetActiveProcess( DWORD pid, const std::wstring& name );

    /// <summary>
    /// Update interface controls
    /// </summary>
    /// <returns>Error code</returns>
    DWORD UpdateInterface();

    /// <summary>
    /// Load selected image and do some validation
    /// </summary>
    /// <param name="path">Full qualified image path</param>
    /// <returns>Error code </returns>
    DWORD LoadImageFile( const std::wstring& path );

    /// <summary>
    /// Add module to module list
    /// </summary>
    /// <param name="path">Loaded image</param>
    void AddToModuleList( std::shared_ptr<blackbone::pe::PEImage>& img );

    /// <summary>
    /// Invoke Open/Save file dialog
    /// </summary>
    /// <param name="filter">File filter</param>
    /// <param name="defIndex">Default filter index</param>
    /// <param name="selectedPath">Target file path</param>
    /// <param name="bSave">true to Save file, false to open</param>
    /// <param name="defExt">Default file extension for file save</param>
    /// <returns>true on success, false if operation was canceled by user</returns>
    bool OpenSaveDialog(
        const wchar_t* filter,
        int defIndex,
        std::wstring& selectedPath,
        bool bSave = false,
        const std::wstring& defExt = L""
        );

    //
    // Message handlers
    //
    MSG_HANDLER( OnInit );
    MSG_HANDLER( OnCommand );
    MSG_HANDLER( OnTheme );
    MSG_HANDLER( OnTimer );
    MSG_HANDLER( OnNotify );
    MSG_HANDLER( OnAutoCS2 );
    MSG_HANDLER( OnLocateImage );
    MSG_HANDLER( OnInjectionDone );
    MSG_HANDLER( OnMenu );
    void RefreshTarget();
    void RefreshLibrary();
    void SetBusy( bool busy );
    bool HasReadySelection() const;
    static const UINT WM_INJECTION_DONE = WM_APP + 51;
    static const UINT WM_BUNDLE_PROGRESS = WM_APP + 52, WM_BUNDLE_DONE = WM_APP + 53;
    MSG_HANDLER(OnBundleProgress);
    MSG_HANDLER(OnBundleDone);
    int _bundleBusy=0,_bundlePercent=0;
    std::wstring _lastExportZip;
    void BeginBundle(bool importing,const std::wstring& path);
    void PrepareExport(bool diagnostics);
    void ConfirmExport();
    void BeginReportExport(const std::wstring& path,std::vector<CatalogueArchive::Report> reports);
    bool _exportDiagnostics=false;
    ProfileMgr::ConfigData _exportSnapshot,_exportCatalogue;
    std::vector<std::pair<int,int>> _exportRows;
    std::vector<Diagnostics::Entry> _exportReports;
    void PreviewBundle(const std::wstring& path);
    CatalogueArchive::PreviewInfo _pendingPreview;
    std::wstring _pendingZip;
    Diagnostics::Journal _errors;
    std::wstring _attemptStage,_attemptFacts;
    std::vector<std::string> _attemptTrace;
    void ShowErrors();
    void ShowErrorDetail(int index);
    void RecordError(const std::wstring& title,const std::wstring& stage,unsigned long code,bool native,
        const std::wstring& facts=L"",DWORD pid=0,uint64_t identity=0);
    void CopyError();
    bool ApplyCatalogue(const std::wstring& path);
    void RevealZip();
    PrismeSurface _surface;
    PrismeState _view;
    bool _prismeReady = false;
    float _uiScale = 1;
    MSG_HANDLER(OnPrismeInput);
    void DrawPrisme();
    void TickPrisme();
    int _pressAction = -1;
    bool _pressDown = false, _ticking = false, _caretTicking=false;
    void PrismeAction(int action);
    int PanelSize() const;
    int PanelCount() const;
    std::vector<std::wstring> _imagePaths;
    std::vector<bool> _imageEnabled;
    std::vector<std::wstring> _cheatTitles,_cheatArtwork;
    std::vector<ProfileMgr::VersionGroup> _versions;
    int _versionCheat=-1,_versionChoice=0,_editingVersion=-1;
    std::wstring _editVersionPath;
    void PreserveVersions(int index);
    void DeleteStoredCopies(const std::vector<std::wstring>& candidates);
    void ShowVersions(int index);
    void RefreshVersionPanel();
    bool ActivateVersion(int cheat,int version,bool save=true);
    void EditVersion(bool add);
    void CommitVersion();
    void RemoveVersion();
    int _selectedCheat=-1,_editingCheat=-1;
    std::wstring _editDllPath,_editArtwork,_loaderStatus;
    LoaderFlow _loader;
    RestartFlow _restart;
    std::vector<std::pair<HANDLE,DWORD>> _restartTargets;
    std::set<HWND> _closeRequestedWindows;
    bool WaitingForGame() const {return _restart.active() || _loader.waiting();}
    void ReleaseRestartTargets();
    void LaunchFreshGame();
    bool TickRestart();
    std::set<std::pair<uint64_t,std::wstring>> _loadedCheats;
    std::wstring _loadingPath;
    void SyncCatalogue();
    void EditCheat(bool add);
    void CommitCheat();
    void ChooseArtwork();
    void StartLoader();
    void TickLoader();
    void CancelLoader();
    bool ValidateCheat(const std::wstring& path,bool showError=true);
    void SelectCheat(int index);
    void ClearLoaderStatus();
    bool _loadingLibrary = false;
    std::atomic<bool> _injecting{false};
    std::thread _worker;
    DWORD _lastAutoPid = MAXDWORD;
    MSG_HANDLER( OnClose );
    MSG_HANDLER( OnLoadImage );
    MSG_HANDLER( OnRemoveImage );
    MSG_HANDLER( OnClearImages );
    MSG_HANDLER( OnSelectExecutable );
    MSG_HANDLER( OnExecute );
    MSG_HANDLER( OnSettings);
    MSG_HANDLER( OnDropDown );
    MSG_HANDLER( OnSelChange );
    MSG_HANDLER( OnDragDrop );
    MSG_HANDLER( OnExistingProcess );

    MSG_HANDLER( OnLoadProfile );
    MSG_HANDLER( OnSaveProfile );

    MSG_HANDLER( OnEjectModules );
    MSG_HANDLER( OnProtectSelf );

private:
    StartAction     _action;        // Starting action
    std::wstring    _defConfig;     // Profile to load on start
    InjectionCore   _core;          // Injection implementation
    ProfileMgr      _profileMgr;    // Configuration manager
    std::wstring    _processPath;   // Target process path
    vecPEImages     _images;        // Loaded module exports
    vecImageExports _exports;       // Exports list
    std::mutex      _lock;          // Injection lock

    //
    // Interface controls
    //  
    ctrl::ComboBox _procList;       // Process list
    ctrl::ListView _modules;        // Image list

    ctrl::StatusBar _status;        // Status bar

    ctrl::Button _exProc;           // Existing process radio-button
    ctrl::Button _newProc;          // New process radio-button
    ctrl::Button _autoProc;         // Manual launch radio-button
    ctrl::Button _selectProc;       // Select process path
    ctrl::Button _inject;           // Inject button
};
