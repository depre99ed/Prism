#pragma once
#include "../src/TitleInput.h"
static void TestTitleInput()
{
    std::wstring title=L"Aurora";int caret=3;bool all=false;
    TitleInput::Insert(title,caret,all,L" X");require(title==L"Aur Xora" && caret==5,"typing inserts at visible caret");
    TitleInput::Erase(title,caret,all,true);require(title==L"Aur ora" && caret==4,"backspace before caret");
    TitleInput::Erase(title,caret,all,false);require(title==L"Aur ra" && caret==4,"delete after caret");
    all=true;TitleInput::Insert(title,caret,all,L"New\nTitle\t");require(title==L"NewTitle" && caret==8 && !all,"paste replaces selection and filters line breaks");
    title=L"A\xD83D\xDE00";caret=3;
    TitleInput::Erase(title,caret,all,true);require(title==L"A" && caret==1,"backspace removes an entire UTF-16 pair");
    title=std::wstring(79,L'x');caret=79;
    TitleInput::Insert(title,caret,all,L"\xD83D\xDE00");require(title.size()==79,"paste limit never splits a UTF-16 pair");
    title=L"\xD83D\xDE00";require(TitleInput::Position(title,1)==0 && TitleInput::Next(title,0)==2,"cursor does not split a UTF-16 pair");
}
