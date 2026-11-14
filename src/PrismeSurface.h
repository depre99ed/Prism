#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cmath>
#include <algorithm>
#include "TitleInput.h"
#include <objidl.h>
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"gdiplus.lib")

struct PrismeDll { std::wstring name, detail; bool enabled=true, missing=false; std::wstring artwork; };
struct PrismeProcess { std::wstring name; DWORD pid=0; };
struct PrismeState
{
    std::wstring target=L"cs2.exe", detail=L"Waiting for process", status=L"Add your first cheat.";
    std::vector<PrismeDll> dlls;
    std::vector<PrismeProcess> processes;
    std::vector<std::pair<std::wstring,std::wstring>> previewItems,errorItems;
    std::vector<std::wstring> errorLines;
    std::vector<std::pair<std::wstring,std::wstring>> versionItems,selectionItems;
    std::vector<bool> selectionChecked;
    std::wstring versionTitle;
    int versionChoice=0;
    bool editorVersion=false,exportDiagnostics=false,exportOption=true;
    std::wstring previewName,previewSummary,errorHeading;
    int panelFirst=0,errorIndex=-1;
    bool errorAvailable=false;
    int reportCopy=0;
    std::wstring query,primaryLabel=L"Load",editorTitle,editorArtwork,editorDllName;
    bool editorNew=true,editorSelectAll=false,canRevealZip=false;
    bool editorFocused=true,caretVisible=true;
    int editorCaret=-1;
    bool connected=false, autoCS2=true, busy=false, canInject=false, blocked=false;
    int overlay=0, first=0, firstProcess=0, selected=-1, hover=-1, pressed=-1, focus=-1, opacity=226;
    unsigned nudge=0;   // bumped when a disabled primary action is clicked: the status line flashes
};

// This is the real application renderer. It paints only our own scene to a
// premultiplied alpha DIB; no desktop capture or opaque native child controls.
// Hover, press and menu transitions are time based (Step); the host only has to
// keep calling Present while Animating() is true.
class PrismeSurface
{
public:
    enum Action { Shield=9, Add=10, Menu, Process, Auto, Inject, Close, Minimize,
        Settings, OpenProfile, SaveProfile, Modules, NewProcess, WaitProcess,
        Transparency, Back, Locate, Remove, Clear, SaveEntry=40, EditEntry, PickArtwork, ClearArtwork, EditTitle, RevealExport, PreviousPage, NextPage, ConfirmImport, History, CopyReport, PanelPrevious, PanelNext, Versions, AddVersion, RenameVersion, UseVersion, RemoveVersion, ExportReports, ConfirmExport, SelectAll, SelectNone, ExportOption, ErrorBase=100, ToggleBase=1000,
        SelectBase=2000, OptionsBase=3000, PickBase=4000, VersionBase=5000, ExportBase=7000 };
    static constexpr int Width=392, Height=444;
    // Shield swallows clicks on dead glass inside a menu; stop=false keeps a hit out of the Tab order.
    struct Hit { Gdiplus::RectF rect; int action; std::wstring label; bool stop; };
    std::vector<Hit> hits;
private:
    using G=Gdiplus::Graphics;
    using Col=Gdiplus::Color;
    using Rc=Gdiplus::RectF;
    ULONG_PTR _token=0;
    HDC _dc=nullptr; HBITMAP _dib=nullptr; HGDIOBJ _old=nullptr;
    void* _bits=nullptr;
    std::unique_ptr<Gdiplus::Bitmap> _bitmap, _back, _layer, _small;
    float _scale=1, _fade=1;
    int _backOpacity=-1;
    bool _rec=true, _animating=false;
    std::unique_ptr<Gdiplus::FontFamily> _family, _semibold;
    std::map<std::pair<int,bool>,std::unique_ptr<Gdiplus::Font>> _fonts;
    std::map<int,float> _hov, _prs;
    std::map<std::wstring,std::unique_ptr<Gdiplus::Bitmap>> _artwork;
    float _ovT=0, _flash=0;
    int _ovKind=1;
    unsigned _nudge=0;
    ULONGLONG _last=0;
    float _clock=0;
    float _editorScroll=0;
    std::unique_ptr<Gdiplus::Bitmap> _logo;

