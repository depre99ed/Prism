#include "ProfileMgr.h"
#include "PrismePaths.h"
#include <BlackBone/src/BlackBone/Misc/Utils.h>

#define CURRENT_PROFILE L"\\PrismeProfile.xpr"

bool ProfileMgr::Save( const std::wstring& path /*= L""*/)
{
    try
    {
        auto filepath = path.empty() ? (PrismeDataDirectory() + CURRENT_PROFILE) : path;

        acut::XmlDoc<wchar_t> xml;
        xml.create_document();

        std::vector<std::wstring> enabledValues;
        enabledValues.reserve(_config.images.size());
        for (size_t i = 0; i < _config.images.size(); ++i)
            enabledValues.emplace_back(i >= _config.imageEnabled.size() || _config.imageEnabled[i] ? L"1" : L"0");
        std::vector<std::wstring> titleValues,artworkValues;
        titleValues.reserve(_config.images.size());artworkValues.reserve(_config.images.size());
        for(size_t i=0;i<_config.images.size();++i)
        {
            titleValues.push_back(i<_config.imageTitles.size()?_config.imageTitles[i]:L"");
            artworkValues.push_back(i<_config.imageArtwork.size()?_config.imageArtwork[i]:L"");
        }
        for (size_t i = 0; i < _config.images.size(); ++i)
        {
            xml.append( L"XenosConfig.imagePath" ).value( _config.images[i] );
            xml.append( L"XenosConfig.imageEnabled" ).value( enabledValues[i] );
            xml.append(L"XenosConfig.cheatTitle").value(titleValues[i]);
            xml.append(L"XenosConfig.cheatArtwork").value(artworkValues[i]);
        }
        xml.set(L"XenosConfig.selectedCheat",_config.selectedCheat);
        for(size_t i=0;i<_config.versionGroups.size() && i<_config.images.size();++i)
        {
            const auto& source=_config.versionGroups[i];auto group=xml.append(L"XenosConfig.versionGroup");
            group.set(L"index",(int)i);group.set(L"id",source.id.c_str());group.set(L"active",source.active);
            for(const auto& item:source.items)
            {
                auto version=group.append(L"version");version.set(L"label",item.label.c_str());version.set(L"path",item.path.c_str());
                version.set(L"id",item.id.c_str());version.set(L"sha256",item.sha256.c_str());
            }
        }
        xml.set( L"XenosConfig.autoTargetCs2", _config.autoTargetCs2 );

        xml.set( L"XenosConfig.manualMapFlags", _config.mmapFlags );
        xml.set( L"XenosConfig.procName",       _config.procName.c_str() );
        xml.set( L"XenosConfig.hijack",         _config.hijack );
        xml.set( L"XenosConfig.unlink",         _config.unlink );
        xml.set( L"XenosConfig.erasePE",        _config.erasePE );
        xml.set( L"XenosConfig.close",          _config.close );
        xml.set( L"XenosConfig.krnHandle",      _config.krnHandle );
        xml.set( L"XenosConfig.injIndef",       _config.injIndef );
        xml.set( L"XenosConfig.processMode",    _config.processMode );
        xml.set( L"XenosConfig.injectMode",     _config.injectMode );
        xml.set( L"XenosConfig.delay",          _config.delay );
        xml.set( L"XenosConfig.period",         _config.period );
        xml.set( L"XenosConfig.skip",           _config.skipProc );
        xml.set( L"XenosConfig.procCmdLine",    _config.procCmdLine.c_str() );
        xml.set( L"XenosConfig.initRoutine",    _config.initRoutine.c_str() );
        xml.set( L"XenosConfig.initArgs",       _config.initArgs.c_str() );

        const auto temporary=filepath+L".tmp";
        xml.write_document(temporary);
        if(GetFileAttributesW(filepath.c_str())!=INVALID_FILE_ATTRIBUTES)
            CopyFileW(filepath.c_str(),(filepath+L".bak").c_str(),TRUE);
        if(!MoveFileExW(temporary.c_str(),filepath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
        
        return true;
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

bool ProfileMgr::Load( const std::wstring& path /*= L""*/ )
{
    try
    {
        auto filepath = path.empty() ? (PrismeDataDirectory() + CURRENT_PROFILE) : path;
        if (!acut::file_exists( filepath ))
            return false;

        acut::XmlDoc<wchar_t> xml;
        xml.read_from_file( filepath );

        ConfigData loaded;
        // Load images in a safe way
        if(xml.has( L"XenosConfig.imagePath" ))
        {
            auto nodes = xml.all_nodes_named( L"XenosConfig.imagePath" );
            for (auto& node : nodes)
                loaded.images.emplace_back( node.value() );
        }

        if (xml.has( L"XenosConfig.imageEnabled" ))
            for (auto& node : xml.all_nodes_named( L"XenosConfig.imageEnabled" ))
                loaded.imageEnabled.push_back( node.value() != L"0" );
        if(xml.has(L"XenosConfig.cheatTitle"))
            for(auto& node:xml.all_nodes_named(L"XenosConfig.cheatTitle"))loaded.imageTitles.push_back(node.value());
        if(xml.has(L"XenosConfig.cheatArtwork"))
            for(auto& node:xml.all_nodes_named(L"XenosConfig.cheatArtwork"))loaded.imageArtwork.push_back(node.value());
        xml.get_if_present(L"XenosConfig.selectedCheat",loaded.selectedCheat);
        if(xml.has(L"XenosConfig.versionGroup"))
        {
            loaded.versionGroups.resize(loaded.images.size());std::vector<bool> seen(loaded.images.size());size_t total=0;
            for(auto group:xml.all_nodes_named(L"XenosConfig.versionGroup"))
            {
                int index=-1;group.get_if_present(L"index",index);
                if(index<0 || index>=(int)loaded.images.size() || seen[index])return false;seen[index]=true;
                auto& target=loaded.versionGroups[index];group.get_if_present(L"id",target.id);group.get_if_present(L"active",target.active);
                if(group.has(L"version"))for(auto version:group.all_nodes_named(L"version"))
                {
                    DllVersion record;version.get_if_present(L"label",record.label);version.get_if_present(L"path",record.path);
                    version.get_if_present(L"id",record.id);version.get_if_present(L"sha256",record.sha256);
                    if(record.path.empty() || record.label.size()>80 || ++total>1000 || target.items.size()>=100)return false;
                    target.items.push_back(std::move(record));
                }
                if(!target.items.empty())
                {
                    if(target.active<0 || target.active>=(int)target.items.size())return false;
                    loaded.images[index]=target.items[target.active].path;
                }
            }
        }
        xml.get_if_present( L"XenosConfig.autoTargetCs2", loaded.autoTargetCs2 );

        xml.get_if_present( L"XenosConfig.manualMapFlags",  loaded.mmapFlags );
        xml.get_if_present( L"XenosConfig.procName",        loaded.procName );
        xml.get_if_present( L"XenosConfig.hijack",          loaded.hijack );
        xml.get_if_present( L"XenosConfig.unlink",          loaded.unlink );
        xml.get_if_present( L"XenosConfig.erasePE",         loaded.erasePE );
        xml.get_if_present( L"XenosConfig.close",           loaded.close );
        xml.get_if_present( L"XenosConfig.krnHandle",       loaded.krnHandle );
        xml.get_if_present( L"XenosConfig.injIndef",        loaded.injIndef );
        xml.get_if_present( L"XenosConfig.processMode",     loaded.processMode );
        xml.get_if_present( L"XenosConfig.injectMode",      loaded.injectMode );
        xml.get_if_present( L"XenosConfig.delay",           loaded.delay );
        xml.get_if_present( L"XenosConfig.period",          loaded.period );
        xml.get_if_present( L"XenosConfig.skip",            loaded.skipProc );
        xml.get_if_present( L"XenosConfig.procCmdLine",     loaded.procCmdLine );
        xml.get_if_present( L"XenosConfig.initRoutine",     loaded.initRoutine );
        xml.get_if_present( L"XenosConfig.initArgs",        loaded.initArgs );

        _config=std::move(loaded);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
