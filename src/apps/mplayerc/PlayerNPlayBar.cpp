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
        -MulDiv(11, dpiY, 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_smallFont.CreateFontW(
        -MulDiv(8, dpiY, 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_szMinVert = CSize(220, 360);
    m_szVert = CSize(248, 620);
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

    const int top = 94;
    const int row = 42;
    const int gap = 5;

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

    dc.SetBkMode(TRANSPARENT);
    switch (icon) {
        case 0: { // Home
            CPoint roof[3] = {{cx - 8, cy - 1}, {cx, cy - 8}, {cx + 8, cy - 1}};
            dc.Polyline(roof, 3);
            dc.MoveTo(cx - 6, cy - 2); dc.LineTo(cx - 6, cy + 7); dc.LineTo(cx + 6, cy + 7); dc.LineTo(cx + 6, cy - 2);
            dc.MoveTo(cx - 1, cy + 7); dc.LineTo(cx - 1, cy + 1); dc.LineTo(cx + 2, cy + 1); dc.LineTo(cx + 2, cy + 7);
            break;
        }
        case 1: // Playlist
            dc.MoveTo(cx - 8, cy - 6); dc.LineTo(cx + 8, cy - 6);
            dc.MoveTo(cx - 8, cy); dc.LineTo(cx + 8, cy);
            dc.MoveTo(cx - 8, cy + 6); dc.LineTo(cx + 4, cy + 6);
            break;
        case 2: // Video
            dc.RoundRect(cx - 8, cy - 7, cx + 8, cy + 7, 3, 3);
            dc.MoveTo(cx - 2, cy - 4); dc.LineTo(cx + 4, cy); dc.LineTo(cx - 2, cy + 4);
            break;
        case 3: // Audio
            dc.MoveTo(cx - 7, cy - 3); dc.LineTo(cx - 2, cy - 3); dc.LineTo(cx + 3, cy - 8); dc.LineTo(cx + 3, cy + 8); dc.LineTo(cx - 2, cy + 3); dc.LineTo(cx - 7, cy + 3); dc.LineTo(cx - 7, cy - 3);
            dc.Arc(cx - 2, cy - 7, cx + 11, cy + 7, cx + 5, cy + 5, cx + 5, cy - 5);
            break;
        default: // Favorites
            POINT heart[6] = {
                {cx, cy + 8}, {cx - 8, cy - 1}, {cx - 6, cy - 7},
                {cx, cy - 4}, {cx + 6, cy - 7}, {cx + 8, cy - 1}
            };
            dc.Polyline(heart, 6);
            dc.LineTo(cx, cy + 8);
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
    const COLORREF bg = dark ? RGB(6, 12, 22) : RGB(246, 248, 251);
    const COLORREF panel = dark ? RGB(11, 22, 35) : RGB(255, 255, 255);
    const COLORREF panelHot = dark ? RGB(16, 33, 49) : RGB(238, 244, 248);
    const COLORREF panelActive = dark ? RGB(16, 48, 65) : RGB(229, 241, 247);
    const COLORREF border = dark ? RGB(28, 52, 70) : RGB(222, 229, 236);
    const COLORREF text = dark ? RGB(232, 240, 248) : RGB(31, 43, 56);
    const COLORREF muted = dark ? RGB(132, 151, 171) : RGB(103, 116, 132);
    const COLORREF accent = RGB(48, 196, 238);

    dc.FillSolidRect(rc, bg);

    // Brand card.
    CRect brand(12, 12, rc.Width() - 12, 72);
    CBrush brandBrush(panel);
    dc.FillRect(brand, &brandBrush);
    dc.SelectObject(&brandBrush);
    dc.RoundRect(brand, CPoint(12, 12));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CPen brandPen(PS_SOLID, 1, border);
    CPen* oldPen = dc.SelectObject(&brandPen);
    CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    dc.RoundRect(brand, CPoint(12, 12));
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);

    const int logoSize = 34;
    CRect logo(brand.left + 12, brand.CenterPoint().y - logoSize / 2,
               brand.left + 12 + logoSize, brand.CenterPoint().y + logoSize / 2);
    CBrush logoBrush(accent);
    dc.SelectObject(&logoBrush);
    dc.RoundRect(logo, CPoint(9, 9));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CBrush logoCut(bg);
    CPoint tri[3] = {
        { logo.left + 13, logo.top + 9 },
        { logo.left + 13, logo.bottom - 9 },
        { logo.right - 9, logo.CenterPoint().y }
    };
    dc.SelectObject(&logoCut);
    dc.Polygon(tri, 3);

    CFont* oldFont = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(text);
    CRect title(brand.left + 56, brand.top + 11, brand.right - 10, brand.top + 35);
    dc.DrawTextW(L"N PLAY", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CFont* oldSmall = dc.SelectObject(&m_smallFont);
    dc.SetTextColor(accent);
    CRect subtitle(brand.left + 57, brand.top + 35, brand.right - 10, brand.bottom - 7);
    dc.DrawTextW(L"MEDIA PLAYER", subtitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.SelectObject(oldSmall);
    dc.SelectObject(oldFont);

    // Navigation.
    for (size_t i = 0; i < m_items.size(); ++i) {
        const bool active = static_cast<int>(i) == m_activeItem;
        const bool hot = static_cast<int>(i) == m_hotItem;
        CRect item = m_items[i].rect;

        if (active || hot) {
            CBrush itemBrush(active ? panelActive : panelHot);
            dc.SelectObject(&itemBrush);
            dc.RoundRect(item, CPoint(9, 9));
            dc.SelectObject(oldBrush);

            CPen itemPen(PS_SOLID, 1, active ? RGB(35, 91, 116) : border);
            CPen* savedPen = dc.SelectObject(&itemPen);
            dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
            dc.RoundRect(item, CPoint(9, 9));
            dc.SelectObject(savedPen);

            CBrush accentBrush(accent);
            CRect stripe(item.left, item.top + 9, item.left + 3, item.bottom - 9);
            dc.FillRect(stripe, &accentBrush);
        }

        DrawIcon(dc, CRect(item.left + 13, item.top + 8, item.left + 39, item.bottom - 8),
                 m_items[i].icon, active);

        CFont* old = dc.SelectObject(&m_font);
        dc.SetTextColor(active ? text : muted);
        CRect label(item.left + 51, item.top, item.right - 10, item.bottom);
        dc.DrawTextW(m_items[i].label, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(old);
    }

    // Playlist section.
    const int sectionY = rc.Height() - 132;
    CFont* old = dc.SelectObject(&m_smallFont);
    dc.SetTextColor(muted);
    CRect section(16, sectionY, rc.Width() - 16, sectionY + 22);
    dc.DrawTextW(L"PLAYLISTS", section, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CPen sectionPen(PS_SOLID, 1, border);
    dc.SelectObject(&sectionPen);
    dc.MoveTo(16, sectionY + 24);
    dc.LineTo(rc.Width() - 16, sectionY + 24);

    CRect listItem(12, sectionY + 34, rc.Width() - 12, sectionY + 74);
    CBrush listBrush(dark ? RGB(11, 22, 35) : RGB(255, 255, 255));
    dc.SelectObject(&listBrush);
    dc.RoundRect(listItem, CPoint(8, 8));
    dc.SelectObject(oldBrush);
    dc.SelectObject(&sectionPen);
    dc.RoundRect(listItem, CPoint(8, 8));

    dc.SetTextColor(text);
    CRect listLabel(listItem.left + 12, listItem.top, listItem.right - 12, listItem.bottom);
    dc.DrawTextW(L"Default Playlist", listLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CFont* oldMain = dc.SelectObject(&m_font);
    dc.SetTextColor(accent);
    CRect addItem(12, sectionY + 82, rc.Width() - 12, sectionY + 122);
    dc.DrawTextW(L"+  New Playlist", addItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.SelectObject(oldMain);
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