    static Col C(int a,int r,int g,int b) {return Col(BYTE((std::clamp)(a,0,255)),BYTE(r),BYTE(g),BYTE(b));}
    Col F(Col c) const {return Col(BYTE(c.GetA()*_fade),c.GetR(),c.GetG(),c.GetB());}
    static Col Mix(Col a,Col b,float t)
    {
        auto m=[&](int x,int y){return int(x+(y-x)*t);};
        return Col(BYTE(m(a.GetA(),b.GetA())),BYTE(m(a.GetR(),b.GetR())),BYTE(m(a.GetG(),b.GetG())),BYTE(m(a.GetB(),b.GetB())));
    }
    static float Ease(float t) {t=(std::clamp)(t,0.f,1.f);return 1-(1-t)*(1-t)*(1-t);}
    float H(int action) const {auto i=_hov.find(action);return i==_hov.end()?0.f:i->second;}
    float P(int action) const {auto i=_prs.find(action);return i==_prs.end()?0.f:i->second;}
    static void Round(Gdiplus::GraphicsPath& p,Rc r,float radius)
    {
        float d=radius*2;
        p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
        p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);p.CloseFigure();
    }
    static void Body(Gdiplus::GraphicsPath& p) {Round(p,Rc(10,10,372,Height-20),24);}
    void Fill(G& g,Gdiplus::GraphicsPath& p,Col c) {Gdiplus::SolidBrush b(F(c));g.FillPath(&b,&p);}
    void Card(G& g,Rc r,float radius,Col fill,Col border)
    {
        Gdiplus::GraphicsPath p;Round(p,r,radius);Fill(g,p,fill);
        Gdiplus::Pen pen(F(border),1);g.DrawPath(&pen,&p);
    }
    // A thin highlight that fades out at both ends, as light catching the top edge of a pane.
    void Sheen(G& g,float x0,float x1,float y,int a)
    {
        Gdiplus::LinearGradientBrush b(Gdiplus::PointF(x0,y),Gdiplus::PointF(x1,y),C(0,255,255,255),C(0,255,255,255));
        Col cols[3]={F(C(0,255,255,255)),F(C(a,255,255,255)),F(C(0,255,255,255))};float pos[3]={0,.5f,1};
        b.SetInterpolationColors(cols,pos,3);
        Gdiplus::Pen p(&b,1);g.DrawLine(&p,x0,y,x1,y);
    }
    // Translucent glass tile: vertical sheen, graded rim, faint top highlight. hover/press are 0..1 levels.
    void Glass(G& g,Rc r,float radius,int alpha,int rim,float hover=0,float press=0,Col tint=Col(255,222,232,255))
    {
        alpha+=6;if(rim>0)rim+=14;   // crisper separation from the body
        r.Inflate(-press,-press);
        Gdiplus::GraphicsPath p;Round(p,r,(std::max)(2.f,radius-press));
        Gdiplus::LinearGradientBrush fill(r,F(C(alpha+9+int(hover*30),tint.GetR(),tint.GetG(),tint.GetB())),
            F(C(alpha-3+int(hover*20),tint.GetR(),tint.GetG(),tint.GetB())),Gdiplus::LinearGradientModeVertical);
        g.FillPath(&fill,&p);
        if(press>0){Gdiplus::SolidBrush dim(F(C(int(46*press),6,10,22)));g.FillPath(&dim,&p);}
        Gdiplus::LinearGradientBrush edge(r,F(C(rim+int(hover*44),240,246,255)),F(C(rim/3+int(hover*20),210,225,255)),Gdiplus::LinearGradientModeVertical);
        Gdiplus::Pen pen(&edge,1);g.DrawPath(&pen,&p);
        if(r.Height>=24)Sheen(g,r.X+radius*.7f,r.GetRight()-radius*.7f,r.Y+1.1f,int(30+hover*34));
    }
    Gdiplus::Font& Face(float size,bool bold)
    {
        auto& f=_fonts[{int(size*10),bold}];
        if(!f)
        {
            bool semi=bold && _semibold && _semibold->IsAvailable();
            f.reset(new Gdiplus::Font(semi?_semibold.get():_family.get(),size,bold&&!semi?Gdiplus::FontStyleBold:Gdiplus::FontStyleRegular,Gdiplus::UnitPixel));
        }
        return *f;
    }
    void Text(G& g,const std::wstring& t,float x,float y,float w,float h,float size,
        Col color,bool bold=false,bool center=false)
    {
        Gdiplus::SolidBrush b(F(color));Gdiplus::StringFormat fmt;
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        if(center)fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(t.c_str(),-1,&Face(size,bold),Rc(x,y,w,h),&fmt,&b);
    }
    float Measure(const std::wstring& t,float size,bool bold)
    {
        G g(_small.get());Rc box;Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsMeasureTrailingSpaces|Gdiplus::StringFormatFlagsNoWrap);
        g.MeasureString(t.c_str(),-1,&Face(size,bold),Gdiplus::PointF(0,0),&fmt,&box);return box.Width;
    }
    void Line(G& g,float x,float y,float x2,float y2,Col c,float width=1.3f)
    {Gdiplus::Pen p(F(c),width);p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&p,x,y,x2,y2);}
    void AddHit(float x,float y,float w,float h,int action,const wchar_t* label,bool stop=true)
    {if(_rec)hits.push_back({Rc(x,y,w,h),action,label,stop});}
    void Plus(G& g,float x,float y,Col c)
    {Line(g,x-4,y,x+4,y,c);Line(g,x,y-4,x,y+4,c);}
    void Arrow(G& g,float x,float y,Col c)
    {Line(g,x-4,y-2,x,y+2,c);Line(g,x,y+2,x+4,y-2,c);}
    void Dots(G& g,float x,float y,Col c)
    {Gdiplus::SolidBrush b(F(c));for(int i=-1;i<=1;++i)g.FillEllipse(&b,x+i*6-1.6f,y-1.6f,3.2f,3.2f);}
    void LibraryAction(G& g,const Rc& rect,int action,const wchar_t* label,bool exporting)
    {
        Glass(g,rect,10,20,40,H(action),P(action));
        const float x=rect.X+rect.Width*.5f,y=rect.Y+rect.Height*.5f;
        const auto ink=Mix(C(232,200,211,228),C(249,244,247,255),.6f+H(action)*.4f);
        if(exporting)
        {
            Line(g,x,y+2.5f,x,y-5.5f,ink);
            Line(g,x-3,y-2.5f,x,y-5.5f,ink);Line(g,x,y-5.5f,x+3,y-2.5f,ink);
            Line(g,x-5.5f,y+1.5f,x-5.5f,y+5.5f,ink);
            Line(g,x-5.5f,y+5.5f,x+5.5f,y+5.5f,ink);
            Line(g,x+5.5f,y+5.5f,x+5.5f,y+1.5f,ink);
        }
        else {Line(g,x-5.5f,y,x+5.5f,y,ink);Line(g,x,y-5.5f,x,y+5.5f,ink);}
        AddHit(rect.X-4,rect.Y-4,rect.Width+8,rect.Height+8,action,label);
    }
    void Button(G& g,Rc r,const std::wstring& label,int action,Col text=Col(240,234,240,251),float pad=2)
    {
        Glass(g,r,11,22,38,H(action),P(action));
        Text(g,label,r.X+10,r.Y,r.Width-20,r.Height,12,text,false,true);
        AddHit(r.X-pad,r.Y-pad,r.Width+pad*2,r.Height+pad*2,action,label.c_str());
    }
    void CloseButton(G& g,Rc r,int action)
    {
        Glass(g,r,9,22,38,H(action),P(action));
        float cx=r.X+r.Width/2,cy=r.Y+r.Height/2;Col c=Mix(C(215,188,199,217),C(255,244,247,255),H(action));
        Line(g,cx-3.5f,cy-3.5f,cx+3.5f,cy+3.5f,c);Line(g,cx+3.5f,cy-3.5f,cx-3.5f,cy+3.5f,c);
        AddHit(r.X-4,r.Y-4,r.Width+8,r.Height+8,action,L"Close menu");
    }
    void Track(G& g,float x,float y,float h,int total,int visible,int first)
    {
        if(total<=visible)return;
        float th=(std::max)(18.f,h*visible/total),ty=y+(h-th)*first/(float)(total-visible);
        Card(g,Rc(x,y,3,h),1.5f,C(16,235,240,255),C(0,0,0,0));Card(g,Rc(x,ty,3,th),1.5f,C(90,235,240,255),C(0,0,0,0));
    }
    void Avatar(G& g,const Rc& rect,const std::wstring& path,const std::wstring& title)
    {
        Glass(g,rect,9,24,44);
        Gdiplus::Bitmap* image=nullptr;
        if(!path.empty())
        {
            auto& cached=_artwork[path];
            if(!cached){auto candidate=std::make_unique<Gdiplus::Bitmap>(path.c_str());if(candidate->GetLastStatus()==Gdiplus::Ok)cached=std::move(candidate);}
            image=cached.get();
        }
        if(image)
        {
            auto saved=g.Save();Gdiplus::GraphicsPath clip;Round(clip,rect,9);g.SetClip(&clip,Gdiplus::CombineModeIntersect);
            float side=static_cast<float>((std::min)(image->GetWidth(),image->GetHeight()));
            g.DrawImage(image,rect,(image->GetWidth()-side)/2,(image->GetHeight()-side)/2,side,side,Gdiplus::UnitPixel);g.Restore(saved);
        }
        else {std::wstring initial=title.empty()?L"P":title.substr(0,1);initial[0]=(wchar_t)towupper(initial[0]);Text(g,initial,rect.X,rect.Y,rect.Width,rect.Height,rect.Height*.42f,C(240,224,234,255),true,true);}
    }
    static Rc OverlayCard(int kind) {return kind==1?Rc(37,66,318,326):(kind>=5?Rc(37,66,318,340):(kind==4?Rc(37,86,318,315):(kind==2?Rc(27,86,338,316):Rc(44,166,304,230))));}

    // Static glass body: shadow, tint, reflections. Cached; only the opacity setting invalidates it.
    void Backdrop(int opacity)
    {
        using namespace Gdiplus;
        G g(_back.get());g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetCompositingMode(CompositingModeSourceCopy);g.Clear(Col(0,0,0,0));g.SetCompositingMode(CompositingModeSourceOver);
        g.ScaleTransform(_scale,_scale);_fade=1;
        // No drop shadow: on a light desktop it reads as a dark frame. The margin stays fully transparent.
        GraphicsPath body;Body(body);
        LinearGradientBrush tint(PointF(12,12),PointF(380,445),C(opacity,25,30,47),C(opacity+12,12,18,31));
        g.FillPath(&tint,&body);
        auto saved=g.Save();g.SetClip(&body);
        LinearGradientBrush reflection(Rc(10,10,372,201),C(35,232,239,255),C(0,232,239,255),LinearGradientModeVertical);
        g.FillRectangle(&reflection,10,10,372,200);
        GraphicsPath lens;lens.AddEllipse(Rc(-90,-120,360,230));PathGradientBrush lb(&lens);
        lb.SetCenterColor(C(40,236,242,255));Col edge[1]={C(0,236,242,255)};int count=1;lb.SetSurroundColors(edge,&count);lb.SetCenterPoint(PointF(70,42));
        g.FillPath(&lb,&lens);
        LinearGradientBrush glow(Rc(10,180,372,Height-190),C(0,121,147,255),C(16,121,147,255),LinearGradientModeVertical);
        g.FillRectangle(&glow,10,180,372,266);g.Restore(saved);
        _backOpacity=opacity;
    }
    // Rim light, drawn last so nothing paints over the contour: bright top-left and bottom-right, calm between.
    void Rim(G& g)
    {
        using namespace Gdiplus;
        g.ResetTransform();g.ScaleTransform(_scale,_scale);_fade=1;
        GraphicsPath body;Body(body);
        LinearGradientBrush edge(PointF(14,10),PointF(378,Height-10),C(210,246,250,255),C(150,230,238,255));
        Col cols[4]={C(215,246,250,255),C(52,200,214,250),C(44,190,205,245),C(150,230,238,255)};float pos[4]={0,.34f,.68f,1};
        edge.SetInterpolationColors(cols,pos,4);Pen outline(&edge,1.2f);g.DrawPath(&outline,&body);
        GraphicsPath inner;Round(inner,Rc(11.5f,11.5f,369,Height-23),22.5f);
        LinearGradientBrush glass(PointF(14,10),PointF(378,Height-10),C(60,255,255,255),C(14,255,255,255));Pen inside(&glass,1);g.DrawPath(&inside,&inner);
        Sheen(g,38,354,11.2f,140);
    }

    void Main(G& g,PrismeState& s)
    {
        using namespace Gdiplus;
        const auto white=C(249,244,247,255), muted=C(232,200,211,228), faint=C(214,178,190,212);
        g.ResetTransform();g.ScaleTransform(_scale,_scale);hits.clear();_rec=true;_fade=1;
        // Transparent brand mark, embedded in the executable.
        if(_logo){auto logoState=g.Save();g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.DrawImage(_logo.get(),Rc(18,22,36,36));g.Restore(logoState);}
        else Text(g,L"P",21,24,31,32,24,C(255,177,106,255),true,true);
        Text(g,L"Prism",55,25,140,30,20,white,true);
        Text(g,L"One step. That's all.",56,54,200,17,10.5f,faint);
        // Window controls remain inside the glass.
        float hm=H(Minimize),hc=H(Close);
        if(hm>0)Card(g,Rc(310,23,26,26),9,C(int(30*hm+16*P(Minimize)),255,255,255),C(0,0,0,0));
        if(hc>0)Card(g,Rc(340,23,26,26),9,C(int(58*hc+22*P(Close)),255,145,155),C(0,0,0,0));
        Line(g,319,36,327,36,Mix(muted,white,hm));
        Line(g,349,32,357,40,Mix(muted,white,hc));Line(g,357,32,349,40,Mix(muted,white,hc));
        AddHit(306,19,34,34,Minimize,L"Minimize");AddHit(340,19,34,34,Close,L"Close");
        Text(g,L"GAME",28,87,180,16,10,faint,true);
        float hmenu=H(Menu);
        if(hmenu>0)Card(g,Rc(332,81,34,24),10,C(int(28*hmenu+14*P(Menu)),255,255,255),C(0,0,0,0));
        Dots(g,349,93,Mix(muted,white,hmenu));AddHit(326,72,46,40,Menu,L"Options");
        Glass(g,Rc(27,113,338,54),16,18,40,H(Process)*.8f,P(Process));
        if(s.connected){SolidBrush halo(C(40,132,230,188));g.FillEllipse(&halo,38.f,127.f,16.f,16.f);}
        SolidBrush dot(s.connected?C(240,132,230,188):C(190,172,180,197));g.FillEllipse(&dot,43,132,6,6);
        Text(g,s.target,59,119,181,25,15,white,true);
        Text(g,s.detail,43,143,225,17,10.5f,muted);
        Glass(g,Rc(292,128,54,24),9,40,84,0,0,Col(255,236,240,252));
        Text(g,L"CS2",292,128,54,24,9,C(245,238,242,252),true,true);
        const std::wstring library=L"MY CHEATS";
        Text(g,library,28,182,190,20,10,faint,true);
        if(!s.dlls.empty())
        {
            std::wstring n=std::to_wstring(s.dlls.size());
            float x=28+Measure(library,10,true)+8,w=(std::max)(18.f,Measure(n,10,true)+12);
            Card(g,Rc(x,184,w,16),8,C(24,225,235,255),C(0,0,0,0));Text(g,n,x,184,w,16,10,muted,true,true);
        }
        LibraryAction(g,Rc(295,177,30,30),SaveProfile,L"Export catalogue as ZIP",true);
        LibraryAction(g,Rc(335,177,30,30),Add,L"Add a cheat",false);
        if(s.dlls.empty())
        {
            float he=H(Add),pe=P(Add);
            Glass(g,Rc(27,215,338,120),17,10,0,he,pe);
            GraphicsPath zone;Round(zone,Rc(27.5f,215.5f,337,119),16.5f);Pen dash(F(C(50+int(he*60),221,233,255)),1);dash.SetDashStyle(DashStyleDash);g.DrawPath(&dash,&zone);
            Glass(g,Rc(176,234,40,36),13,15,35,he);
            Plus(g,196,252,Mix(C(220,207,224,255),white,he));
            Text(g,L"Add a cheat",44,279,304,22,13,white,false,true);
            Text(g,L"or click to browse",44,303,304,17,10.5f,faint,false,true);
            AddHit(27,215,338,120,Add,L"Add a cheat");
        }
        else
        {
            s.first=(std::max)(0,(std::min)(s.first,(int)s.dlls.size()-1));
            for(int row=0;row<2 && row+s.first<(int)s.dlls.size();++row)
            {
                int i=row+s.first;auto& d=s.dlls[i];float y=215.f+row*61.f;
                float ht=H(ToggleBase+i),ho=H(OptionsBase+i),hs=(std::max)({H(SelectBase+i),ht,ho});
                bool selected=s.selected==i;
                Glass(g,Rc(27,y,338,54),14,selected?26:12,selected?76:34,hs*.8f,P(SelectBase+i)*.6f);
                Avatar(g,Rc(41,y+11,32,32),d.artwork,d.name);
                if(selected)
                {
                    Card(g,Rc(28.5f,y+13,2,28),1,C(220,218,231,255),C(0,0,0,0));
                }
                Text(g,d.name,85,y+5,224,24,12.5f,white,true);
                Text(g,d.detail,85,y+29,224,16,10,d.missing?C(240,252,192,139):muted);
                if(ho>0)Card(g,Rc(324,y+14,37,26),10,C(int(30*ho+14*P(OptionsBase+i)),255,255,255),C(0,0,0,0));
                Dots(g,342.5f,y+27,Mix(muted,white,ho));
                AddHit(27,y,287,54,SelectBase+i,L"Select this cheat");
                AddHit(314,y,51,54,OptionsBase+i,L"DLL options");
            }
            if(s.dlls.size()>2)
            {
                Track(g,370,215,115,(int)s.dlls.size(),2,s.first);
                Glass(g,Rc(28,336,32,24),10,20,40,H(PreviousPage),P(PreviousPage));
                Line(g,47,344,43,348,muted);Line(g,43,348,47,352,muted);
                AddHit(26,334,36,28,PreviousPage,L"Previous page");
                Text(g,std::to_wstring(s.first+1)+L"\u2013"+std::to_wstring((std::min)((int)s.dlls.size(),s.first+2))+L" / "+std::to_wstring(s.dlls.size()),66,336,258,24,10,faint,false,true);
                Glass(g,Rc(332,336,32,24),10,20,40,H(NextPage),P(NextPage));
                Line(g,345,344,349,348,muted);Line(g,349,348,345,352,muted);
                AddHit(330,334,36,28,NextPage,L"Next page");
            }
        }
        // Primary action is the only large button on the surface.
        {
            Rc ar(27,366,338,42);bool can=s.canInject;
            float hi=can?H(Inject):0,pi=can?P(Inject):0;
            GraphicsPath action;Round(action,Rc(ar.X+pi,ar.Y+pi,ar.Width-pi*2,ar.Height-pi*2),14-pi);
            // Violet-to-amethyst gradient follows the P mark, with a restrained glass sheen.
            LinearGradientBrush fill(PointF(ar.X-1,ar.Y),PointF(ar.GetRight()+1,ar.Y),
                can?Mix(C(255,139,67,227),C(255,149,77,237),hi):C(45,147,104,208),
                can?Mix(C(255,81,24,182),C(255,91,34,192),hi):C(29,90,54,150));
            Col accentStops[3]={can?Mix(C(255,139,67,227),C(255,149,77,237),hi):C(45,147,104,208),
                can?Mix(C(255,112,36,218),C(255,122,46,228),hi):C(37,118,74,185),
                can?Mix(C(255,81,24,182),C(255,91,34,192),hi):C(29,90,54,150)};
            float accentPositions[3]={0,.48f,1};fill.SetInterpolationColors(accentStops,accentPositions,3);fill.SetWrapMode(WrapModeTileFlipXY);
            g.FillPath(&fill,&action);
            auto saved=g.Save();g.SetClip(&action);
            LinearGradientBrush gloss(Rc(ar.X,ar.Y,ar.Width,ar.Height*.55f),C(can?14:8,216,185,255),C(0,255,255,255),LinearGradientModeVertical);
            g.FillRectangle(&gloss,ar.X,ar.Y,ar.Width,ar.Height*.55f);
            if(pi>0){SolidBrush dim(C(int(44*pi),10,16,40));g.FillPath(&dim,&action);}
            if(s.busy)
            {
                float x=ar.X-90+std::fmod(_clock/950.f,1.f)*(ar.Width+180);
                LinearGradientBrush band(PointF(x,0),PointF(x+90,0),C(0,255,255,255),C(0,255,255,255));
                Col cols[3]={C(0,255,255,255),C(60,235,242,255),C(0,255,255,255)};float pos[3]={0,.5f,1};band.SetInterpolationColors(cols,pos,3);
                g.FillRectangle(&band,x,ar.Y,90.f,ar.Height);
            }
            g.Restore(saved);
            Pen actionEdge(can?C(205,210,172,255):C(62,167,127,217),1);g.DrawPath(&actionEdge,&action);
            Text(g,s.primaryLabel,45,ar.Y,300,ar.Height,13.5f,can?C(255,250,246,255):C(205,205,185,228),true,true);
            AddHit(ar.X,ar.Y,ar.Width,ar.Height,Inject,L"Load");
        }
        float shake=_flash>0?std::sin(_flash*30.f)*3.f*_flash:0;
        Text(g,s.status,29+shake,412,334,18,10,s.blocked?Mix(muted,C(255,252,192,139),_flash):faint,false,true);
        if(s.errorAvailable)AddHit(27,410,338,28,History,L"View this error report");
    }

    void PanelPager(G& g,PrismeState& s,int count,int visible,float y,Col color)
    {
        if(count<=visible)return;
        Button(g,Rc(52,y,42,28),L"\u2039",PanelPrevious);
        Text(g,std::to_wstring(s.panelFirst+1)+L"\u2013"+std::to_wstring((std::min)(count,s.panelFirst+visible))+L" / "+std::to_wstring(count),100,y,190,28,10,color,false,true);
        Button(g,Rc(298,y,42,28),L"\u203a",PanelNext);
    }

    void Overlay(G& g,PrismeState& s,Gdiplus::GraphicsPath& body)
    {
        using namespace Gdiplus;
        const auto white=C(249,244,247,255), muted=C(232,200,211,228), faint=C(214,178,190,212);
        float e=Ease(_ovT);_fade=e;_rec=s.overlay!=0;if(_rec)hits.clear();
        Fill(g,body,C(150,6,10,22));
        Rc card=OverlayCard(_ovKind);if(_ovKind==4 && s.editorVersion)card=Rc(37,86,318,227);
        if(_ovKind==10)card=Rc(37,82,318,280);
        AddHit(card.X,card.Y,card.Width,card.Height,Shield,L"",false);
        float cx=card.X+card.Width/2,cy=card.Y+card.Height/2,k=.955f+.045f*e;
        auto saved=g.Save();g.TranslateTransform(cx,cy+(1-e)*10);g.ScaleTransform(k,k);g.TranslateTransform(-cx,-cy);
        for(int i=8;i>=1;--i){Rc q=card;q.Inflate(i*1.2f,i*1.2f);q.Offset(0,i*.9f);GraphicsPath p;Round(p,q,20+i*.8f);Fill(g,p,C(8,0,0,0));}
        GraphicsPath face;Round(face,card,20);
        LinearGradientBrush fill(card,F(C(246,38,46,68)),F(C(248,24,30,48)),LinearGradientModeVertical);g.FillPath(&fill,&face);
        LinearGradientBrush rim(card,F(C(130,235,242,255)),F(C(50,200,215,250)),LinearGradientModeVertical);Pen rimPen(&rim,1);g.DrawPath(&rimPen,&face);
        Sheen(g,card.X+18,card.GetRight()-18,card.Y+1.2f,80);
        if(_ovKind==1)
        {
            Text(g,L"Options",54,78,220,30,18,white,true);
            CloseButton(g,Rc(313,81,25,25),Back);
            const wchar_t* levels[]={L"High",L"Medium",L"Low"};
            std::wstring glass=L"Transparency:  "+std::wstring(levels[s.opacity<170?0:(s.opacity<215?1:2)]);
            std::wstring labels[]={L"Add a cheat",L"Import ZIP / catalogue",L"Export complete ZIP",L"Error history",glass,L"Show latest ZIP"};
            int ids[]={Add,OpenProfile,SaveProfile,History,Transparency,RevealExport};
            for(int i=0;i<(s.canRevealZip?6:5);++i)Button(g,Rc(52,116.f+i*40,288,33),labels[i],ids[i]);
            Text(g,L"Prism",52,370,288,12,9,faint,false,true);
        }
        else if(_ovKind==10)
        {
            Text(g,L"Antivirus notice",54,94,250,28,17,white,true);CloseButton(g,Rc(313,97,25,25),Back);
            Text(g,L"Shown once per Windows user",52,129,288,20,10,faint);
            const wchar_t* lines[]={
                L"Security software may flag DLL injection.",
                L"Review any alert before running Prism or",
                L"your DLLs; an alert can be legitimate.",
                L"",
                L"Exclusions are optional and reduce",
                L"protection. Only consider one for files",
                L"you have independently verified and trust.",
                L"",
                L"Prism does not change security settings."};
            for(int i=0;i<9;++i)Text(g,lines[i],52,153.f+i*16.5f,288,17,10.5f,muted);
            Button(g,Rc(52,317,288,28),L"Continue",Back,white);
        }
        else if(_ovKind==2)
        {
            Text(g,L"Processes",44,97,235,28,17,white,true);CloseButton(g,Rc(322,98,25,25),Back);
            Glass(g,Rc(43,135,306,31),10,16,40);
            Text(g,s.query.empty()?L"Type to search...":s.query,55,136,285,29,11,s.query.empty()?faint:white);
            if(!s.query.empty()){float x=55+(std::min)(Measure(s.query,11,false),270.f)+1;Line(g,x,143,x,159,C(200,203,224,255),1.2f);}
            std::vector<int> filtered;
            for(size_t i=0;i<s.processes.size();++i){auto name=s.processes[i].name,query=s.query;for(auto& c:name)c=(wchar_t)towlower(c);for(auto& c:query)c=(wchar_t)towlower(c);if(query.empty()||name.find(query)!=std::wstring::npos)filtered.push_back((int)i);}
            s.firstProcess=(std::max)(0,(std::min)(s.firstProcess,(std::max)(0,(int)filtered.size()-5)));
            for(int row=0;row<5 && row+s.firstProcess<(int)filtered.size();++row){int i=filtered[row+s.firstProcess];auto& p=s.processes[i];Button(g,Rc(43,177.f+row*39,296,34),p.name+L"   \u00b7   "+std::to_wstring(p.pid),PickBase+i,white,1);}
            Track(g,344,177,190,(int)filtered.size(),5,s.firstProcess);
            if(filtered.empty())Text(g,L"No processes found",45,240,300,40,12,muted,false,true);
        }
        else if(_ovKind==5)
        {
            Text(g,L"Import preview",54,78,250,28,17,white,true);CloseButton(g,Rc(313,81,25,25),Back);
            Text(g,s.previewName,52,113,288,20,10.5f,muted);
            Text(g,s.previewSummary,52,136,288,20,10.5f,faint);
            s.panelFirst=(std::clamp)(s.panelFirst,0,(std::max)(0,(int)s.previewItems.size()-3));
            for(int row=0;row<3 && row+s.panelFirst<(int)s.previewItems.size();++row)
            {
                const auto& item=s.previewItems[row+s.panelFirst];float y=166.f+row*48;
                Glass(g,Rc(52,y,280,42),10,12,25);
                Text(g,item.first,62,y+1,260,21,12,white,true);Text(g,item.second,62,y+22,260,17,9.5f,muted);
            }
            Track(g,338,166,138,(int)s.previewItems.size(),3,s.panelFirst);
            PanelPager(g,s,(int)s.previewItems.size(),3,312,muted);
            Text(g,L"Existing catalogue kept. Duplicates skipped.",52,344,288,19,9.5f,faint,false,true);
            Button(g,Rc(52,369,132,28),L"Cancel",Back);
            Button(g,Rc(194,369,146,28),L"Import",ConfirmImport,white);
        }
        else if(_ovKind==6)
        {
            Text(g,L"Error history",54,78,250,28,16,white,true);CloseButton(g,Rc(313,81,25,25),Back);
            Text(g,L"Last 100 reports \u00b7 stored on this PC",52,115,288,20,10,faint);
            s.panelFirst=(std::clamp)(s.panelFirst,0,(std::max)(0,(int)s.errorItems.size()-4));
            for(int row=0;row<4 && row+s.panelFirst<(int)s.errorItems.size();++row)
            {
                int i=row+s.panelFirst;const auto& item=s.errorItems[i];float y=145.f+row*46;
                Glass(g,Rc(52,y,280,40),10,12,25,H(ErrorBase+i),P(ErrorBase+i));
                Text(g,item.first,62,y,260,21,11.5f,white,true);Text(g,item.second,62,y+21,260,17,9.5f,muted);
                AddHit(52,y,280,40,ErrorBase+i,L"Open report");
            }
            if(s.errorItems.empty())Text(g,L"No errors recorded.",52,200,288,60,12,muted,false,true);
            Track(g,338,145,178,(int)s.errorItems.size(),4,s.panelFirst);
            PanelPager(g,s,(int)s.errorItems.size(),4,337,muted);
            Button(g,Rc(52,369,288,28),L"Export selected reports",ExportReports);
        }
        else if(_ovKind==7)
        {
            Text(g,L"Error details",54,78,250,28,17,white,true);CloseButton(g,Rc(313,81,25,25),History);
            Text(g,s.errorHeading,52,114,288,22,11,muted,true);
            Glass(g,Rc(49,144,294,177),10,10,22);
            s.panelFirst=(std::clamp)(s.panelFirst,0,(std::max)(0,(int)s.errorLines.size()-10));
            for(int row=0;row<10 && row+s.panelFirst<(int)s.errorLines.size();++row)
                Text(g,s.errorLines[row+s.panelFirst],58,149.f+row*16,272,16,9.5f,white);
            Track(g,338,149,160,(int)s.errorLines.size(),10,s.panelFirst);
            PanelPager(g,s,(int)s.errorLines.size(),10,330,muted);
            Button(g,Rc(52,369,132,28),L"Back",History);
            Button(g,Rc(194,369,146,28),s.reportCopy==1?L"Copied":(s.reportCopy==2?L"Try again":L"Copy report"),CopyReport,white);
        }
        else if(_ovKind==8)
        {
            Text(g,L"DLL versions",54,78,250,28,17,white,true);CloseButton(g,Rc(313,81,25,25),Back);
            Text(g,s.versionTitle,52,112,288,20,11,muted,true);
            Button(g,Rc(52,139,132,28),L"Add version",AddVersion);Button(g,Rc(194,139,146,28),L"Rename",RenameVersion);
            s.panelFirst=(std::clamp)(s.panelFirst,0,(std::max)(0,(int)s.versionItems.size()-3));
            for(int row=0;row<3 && row+s.panelFirst<(int)s.versionItems.size();++row)
            {
                int i=row+s.panelFirst;float y=181.f+row*45;const auto& item=s.versionItems[i];
                Glass(g,Rc(52,y,280,40),10,i==s.versionChoice?25:12,i==s.versionChoice?65:25,H(VersionBase+i),P(VersionBase+i));
                if(i==s.versionChoice)Card(g,Rc(54,y+9,2,22),1,C(220,218,231,255),C(0,0,0,0));
                Text(g,item.first,63,y,257,21,11.5f,white,true);Text(g,item.second,63,y+21,257,17,9.5f,muted);
                AddHit(52,y,280,40,VersionBase+i,L"Select version");
            }
            Track(g,338,181,130,(int)s.versionItems.size(),3,s.panelFirst);PanelPager(g,s,(int)s.versionItems.size(),3,320,muted);
            Button(g,Rc(52,369,132,28),L"Use version",UseVersion,white);Button(g,Rc(194,369,146,28),L"Remove",RemoveVersion,C(245,255,196,202));
        }
        else if(_ovKind==9)
        {
            Text(g,s.exportDiagnostics?L"Export diagnostics":L"Export catalogue",54,78,250,28,16,white,true);CloseButton(g,Rc(313,81,25,25),Back);
            Text(g,s.exportDiagnostics?L"Choose the reports to share.":L"Choose the DLL versions to share.",52,109,288,18,10,muted);
            Glass(g,Rc(52,132,288,28),9,14,25,H(ExportOption));
            Text(g,s.exportOption?L"\u2611":L"\u2610",60,132,22,28,14,white,false,true);
            Text(g,s.exportDiagnostics?L"Hide file paths":L"Include images",86,132,245,28,11,white);AddHit(52,132,288,28,ExportOption,L"Toggle export option");
            Button(g,Rc(52,170,132,28),L"Select all",SelectAll);Button(g,Rc(194,170,146,28),L"Clear selection",SelectNone);
            s.panelFirst=(std::clamp)(s.panelFirst,0,(std::max)(0,(int)s.selectionItems.size()-2));
            for(int row=0;row<2 && row+s.panelFirst<(int)s.selectionItems.size();++row)
            {
                int i=row+s.panelFirst;float y=212.f+row*49;const auto& item=s.selectionItems[i];bool selected=i<(int)s.selectionChecked.size() && s.selectionChecked[i];
                Glass(g,Rc(52,y,280,42),10,selected?22:10,selected?52:24,H(ExportBase+i),P(ExportBase+i));
                Text(g,selected?L"\u2611":L"\u2610",58,y,25,42,15,white,false,true);
                Text(g,item.first,87,y+1,235,21,11,white,true);Text(g,item.second,87,y+23,235,17,9.5f,muted);
                AddHit(52,y,280,42,ExportBase+i,L"Toggle item");
            }
            Track(g,338,212,91,(int)s.selectionItems.size(),2,s.panelFirst);PanelPager(g,s,(int)s.selectionItems.size(),2,315,muted);
            int selected=(int)std::count(s.selectionChecked.begin(),s.selectionChecked.end(),true);
            Text(g,std::to_wstring(selected)+L" of "+std::to_wstring(s.selectionItems.size())+L" selected",52,346,288,18,10,faint,false,true);
            Button(g,Rc(52,369,132,28),L"Cancel",Back);Button(g,Rc(194,369,146,28),L"Export ZIP",ConfirmExport,white);
        }
        else if(_ovKind==4)
        {
            Text(g,s.editorVersion?(s.editorNew?L"Add DLL version":L"Rename version"):(s.editorNew?L"Add a cheat":L"Edit cheat"),54,98,240,30,17,white,true);
            CloseButton(g,Rc(313,101,25,25),Back);
            Text(g,s.editorVersion?L"Version label":L"Title",52,141,288,21,11,muted);
            Glass(g,Rc(52,166,288,36),10,25,65);
            if(s.editorSelectAll)Card(g,Rc(60,172,272,24),5,C(55,172,199,255),C(0,0,0,0));
            int caret=TitleInput::Position(s.editorTitle,s.editorCaret);
            float before=Measure(s.editorTitle.substr(0,caret),12,false);
            _editorScroll=(std::clamp)(_editorScroll,0.f,(std::max)(0.f,Measure(s.editorTitle,12,false)-258));
            if(before-_editorScroll>258)_editorScroll=before-258;
            if(before<_editorScroll)_editorScroll=before;
            _editorScroll=(std::max)(0.f,_editorScroll);
            auto editClip=g.Save();g.SetClip(Rc(62,168,268,32));
            Gdiplus::StringFormat editFormat(Gdiplus::StringFormat::GenericTypographic());
            editFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap|Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
            editFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::SolidBrush editInk(F(s.editorTitle.empty()?faint:white));
            const auto& editText=s.editorTitle.empty()?std::wstring(L"Cheat name"):s.editorTitle;
            g.DrawString(editText.c_str(),-1,&Face(12,false),Rc(62-_editorScroll,166,10000,36),&editFormat,&editInk);
            if(s.editorFocused && s.caretVisible)Line(g,62+before-_editorScroll,175,62+before-_editorScroll,194,white,1.4f);
            g.Restore(editClip);
            AddHit(52,166,288,36,EditTitle,L"Edit title");
            if(s.editorVersion)
            {
                Text(g,s.editorDllName,52,213,288,22,10,faint);
                Button(g,Rc(52,257,288,36),L"Save version",SaveEntry,white);
            }
            else
            {
            Text(g,L"Optional image",52,207,288,21,11,muted);
            Avatar(g,Rc(52,235,64,64),s.editorArtwork,s.editorTitle);
            Button(g,Rc(126,235,214,30),s.editorArtwork.empty()?L"Add image":L"Change image",PickArtwork);
            Button(g,Rc(126,273,214,30),L"Remove image",ClearArtwork);
            Text(g,s.editorDllName,52,310,288,20,10,faint);
            Button(g,Rc(52,345,288,36),L"Save",SaveEntry,white);
            }
        }
        else
        {
            std::wstring name=s.selected>=0&&s.selected<(int)s.dlls.size()?s.dlls[s.selected].name:L"DLL";
            Text(g,name,61,179,220,25,14,white,true);CloseButton(g,Rc(307,180,24,24),Back);
            Button(g,Rc(60,218,272,34),L"Edit title / image",EditEntry);
            Button(g,Rc(60,261,272,34),L"Manage DLL versions",Versions);
            Button(g,Rc(60,304,272,33),L"Remove this cheat",Remove,C(245,255,196,202));
            Button(g,Rc(60,347,272,33),L"Clear catalogue",Clear,C(245,255,196,202));
        }
        g.Restore(saved);_fade=1;
    }

    void Step(const PrismeState& s,float ms)
    {
        bool busy=false;
        auto approach=[&](float& v,float target,float tau){v+=(target-v)*(1-std::exp(-ms/tau));if(std::fabs(target-v)<.01f)v=target;if(v!=target)busy=true;};
        if(s.hover>=0)_hov[s.hover];
        if(s.pressed>=0)_prs[s.pressed];
        for(auto i=_hov.begin();i!=_hov.end();)
        {approach(i->second,i->first==s.hover?1.f:0.f,55);if(i->second==0 && i->first!=s.hover)i=_hov.erase(i);else ++i;}
        for(auto i=_prs.begin();i!=_prs.end();)
        {bool down=i->first==s.pressed && s.hover==s.pressed;approach(i->second,down?1.f:0.f,34);if(i->second==0 && !down)i=_prs.erase(i);else ++i;}
        if(s.overlay)_ovKind=s.overlay;
        float t=s.overlay?1.f:0.f;
        _ovT=t?(std::min)(1.f,_ovT+ms/190.f):(std::max)(0.f,_ovT-ms/140.f);
        if(_ovT!=t)busy=true;
        if(s.nudge!=_nudge){_nudge=s.nudge;_flash=1;}
        if(_flash>0){_flash=(std::max)(0.f,_flash-ms/700.f);busy=true;}
        _clock+=(std::min)(ms,50.f);
        _animating=busy||s.busy;
    }

    void Scene(G& g,PrismeState& s)
    {
        using namespace Gdiplus;
        const int w=(int)(Width*_scale),h=(int)(Height*_scale);
        if(_backOpacity!=s.opacity)Backdrop(s.opacity);
        g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ResetTransform();
        g.SetCompositingMode(CompositingModeSourceCopy);g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
        g.DrawImage(_back.get(),Rect(0,0,w,h),0,0,w,h,UnitPixel);
        g.SetCompositingMode(CompositingModeSourceOver);g.SetPixelOffsetMode(PixelOffsetModeDefault);g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
        if(_ovT<=.001f)Main(g,s);
        else
        {
            // Frosted glass: the page is rendered on its own layer, then cross-faded with a blurred copy under the menu.
            G lg(_layer.get());lg.SetSmoothingMode(SmoothingModeAntiAlias);lg.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            lg.SetCompositingMode(CompositingModeSourceCopy);lg.Clear(Col(0,0,0,0));lg.SetCompositingMode(CompositingModeSourceOver);
            Main(lg,s);
            const int sw=(int)_small->GetWidth(),sh=(int)_small->GetHeight();
            {
                G sg(_small.get());sg.SetCompositingMode(CompositingModeSourceCopy);sg.SetInterpolationMode(InterpolationModeHighQualityBilinear);
                ImageAttributes wrap;wrap.SetWrapMode(WrapModeTileFlipXY);
                sg.DrawImage(_layer.get(),Rect(0,0,sw,sh),0,0,w,h,UnitPixel,&wrap);
            }
            float e=Ease(_ovT);
            auto draw=[&](Image* image,int sx,int sy,float alpha){
                ColorMatrix m={1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,alpha,0, 0,0,0,0,1};
                ImageAttributes a;a.SetColorMatrix(&m);a.SetWrapMode(WrapModeClamp);
                g.DrawImage(image,Rect(0,0,w,h),0,0,sx,sy,UnitPixel,&a);
            };
            if(e<1)draw(_layer.get(),w,h,1-e);
            draw(_small.get(),sw,sh,e);
            g.ResetTransform();g.ScaleTransform(_scale,_scale);
            GraphicsPath body;Body(body);Overlay(g,s,body);
        }
        g.ResetTransform();g.ScaleTransform(_scale,_scale);
        if(s.focus>=0 && s.focus<(int)hits.size())
        {auto r=hits[s.focus].rect;r.Inflate(2,2);GraphicsPath p;Round(p,r,9);Pen pen(C(245,240,244,255),1.5f);g.DrawPath(&pen,&p);}
        Rim(g);
    }
