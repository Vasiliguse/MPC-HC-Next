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
    ScaleForDpi();
    return TRUE;
}

void CPlayerNPlayBar::ScaleForDpi()
{
    if (!m_pMainFrame) {
        return;
    }

    const int dpiY = m_pMainFrame->GetDPIY();
    m_font.DeleteObject();
    m_smallFont.DeleteObject();

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

    m_szMinVert = CSize(m_pMainFrame->ScaleX(220), m_pMainFrame->ScaleY(360));
    m_szVert = CSize(m_pMainFrame->ScaleX(248), m_pMainFrame->ScaleY(620));
    m_szMinFloat = m_szMinVert;
    m_szFloat = m_szVert;

    m_hotItem = -1;
    m_pressedItem = -1;
    LayoutItems();
    Invalidate(FALSE);
}

void CPlayerNPlayBar::ReloadTranslatableResources()
{
    SetWindowTextW(L"N Play");
}

void CPlayerNPlayBar::SetActiveItem(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size()) || m_activeItem == index) {
        return;
    }

    m_activeItem = index;
    Invalidate(FALSE);
}

void CPlayerNPlayBar::LayoutItems()
{
    CRect rc;
    GetClientRect(&rc);

    CClientDC dc(this);
    const int dpiY = dc.GetDeviceCaps(LOGPIXELSY);
    const int top = MulDiv(94, dpiY, 96);
    const int row = MulDiv(42, dpiY, 96);
    const int gap = MulDiv(5, dpiY, 96);
    const int margin = MulDiv(12, dpiY, 96);

    for (size_t i = 0; i < m_items.size(); ++i) {
        const int y = top + static_cast<int>(i) * (row + gap);
        m_items[i].rect = CRect(margin, y, rc.Width() - margin, y + row);
    }
}

