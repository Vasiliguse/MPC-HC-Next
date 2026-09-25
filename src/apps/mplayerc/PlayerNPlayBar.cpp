#include "stdafx.h"
#include "MainFrm.h"
#include "PlayerNPlayBar.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CPlayerNPlayBar, CPlayerBar)

CPlayerNPlayBar::CPlayerNPlayBar(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
{
    m_items = {
        {L"Home",      0, {}},
        {L"Playlist",  1, {}},
        {L"Video",     2, {}},
        {L"Audio",     3, {}},
        {L"Favorites", 4, {}},
    };
}

CPlayerNPlayBar::~CPlayerNPlayBar()
{
}

BOOL CPlayerNPlayBar::Create(CWnd* pParentWnd, UINT defDockBarID)
{
    if (!CPlayerBar::Create(L"N Play", pParentWnd, ID_VIEW_NPLAY_NAVIGATION, defDockBarID, L"NPlayNavigationBar")) {
        return FALSE;
    }

    m_pMainFrame = static_cast<CMainFrame*>(pParentWnd);

    CClientDC fontDc(this);
    const int dpiY = fontDc.GetDeviceCaps(LOGPIXELSY);

    m_font.CreateFontW(
        -MulDiv(10, dpiY, 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_smallFont.CreateFontW(
        -MulDiv(8, dpiY, 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_szMinVert = CSize(188, 320);
    m_szVert = CSize(220, 520);
    m_szMinFloat = m_szMinVert;
    m_szFloat = m_szVert;

    LayoutItems();
    return TRUE;
}

void CPlayerNPlayBar::ReloadTranslatableResources()
{
    SetWindowTextW(L"N Play");
}

void CPlayerNPlayBar::LayoutItems()
{
    CRect rc;
    GetClientRect(&rc);

    const int top = 74;
    const int row = 40;
    const int gap = 6;

    for (size_t i = 0; i < m_items.size(); ++i) {
        const int y = top + static_cast<int>(i) * (row + gap);
        m_items[i].rect = CRect(12, y, rc.Width() - 12, y + row);
    }
}

void CPlayerNPlayBar::DrawIcon(CDC& dc, const CRect& r, int icon, bool active) const
{
    const COLORREF fg = active ? RGB(76, 201, 240) : RGB(145, 160, 176);
    CPen pen(PS_SOLID, 2, fg);
    CBrush brush(fg);
    CPen* oldPen = dc.SelectObject(&pen);
    CBrush* oldBrush = dc.SelectObject(&brush);

    const int cx = r.CenterPoint().x;
    const int cy = r.CenterPoint().y;

    switch (icon) {
        case 0:
            dc.MoveTo(cx, cy - 6); dc.LineTo(cx, cy + 5);
            dc.LineTo(cx - 5, cy); dc.LineTo(cx, cy - 6);
            dc.LineTo(cx + 5, cy);
            break;
        case 1:
            dc.Rectangle(cx - 7, cy - 7, cx + 7, cy - 5);
            dc.Rectangle(cx - 7, cy - 2, cx + 7, cy);
            dc.Rectangle(cx - 7, cy + 3, cx + 7, cy + 5);
            break;
        case 2:
            dc.Rectangle(cx - 7, cy - 6, cx + 7, cy + 6);
            dc.MoveTo(cx - 3, cy - 2); dc.LineTo(cx + 4, cy); dc.LineTo(cx - 3, cy + 3);
            break;
        case 3:
            dc.Ellipse(cx - 7, cy - 7, cx + 7, cy + 7);
            dc.MoveTo(cx - 2, cy - 2); dc.LineTo(cx + 4, cy - 5);
            dc.MoveTo(cx - 2, cy - 2); dc.LineTo(cx + 2, cy + 4);
            break;
        default:
            dc.MoveTo(cx - 7, cy - 1); dc.LineTo(cx - 2, cy - 6); dc.LineTo(cx + 3, cy - 1);
            dc.LineTo(cx + 7, cy - 5);
            dc.MoveTo(cx - 7, cy + 3); dc.LineTo(cx - 2, cy + 7); dc.LineTo(cx + 3, cy + 2);
            break;
    }

    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);
}

int CPlayerNPlayBar::HitTest(CPoint point) const
{
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].rect.PtInRect(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CPlayerNPlayBar::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    const bool dark = AfxGetAppSettings().bUseDarkTheme;
    const COLORREF bg = dark ? RGB(8, 16, 28) : RGB(247, 249, 252);
    const COLORREF panel = dark ? RGB(13, 25, 40) : RGB(255, 255, 255);
    const COLORREF text = dark ? RGB(224, 232, 242) : RGB(30, 42, 56);
    const COLORREF muted = dark ? RGB(126, 145, 166) : RGB(105, 117, 132);
    const COLORREF accent = RGB(53, 189, 235);

    dc.FillSolidRect(rc, bg);

    CRect brand(14, 14, rc.Width() - 14, 58);
    CBrush brandBrush(panel);
    dc.FillRect(brand, &brandBrush);
    CPen borderPen(PS_SOLID, 1, dark ? RGB(30, 54, 72) : RGB(221, 227, 235));
    CPen* oldPen = dc.SelectObject(&borderPen);
    CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    dc.RoundRect(brand, CPoint(10, 10));
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);

    CFont* oldFont = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(text);
    dc.DrawTextW(L"N PLAY", brand, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    dc.SelectObject(oldFont);

    for (size_t i = 0; i < m_items.size(); ++i) {
        const bool active = static_cast<int>(i) == m_activeItem;
        const bool hot = static_cast<int>(i) == m_hotItem;
        CRect item = m_items[i].rect;

        if (active || hot) {
            CBrush itemBrush(active ? RGB(18, 55, 73) : (dark ? RGB(16, 34, 50) : RGB(232, 241, 247)));
            dc.FillRect(item, &itemBrush);
            CBrush accentBrush(accent);
            CRect stripe(item.left, item.top + 7, item.left + 3, item.bottom - 7);
            dc.FillRect(stripe, &accentBrush);
        }

        DrawIcon(dc, CRect(item.left + 13, item.top + 8, item.left + 39, item.bottom - 8), m_items[i].icon, active);

        CFont* old = dc.SelectObject(&m_font);
        dc.SetTextColor(active ? text : muted);
        CRect label(item.left + 50, item.top, item.right - 8, item.bottom);
        dc.DrawTextW(m_items[i].label, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(old);
    }

    CFont* old = dc.SelectObject(&m_smallFont);
    dc.SetTextColor(muted);
    CRect section(16, rc.Height() - 118, rc.Width() - 16, rc.Height() - 92);
    dc.DrawTextW(L"PLAYLISTS", section, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CRect listItem(12, rc.Height() - 88, rc.Width() - 12, rc.Height() - 48);
    dc.SetTextColor(text);
    dc.DrawTextW(L"Default Playlist", CRect(listItem.left + 10, listItem.top, listItem.right - 8, listItem.bottom),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CRect addItem(12, rc.Height() - 44, rc.Width() - 12, rc.Height() - 10);
    dc.SetTextColor(accent);
    dc.DrawTextW(L"+  New Playlist", addItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.SelectObject(old);
}

void CPlayerNPlayBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);
    LayoutItems();
    Invalidate(FALSE);
}

void CPlayerNPlayBar::OnMouseMove(UINT nFlags, CPoint point)
{
    const int hit = HitTest(point);
    if (hit != m_hotItem) {
        m_hotItem = hit;
        Invalidate(FALSE);
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        TrackMouseEvent(&tme);
    }
    __super::OnMouseMove(nFlags, point);
}

void CPlayerNPlayBar::OnMouseLeave()
{
    m_hotItem = -1;
    Invalidate(FALSE);
    __super::OnMouseLeave();
}

void CPlayerNPlayBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    const int hit = HitTest(point);
    if (hit >= 0) {
        m_activeItem = hit;
        Invalidate(FALSE);
    }
    __super::OnLButtonDown(nFlags, point);
}

void CPlayerNPlayBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    const int hit = HitTest(point);
    if (hit >= 0 && m_pMainFrame) {
        switch (hit) {
            case 0:
                m_pMainFrame->SendMessageW(WM_COMMAND, ID_FILE_OPENFILE);
                break;
            case 1:
                m_pMainFrame->SendMessageW(WM_COMMAND, ID_VIEW_PLAYLIST);
                break;
            case 3:
                m_pMainFrame->SendMessageW(WM_COMMAND, ID_NAVIGATE_AUDIO);
                break;
            case 4:
                m_pMainFrame->SendMessageW(WM_COMMAND, ID_FAVORITES_ORGANIZE);
                break;
            default:
                break;
        }
    }
    __super::OnLButtonUp(nFlags, point);
}

BEGIN_MESSAGE_MAP(CPlayerNPlayBar, CPlayerBar)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()
