#pragma once
#include <string>
#include <algorithm>
namespace TitleInput
{
    inline bool Low(wchar_t c){return c>=0xDC00 && c<=0xDFFF;}
    inline bool High(wchar_t c){return c>=0xD800 && c<=0xDBFF;}
    inline int Position(const std::wstring& text,int caret)
    {
        int at=caret<0?(int)text.size():(std::clamp)(caret,0,(int)text.size());
        if(at>0 && at<(int)text.size() && Low(text[at]) && High(text[at-1]))--at;return at;
    }
    inline int Previous(const std::wstring& text,int caret)
    {int at=Position(text,caret);if(at>0){--at;if(at>0 && Low(text[at]) && High(text[at-1]))--at;}return at;}
    inline int Next(const std::wstring& text,int caret)
    {int at=Position(text,caret);if(at<(int)text.size()){if(High(text[at]) && at+1<(int)text.size() && Low(text[at+1]))++at;++at;}return at;}
    inline void Insert(std::wstring& text,int& caret,bool& all,const std::wstring& incoming)
    {
        caret=Position(text,caret);if(all){text.clear();caret=0;all=false;}
        std::wstring clean;for(wchar_t c:incoming)if(c>=32 && c!=127)clean+=c;
        size_t available=text.size()<80?80-text.size():0;
        if(clean.size()>available){clean.resize(available);if(!clean.empty() && High(clean.back()))clean.pop_back();}
        text.insert(caret,clean);caret+=(int)clean.size();
    }
    inline void Erase(std::wstring& text,int& caret,bool& all,bool backwards)
    {
        caret=Position(text,caret);if(all){text.clear();caret=0;all=false;return;}
        if(backwards){int before=Previous(text,caret);text.erase(before,caret-before);caret=before;}
        else text.erase(caret,Next(text,caret)-caret);
    }
}