void CPlayerNPlayBar::DrawIcon(CDC& dc, const CRect& r, int icon, bool active) const
{
    const COLORREF fg = active ? RGB(76, 201, 240) : RGB(145, 160, 176);
    const int dpiX = dc.GetDeviceCaps(LOGPIXELSX);
    const auto scale = [dpiX](int value) { return MulDiv(value, dpiX, 96); };
    CPen pen(PS_SOLID, scale(2) < 1 ? 1 : scale(2), fg);
    CBrush brush(fg);
    CPen* oldPen = dc.SelectObject(&pen);
    CBrush* oldBrush = dc.SelectObject(&brush);

    const int cx = r.CenterPoint().x;
    const int cy = r.CenterPoint().y;
    const int x8 = scale(8);
    const int x7 = scale(7);
    const int x6 = scale(6);
    const int x4 = scale(4);
    const int x3 = scale(3);
    const int x2 = scale(2);
    const int x1 = scale(1);
    const int x5 = scale(5);

    dc.SetBkMode(TRANSPARENT);
    switch (icon) {
        case 0: { // Home
            CPoint roof[3] = {{cx - x8, cy - x1}, {cx, cy - x8}, {cx + x8, cy - x1}};
            dc.Polyline(roof, 3);
            dc.MoveTo(cx - x6, cy - x2); dc.LineTo(cx - x6, cy + x7); dc.LineTo(cx + x6, cy + x7); dc.LineTo(cx + x6, cy - x2);
            dc.MoveTo(cx - x1, cy + x7); dc.LineTo(cx - x1, cy + x1); dc.LineTo(cx + x2, cy + x1); dc.LineTo(cx + x2, cy + x7);
            break;
        }
        case 1: // Playlist
            dc.MoveTo(cx - x8, cy - x6); dc.LineTo(cx + x8, cy - x6);
            dc.MoveTo(cx - x8, cy); dc.LineTo(cx + x8, cy);
            dc.MoveTo(cx - x8, cy + x6); dc.LineTo(cx + x4, cy + x6);
            break;
        case 2: // Video
            dc.RoundRect(cx - x8, cy - x7, cx + x8, cy + x7, scale(3), scale(3));
            dc.MoveTo(cx - x2, cy - x4); dc.LineTo(cx + x4, cy); dc.LineTo(cx - x2, cy + x4);
            break;
        case 3: // Audio
            dc.MoveTo(cx - x7, cy - x3); dc.LineTo(cx - x2, cy - x3); dc.LineTo(cx + x3, cy - x8); dc.LineTo(cx + x3, cy + x8); dc.LineTo(cx - x2, cy + x3); dc.LineTo(cx - x7, cy + x3); dc.LineTo(cx - x7, cy - x3);
            dc.Arc(cx - x2, cy - x7, cx + scale(11), cy + x7, cx + x5, cy + x5, cx + x5, cy - x5);
            break;
        default: // Favorites
            POINT heart[6] = {
                {cx, cy + x8}, {cx - x8, cy - x1}, {cx - x6, cy - x7},
                {cx, cy - x4}, {cx + x6, cy - x7}, {cx + x8, cy - x1}
            };
            dc.Polyline(heart, 6);
            dc.LineTo(cx, cy + x8);
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
    CClientDC scaleDc(this);
    const int dpiY = scaleDc.GetDeviceCaps(LOGPIXELSY);
    const auto scale = [dpiY](int value) { return MulDiv(value, dpiY, 96); };

    CRect brand(scale(12), scale(12), rc.Width() - scale(12), scale(72));
    CBrush brandBrush(panel);
    dc.FillRect(brand, &brandBrush);
    dc.SelectObject(&brandBrush);
    dc.RoundRect(brand, CPoint(scale(12), scale(12)));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CPen brandPen(PS_SOLID, 1, border);
    CPen* oldPen = dc.SelectObject(&brandPen);
    CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    dc.RoundRect(brand, CPoint(scale(12), scale(12)));
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);

    const int logoSize = scale(34);
    CRect logo(brand.left + scale(12), brand.CenterPoint().y - logoSize / 2,
               brand.left + scale(12) + logoSize, brand.CenterPoint().y + logoSize / 2);
    CBrush logoBrush(accent);
    dc.SelectObject(&logoBrush);
    dc.RoundRect(logo, CPoint(scale(9), scale(9)));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CBrush logoCut(bg);
    CPoint tri[3] = {
        { logo.left + scale(13), logo.top + scale(9) },
        { logo.left + scale(13), logo.bottom - scale(9) },
        { logo.right - scale(9), logo.CenterPoint().y }
    };
    dc.SelectObject(&logoCut);
    dc.Polygon(tri, 3);

    CFont* oldFont = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(text);
    CRect title(brand.left + scale(56), brand.top + scale(11), brand.right - scale(10), brand.top + scale(35));
    dc.DrawTextW(L"N PLAY", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CFont* oldSmall = dc.SelectObject(&m_smallFont);
    dc.SetTextColor(accent);
    CRect subtitle(brand.left + scale(57), brand.top + scale(35), brand.right - scale(10), brand.bottom - scale(7));
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
            dc.RoundRect(item, CPoint(scale(9), scale(9)));
            dc.SelectObject(oldBrush);

            CPen itemPen(PS_SOLID, 1, active ? RGB(35, 91, 116) : border);
            CPen* savedPen = dc.SelectObject(&itemPen);
            dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
            dc.RoundRect(item, CPoint(scale(9), scale(9)));
            dc.SelectObject(savedPen);

            CBrush accentBrush(accent);
            CRect stripe(item.left, item.top + scale(9), item.left + scale(3), item.bottom - scale(9));
            dc.FillRect(stripe, &accentBrush);
        }

        DrawIcon(dc, CRect(item.left + scale(13), item.top + scale(8),
                           item.left + scale(39), item.bottom - scale(8)),
                 m_items[i].icon, active);

        CFont* old = dc.SelectObject(&m_font);
        dc.SetTextColor(active ? text : muted);
        CRect label(item.left + scale(51), item.top, item.right - scale(10), item.bottom);
        dc.DrawTextW(m_items[i].label, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(old);
    }

    // Playlist section. Hide it when the dock is too short so it never overlaps navigation.
    if (rc.Height() >= scale(468)) {
        const int sectionY = rc.Height() - scale(132);
        CFont* old = dc.SelectObject(&m_smallFont);
        dc.SetTextColor(muted);
        CRect section(scale(16), sectionY, rc.Width() - scale(16), sectionY + scale(22));
        dc.DrawTextW(L"PLAYLISTS", section, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CPen sectionPen(PS_SOLID, 1, border);
        dc.SelectObject(&sectionPen);
        dc.MoveTo(scale(16), sectionY + scale(24));
        dc.LineTo(rc.Width() - scale(16), sectionY + scale(24));

        CRect listItem(scale(12), sectionY + scale(34), rc.Width() - scale(12), sectionY + scale(74));
        CBrush listBrush(dark ? RGB(11, 22, 35) : RGB(255, 255, 255));
        dc.SelectObject(&listBrush);
        dc.RoundRect(listItem, CPoint(scale(8), scale(8)));
        dc.SelectObject(oldBrush);
        dc.SelectObject(&sectionPen);
        dc.RoundRect(listItem, CPoint(scale(8), scale(8)));

        dc.SetTextColor(text);
        CRect listLabel(listItem.left + scale(12), listItem.top, listItem.right - scale(12), listItem.bottom);
        dc.DrawTextW(L"Default Playlist", listLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CFont* oldMain = dc.SelectObject(&m_font);
        dc.SetTextColor(accent);
        CRect addItem(scale(12), sectionY + scale(82), rc.Width() - scale(12), sectionY + scale(122));
        dc.DrawTextW(L"+  New Playlist", addItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(oldMain);
        dc.SelectObject(old);
    }
}

void CPlayerNPlayBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);
    m_hotItem = -1;
    m_pressedItem = -1;
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
    m_pressedItem = HitTest(point);
    if (m_pressedItem >= 0) {
        SetActiveItem(m_pressedItem);
    }
    __super::OnLButtonDown(nFlags, point);
}

void CPlayerNPlayBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    const int hit = HitTest(point);
    const int pressed = m_pressedItem;
    m_pressedItem = -1;

    if (hit >= 0 && hit == pressed && m_pMainFrame) {
        switch (hit) {
            case 0:
                m_pMainFrame->SendMessageW(WM_COMMAND, ID_FILE_OPENFILE);
                break;
            case 1:
                m_pMainFrame->SendMessageW(WM_COMMAND, ID_VIEW_PLAYLIST);
                break;
            case 2:
                m_pMainFrame->OnMenuNavVideo();
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
