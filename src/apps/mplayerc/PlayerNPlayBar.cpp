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

    const int dpiX = fontDc.GetDeviceCaps(LOGPIXELSX);
    const int dpiY = fontDc.GetDeviceCaps(LOGPIXELSY);
    const auto sx = [dpiX](int value) { return MulDiv(value, dpiX, 96); };
    const auto sy = [dpiY](int value) { return MulDiv(value, dpiY, 96); };

    m_szMinVert = CSize(sx(220), sy(360));
    m_szVert = CSize(sx(248), sy(620));
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

    CClientDC dc(this);
    const int dpiX = dc.GetDeviceCaps(LOGPIXELSX);
    const int dpiY = dc.GetDeviceCaps(LOGPIXELSY);
    const auto sx = [dpiX](int value) { return MulDiv(value, dpiX, 96); };
    const auto sy = [dpiY](int value) { return MulDiv(value, dpiY, 96); };

    const int top = sy(94);
    const int row = sy(42);
    const int gap = sy(5);

    for (size_t i = 0; i < m_items.size(); ++i) {
        const int y = top + static_cast<int>(i) * (row + gap);
        m_items[i].rect = CRect(sx(12), y, rc.Width() - sx(12), y + row);
    }
}

void CPlayerNPlayBar::DrawIcon(CDC& dc, const CRect& r, int icon, bool active) const
{
    const COLORREF fg = active ? RGB(76, 201, 240) : RGB(145, 160, 176);
    const auto v = [&r](int value) { return MulDiv(value, r.Height(), 26); };
    CPen pen(PS_SOLID, std::max(1, v(2)), fg);
    CBrush brush(fg);
    CPen* oldPen = dc.SelectObject(&pen);
    CBrush* oldBrush = dc.SelectObject(&brush);

    const int cx = r.CenterPoint().x;
    const int cy = r.CenterPoint().y;

    dc.SetBkMode(TRANSPARENT);
    switch (icon) {
        case 0: { // Home
            CPoint roof[3] = {{cx - v(8), cy - v(1)}, {cx, cy - v(8)}, {cx + v(8), cy - v(1)}};
            dc.Polyline(roof, 3);
            dc.MoveTo(cx - v(6), cy - v(2)); dc.LineTo(cx - v(6), cy + v(7)); dc.LineTo(cx + v(6), cy + v(7)); dc.LineTo(cx + v(6), cy - v(2));
            dc.MoveTo(cx - v(1), cy + v(7)); dc.LineTo(cx - v(1), cy + v(1)); dc.LineTo(cx + v(2), cy + v(1)); dc.LineTo(cx + v(2), cy + v(7));
            break;
        }
        case 1: // Playlist
            dc.MoveTo(cx - v(8), cy - v(6)); dc.LineTo(cx + v(8), cy - v(6));
            dc.MoveTo(cx - v(8), cy); dc.LineTo(cx + v(8), cy);
            dc.MoveTo(cx - v(8), cy + v(6)); dc.LineTo(cx + v(4), cy + v(6));
            break;
        case 2: // Video
            dc.RoundRect(cx - v(8), cy - v(7), cx + v(8), cy + v(7), v(3), v(3));
            dc.MoveTo(cx - v(2), cy - v(4)); dc.LineTo(cx + v(4), cy); dc.LineTo(cx - v(2), cy + v(4));
            break;
        case 3: // Audio
            dc.MoveTo(cx - v(7), cy - v(3)); dc.LineTo(cx - v(2), cy - v(3)); dc.LineTo(cx + v(3), cy - v(8)); dc.LineTo(cx + v(3), cy + v(8)); dc.LineTo(cx - v(2), cy + v(3)); dc.LineTo(cx - v(7), cy + v(3)); dc.LineTo(cx - v(7), cy - v(3));
            dc.Arc(cx - v(2), cy - v(7), cx + v(11), cy + v(7), cx + v(5), cy + v(5), cx + v(5), cy - v(5));
            break;
        default: // Favorites
            POINT heart[6] = {
                {cx, cy + v(8)}, {cx - v(8), cy - v(1)}, {cx - v(6), cy - v(7)},
                {cx, cy - v(4)}, {cx + v(6), cy - v(7)}, {cx + v(8), cy - v(1)}
            };
            dc.Polyline(heart, 6);
            dc.LineTo(cx, cy + v(8));
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
    CClientDC dpiDc(this);
    const int dpiX = dpiDc.GetDeviceCaps(LOGPIXELSX);
    const int dpiY = dpiDc.GetDeviceCaps(LOGPIXELSY);
    const auto sx = [dpiX](int value) { return MulDiv(value, dpiX, 96); };
    const auto sy = [dpiY](int value) { return MulDiv(value, dpiY, 96); };

    CRect brand(sx(12), sy(12), rc.Width() - sx(12), sy(72));
    CBrush brandBrush(panel);
    dc.FillRect(brand, &brandBrush);
    dc.SelectObject(&brandBrush);
    dc.RoundRect(brand, CPoint(sx(12), sy(12)));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CPen brandPen(PS_SOLID, 1, border);
    CPen* oldPen = dc.SelectObject(&brandPen);
    CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    dc.RoundRect(brand, CPoint(sx(12), sy(12)));
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);

    const int logoSize = sy(34);
    CRect logo(brand.left + sx(12), brand.CenterPoint().y - logoSize / 2,
               brand.left + sx(12) + logoSize, brand.CenterPoint().y + logoSize / 2);
    CBrush logoBrush(accent);
    dc.SelectObject(&logoBrush);
    dc.RoundRect(logo, CPoint(sx(9), sy(9)));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CBrush logoCut(bg);
    CPoint tri[3] = {
        { logo.left + sx(13), logo.top + sy(9) },
        { logo.left + sx(13), logo.bottom - sy(9) },
        { logo.right - sx(9), logo.CenterPoint().y }
    };
    dc.SelectObject(&logoCut);
    dc.Polygon(tri, 3);

    CFont* oldFont = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(text);
    CRect title(brand.left + sx(56), brand.top + sy(11), brand.right - sx(10), brand.top + sy(35));
    dc.DrawTextW(L"N PLAY", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CFont* oldSmall = dc.SelectObject(&m_smallFont);
    dc.SetTextColor(accent);
    CRect subtitle(brand.left + sx(57), brand.top + sy(35), brand.right - sx(10), brand.bottom - sy(7));
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
            dc.RoundRect(item, CPoint(sx(9), sy(9)));
            dc.SelectObject(oldBrush);

            CPen itemPen(PS_SOLID, 1, active ? RGB(35, 91, 116) : border);
            CPen* savedPen = dc.SelectObject(&itemPen);
            dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
            dc.RoundRect(item, CPoint(sx(9), sy(9)));
            dc.SelectObject(savedPen);

            CBrush accentBrush(accent);
            CRect stripe(item.left, item.top + sy(9), item.left + sx(3), item.bottom - sy(9));
            dc.FillRect(stripe, &accentBrush);
        }

        DrawIcon(dc, CRect(item.left + sx(13), item.top + sy(8), item.left + sx(39), item.bottom - sy(8)),
                 m_items[i].icon, active);

        CFont* old = dc.SelectObject(&m_font);
        dc.SetTextColor(active ? text : muted);
        CRect label(item.left + sx(51), item.top, item.right - sx(10), item.bottom);
        dc.DrawTextW(m_items[i].label, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(old);
    }

    // Playlist section. Hide it when the dock is too short so it never overlaps navigation.
    if (rc.Height() >= sy(430)) {
        const int sectionY = rc.Height() - sy(132);
        CFont* old = dc.SelectObject(&m_smallFont);
        dc.SetTextColor(muted);
        CRect section(sx(16), sectionY, rc.Width() - sx(16), sectionY + sy(22));
        dc.DrawTextW(L"PLAYLISTS", section, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CPen sectionPen(PS_SOLID, 1, border);
        dc.SelectObject(&sectionPen);
        dc.MoveTo(sx(16), sectionY + sy(24));
        dc.LineTo(rc.Width() - sx(16), sectionY + sy(24));

        CRect listItem(sx(12), sectionY + sy(34), rc.Width() - sx(12), sectionY + sy(74));
        CBrush listBrush(dark ? RGB(11, 22, 35) : RGB(255, 255, 255));
        dc.SelectObject(&listBrush);
        dc.RoundRect(listItem, CPoint(sx(8), sy(8)));
        dc.SelectObject(oldBrush);
        dc.SelectObject(&sectionPen);
        dc.RoundRect(listItem, CPoint(sx(8), sy(8)));

        dc.SetTextColor(text);
        CRect listLabel(listItem.left + sx(12), listItem.top, listItem.right - sx(12), listItem.bottom);
        dc.DrawTextW(L"Default Playlist", listLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CFont* oldMain = dc.SelectObject(&m_font);
        dc.SetTextColor(accent);
        CRect addItem(sx(12), sectionY + sy(82), rc.Width() - sx(12), sectionY + sy(122));
        dc.DrawTextW(L"+  New Playlist", addItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(oldMain);
        dc.SelectObject(old);
    }
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