public:
    PrismeSurface()
    {
        Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&_token,&input,nullptr);
        _family.reset(new Gdiplus::FontFamily(L"Segoe UI"));_semibold.reset(new Gdiplus::FontFamily(L"Segoe UI Semibold"));
    }
    ~PrismeSurface(){_logo.reset();_bitmap.reset();_back.reset();_layer.reset();_small.reset();_artwork.clear();_fonts.clear();_family.reset();_semibold.reset();if(_dc){SelectObject(_dc,_old);DeleteObject(_dib);DeleteDC(_dc);}if(_token)Gdiplus::GdiplusShutdown(_token);}
    bool HasBrandMark() const {return _logo && _logo->GetWidth()>0;}
    int TitleCaretAt(float deviceX,const PrismeState& state)
    {
        float wanted=deviceX/_scale-62+_editorScroll;int best=0;float distance=std::abs(wanted);
        for(int at=0;at<(int)state.editorTitle.size();)
        {
            at=TitleInput::Next(state.editorTitle,at);float delta=std::abs(Measure(state.editorTitle.substr(0,at),12,false)-wanted);
            if(delta<distance){distance=delta;best=at;}
        }
        return best;
    }
    bool Init(float scale=1)
    {
        _scale=scale;
        auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(170),L"PNG");
        if(resource)
        {
            DWORD length=SizeofResource(nullptr,resource);auto data=LockResource(LoadResource(nullptr,resource));
            HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,length);
            if(memory)
            {
                auto bytes=GlobalLock(memory);
                if(bytes){memcpy(bytes,data,length);GlobalUnlock(memory);IStream* stream=nullptr;
                    if(SUCCEEDED(CreateStreamOnHGlobal(memory,TRUE,&stream)))
                    {{Gdiplus::Bitmap decoded(stream,FALSE);if(decoded.GetLastStatus()==Gdiplus::Ok)_logo.reset(decoded.Clone(0,0,decoded.GetWidth(),decoded.GetHeight(),PixelFormat32bppPARGB));}stream->Release();}
                    else GlobalFree(memory);
                }else GlobalFree(memory);
            }
        }
        int w=(int)(Width*scale),h=(int)(Height*scale);
        BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        _dc=CreateCompatibleDC(nullptr);_dib=CreateDIBSection(_dc,&bi,DIB_RGB_COLORS,&_bits,nullptr,0);
        if(!_dc||!_dib)return false;
        _old=SelectObject(_dc,_dib);_bitmap.reset(new Gdiplus::Bitmap(w,h,w*4,PixelFormat32bppPARGB,(BYTE*)_bits));
        _back.reset(new Gdiplus::Bitmap(w,h,PixelFormat32bppPARGB));_layer.reset(new Gdiplus::Bitmap(w,h,PixelFormat32bppPARGB));
        _small.reset(new Gdiplus::Bitmap((std::max)(8,w/5),(std::max)(8,h/5),PixelFormat32bppPARGB));
        _backOpacity=-1;
        return _bitmap->GetLastStatus()==Gdiplus::Ok && _back->GetLastStatus()==Gdiplus::Ok && _layer->GetLastStatus()==Gdiplus::Ok && _small->GetLastStatus()==Gdiplus::Ok;
    }
    // Advances the transitions by real elapsed time (ms<0) or an explicit step, without drawing.
    void Advance(const PrismeState& state,float ms=-1)
    {
        if(ms<0){ULONGLONG now=GetTickCount64();ms=(std::min)(_last?(float)(now-_last):16.f,48.f);_last=now;}
        Step(state,ms);
    }
    bool Animating()const{return _animating;}
    void Draw(PrismeState& state){Gdiplus::Graphics g(_bitmap.get());Scene(g,state);}
    bool Present(HWND window,PrismeState& state)
    {
        if(!_bitmap)return false;Advance(state);Draw(state);SIZE size={(int)(Width*_scale),(int)(Height*_scale)};POINT source={0,0};BLENDFUNCTION alpha={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        return UpdateLayeredWindow(window,nullptr,nullptr,&size,_dc,&source,0,&alpha,ULW_ALPHA)!=FALSE;
    }
    int HitTest(float x,float y)const
    {for(auto i=hits.rbegin();i!=hits.rend();++i)if(i->rect.Contains(x/_scale,y/_scale))return i->action;return -1;}
    // Indices into hits, in Tab order.
    std::vector<int> FocusOrder()const
    {std::vector<int> order;for(size_t i=0;i<hits.size();++i)if(hits[i].stop)order.push_back((int)i);return order;}
    bool Export(const wchar_t* path,PrismeState& state,bool fixture=false,bool settle=true)
    {
        if(settle)Advance(state,100000.f);
        Draw(state);UINT size=0,count=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> bytes(size);auto codecs=(Gdiplus::ImageCodecInfo*)bytes.data();Gdiplus::GetImageEncoders(count,size,codecs);CLSID png={};
        for(UINT i=0;i<count;++i)if(wcscmp(codecs[i].MimeType,L"image/png")==0)png=codecs[i].Clsid;
        if(!fixture)return _bitmap->Save(path,&png)==Gdiplus::Ok;
        Gdiplus::Bitmap result(_bitmap->GetWidth(),_bitmap->GetHeight(),PixelFormat32bppPARGB);Gdiplus::Graphics g(&result);
        g.ScaleTransform(_scale,_scale);
        Gdiplus::LinearGradientBrush bg(Gdiplus::Point(0,0),Gdiplus::Point(Width,Height),Gdiplus::Color(255,35,61,81),Gdiplus::Color(255,99,67,100));g.FillRectangle(&bg,0,0,Width,Height);
        Gdiplus::SolidBrush b(Gdiplus::Color(255,76,115,126));g.FillEllipse(&b,-85,80,320,330);Gdiplus::SolidBrush l(Gdiplus::Color(255,177,133,147));g.FillEllipse(&l,240,-100,260,360);
        g.ResetTransform();g.DrawImage(_bitmap.get(),0,0);return result.Save(path,&png)==Gdiplus::Ok;
    }
};
