#pragma once

#include <windows.h>
#include <string>
#include <vector>

// One selectable entry of the loader. Every cheat lives in its own folder
// under %LOCALAPPDATA%\Prisme\Cheats so moving the folder moves the cheat.
struct CheatEntry
{
    std::wstring id;        // folder name, stable across renames
    std::wstring title;     // display name
    std::wstring dll;       // absolute path to the payload
    std::wstring icon;      // absolute path to the artwork, may be empty
    std::wstring folder;    // absolute path to the entry folder
    std::wstring hash;      // FNV-1a of the payload, for duplicate detection
    bool missing = false;
};

class CheatLibrary
{
public:
    static std::wstring Root();
    static std::wstring LoaderConfig();
    static std::wstring DefaultTitle( const std::wstring& dllPath );
    static std::wstring Architecture( const std::wstring& dllPath );
    static bool IsUsableImage( const std::wstring& dllPath );

    void Scan();

    // Returns the entry index, -1 on error, or the existing index when the
    // same payload has already been imported.
    int Add( const std::wstring& dllPath, std::wstring* error = nullptr );
    bool Remove( int index );
    bool Rename( int index, const std::wstring& title );
    bool SetIcon( int index, const std::wstring& imagePath );

    const std::vector<CheatEntry>& entries() const { return _entries; }

    std::wstring selected() const { return _selected; }
    void setSelected( const std::wstring& id );

private:
    bool Write( const CheatEntry& entry );
    bool Read( const std::wstring& folder, CheatEntry& entry );
    std::wstring UniqueFolder( const std::wstring& base ) const;
    int IndexOfHash( const std::wstring& hash ) const;
    static bool DeleteFolder( const std::wstring& folder );

    std::vector<CheatEntry> _entries;
    std::wstring _selected;
};
