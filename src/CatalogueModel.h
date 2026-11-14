#pragma once
#include "ProfileMgr.h"
#include <algorithm>
#include <stdexcept>
namespace CatalogueModel
{
    inline void Normalize(ProfileMgr::ConfigData& cfg)
    {
        cfg.versionGroups.resize(cfg.images.size());
        for(size_t i=0;i<cfg.images.size();++i)
        {
            auto& group=cfg.versionGroups[i];
            if(group.items.empty())group.items.push_back({L"Initial version",cfg.images[i],L"",L""});
            group.active=(std::clamp)(group.active,0,(int)group.items.size()-1);
            cfg.images[i]=group.items[group.active].path;
        }
    }
    inline ProfileMgr::ConfigData Select(const ProfileMgr::ConfigData& source,
        const std::vector<std::pair<int,int>>& rows,const std::vector<bool>& selected,bool artwork)
    {
        auto normalized=source;Normalize(normalized);ProfileMgr::ConfigData result;
        for(size_t i=0;i<normalized.images.size();++i)
        {
            ProfileMgr::VersionGroup group;group.id=normalized.versionGroups[i].id;int chosen=-1;
            for(size_t v=0;v<normalized.versionGroups[i].items.size();++v)
            {
                bool keep=false;
                for(size_t row=0;row<rows.size() && row<selected.size();++row)
                    if(selected[row] && rows[row]==std::make_pair((int)i,(int)v)){keep=true;break;}
                if(!keep)continue;
                if((int)v==normalized.versionGroups[i].active)chosen=(int)group.items.size();
                group.items.push_back(normalized.versionGroups[i].items[v]);
            }
            if(group.items.empty())continue;
            group.active=chosen<0?0:chosen;
            if((int)i==source.selectedCheat)result.selectedCheat=(int)result.images.size();
            result.images.push_back(group.items[group.active].path);result.versionGroups.push_back(std::move(group));
            result.imageTitles.push_back(i<source.imageTitles.size()?source.imageTitles[i]:L"");
            result.imageArtwork.push_back(artwork && i<source.imageArtwork.size()?source.imageArtwork[i]:L"");
            result.imageEnabled.push_back(false);
        }
        if(result.selectedCheat<0 && !result.images.empty())result.selectedCheat=0;
        if(result.selectedCheat>=0)result.imageEnabled[result.selectedCheat]=true;
        return result;
    }
}
