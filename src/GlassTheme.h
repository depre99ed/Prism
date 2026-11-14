#pragma once
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include "resource.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

// Presentation only. No dependency on Blackbone or the injection core.
class GlassTheme
{
    HBRUSH _background = CreateSolidBrush( RGB(18, 21, 29) );
    HBRUSH _panel = CreateSolidBrush( RGB(28, 32, 43) );
    HFONT _font = nullptr, _title = nullptr;
    bool _glass = false;
    static constexpr COLORREF Text = RGB(232, 236, 245);
    static constexpr COLORREF Muted = RGB(151, 164, 185);

    static LRESULT CALLBACK StatusProc( HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR )
    {
        if (msg == WM_PAINT)
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint( window, &ps );
            RECT rc; GetClientRect( window, &rc );
            HBRUSH brush = CreateSolidBrush( RGB(18, 21, 29) );
            FillRect( dc, &rc, brush ); DeleteObject( brush );
            SetBkMode( dc, TRANSPARENT ); SetTextColor( dc, Muted );
            SelectObject( dc, reinterpret_cast<HFONT>(SendMessageW( window, WM_GETFONT, 0, 0 )) );
            for (int i = 0; i < 3; ++i)
            {
                wchar_t value[512] = {};
                SendMessageW( window, SB_GETTEXTW, i, reinterpret_cast<LPARAM>(value) );
                RECT part; SendMessageW( window, SB_GETRECT, i, reinterpret_cast<LPARAM>(&part) );
                part.left += 12;
                DrawTextW( dc, value, -1, &part, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS );
            }
            EndPaint( window, &ps ); return 0;
        }
        if (msg == WM_NCDESTROY) RemoveWindowSubclass( window, StatusProc, 1 );
        LRESULT result = DefSubclassProc( window, msg, wp, lp );
        if (msg == SB_SETTEXTW) InvalidateRect( window, nullptr, FALSE );
        return result;
    }

public:
    ~GlassTheme()
    {
        DeleteObject( _background ); DeleteObject( _panel );
        if (_font) DeleteObject( _font );
        if (_title) DeleteObject( _title );
    }
    void Apply( HWND window )
    {
        BOOL dark = TRUE;
        DwmSetWindowAttribute( window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark) );
        DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
        DwmSetWindowAttribute( window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners) );
        DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_TRANSIENTWINDOW;
        _glass = SUCCEEDED(DwmSetWindowAttribute( window, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop) ));
        if (_glass) { MARGINS margins = {-1}; DwmExtendFrameIntoClientArea( window, &margins ); }
        HDC dc = GetDC( window ); int dpi = GetDeviceCaps( dc, LOGPIXELSY ); ReleaseDC( window, dc );
        if (!_font) _font = CreateFontW( -MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI" );
        if (!_title) _title = CreateFontW( -MulDiv(21, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI" );
        SendMessageW( window, WM_SETFONT, reinterpret_cast<WPARAM>(_font), TRUE );
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            SendMessageW( child, WM_SETFONT, reinterpret_cast<WPARAM>(GetDlgCtrlID(child) == IDC_BRAND ? _title : _font), TRUE );
            wchar_t cls[80] = {}; GetClassNameW( child, cls, 80 );
            if (_wcsicmp(cls, L"Button") == 0)
            {
                LONG_PTR style = GetWindowLongPtrW( child, GWL_STYLE );
                if ((style & BS_TYPEMASK) == BS_PUSHBUTTON || (style & BS_TYPEMASK) == BS_DEFPUSHBUTTON)
                    SetWindowLongPtrW( child, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW );
                SetWindowTheme( child, L"", L"" );
            }
            else SetWindowTheme( child, L"DarkMode_Explorer", nullptr );
            if (_wcsicmp(cls, WC_LISTVIEWW) == 0)
            {
                ListView_SetBkColor( child, RGB(28,32,43) );
                ListView_SetTextBkColor( child, RGB(28,32,43) );
                ListView_SetTextColor( child, Text );
                if (!ListView_GetImageList(child,LVSIL_SMALL))
                {
                    // Owned and destroyed by the list-view control (no LVS_SHAREIMAGELISTS).
                    HIMAGELIST rows = ImageList_Create(1, MulDiv(30,dpi,96), ILC_COLOR32, 1, 1);
                    ListView_SetImageList( child, rows, LVSIL_SMALL );
                }
                SetWindowTheme( ListView_GetHeader(child), L"DarkMode_ItemsView", nullptr );
            }
            if (_wcsicmp(cls, STATUSCLASSNAMEW) == 0) SetWindowSubclass(child, StatusProc, 1, 0);
        }
        InvalidateRect( window, nullptr, TRUE );
    }
    INT_PTR Handle( HWND window, UINT msg, WPARAM wp, LPARAM lp )
    {
        if (msg == WM_DWMCOMPOSITIONCHANGED) { Apply(window); return TRUE; }
        if (msg == WM_ERASEBKGND)
        {
            RECT rc; GetClientRect(window,&rc);
            FillRect(reinterpret_cast<HDC>(wp),&rc,_glass ? static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)) : _background);
            return TRUE;
        }
        if (msg == WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(_glass ? GetStockObject(BLACK_BRUSH) : _background);
        if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX)
        {
            HDC dc = reinterpret_cast<HDC>(wp);
            const bool panel = msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX;
            SetTextColor(dc, GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == IDC_BRAND ? Text : Muted);
            SetBkColor(dc, panel ? RGB(28,32,43) : (_glass ? RGB(0,0,0) : RGB(18,21,29)));
            SetBkMode(dc,TRANSPARENT);
            return reinterpret_cast<INT_PTR>(panel ? _panel : (_glass ? GetStockObject(BLACK_BRUSH) : _background));
        }
        if (msg == WM_DRAWITEM)
        {
            auto item = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
            if (!item || item->CtlType != ODT_BUTTON) return FALSE;
            bool disabled = (item->itemState & ODS_DISABLED) != 0;
            bool primary = item->CtlID == IDC_EXECUTE;
            COLORREF fill = primary && !disabled ? RGB(160, 188, 255) : RGB(36, 42, 56);
            if (item->itemState & ODS_SELECTED) fill = primary ? RGB(130,160,225) : RGB(48,57,76);
            HBRUSH brush = CreateSolidBrush(fill);
            HPEN pen = CreatePen(PS_SOLID,1,primary && !disabled ? fill : RGB(61,70,90));
            auto oldBrush=SelectObject(item->hDC,brush), oldPen=SelectObject(item->hDC,pen);
            RoundRect(item->hDC,item->rcItem.left,item->rcItem.top,item->rcItem.right,item->rcItem.bottom,10,10);
            SelectObject(item->hDC,oldBrush); SelectObject(item->hDC,oldPen); DeleteObject(brush); DeleteObject(pen);
            SetBkMode(item->hDC,TRANSPARENT);
            SetTextColor(item->hDC,disabled ? RGB(111,123,143) : (primary ? RGB(17,26,45) : Text));
            SelectObject(item->hDC,_font);
            wchar_t text[160] = {}; GetWindowTextW(item->hwndItem,text,160);
            DrawTextW(item->hDC,text,-1,&item->rcItem,DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (item->itemState & ODS_FOCUS) { RECT r=item->rcItem; InflateRect(&r,-4,-4); DrawFocusRect(item->hDC,&r); }
            return TRUE;
        }
        return FALSE;
    }
};
