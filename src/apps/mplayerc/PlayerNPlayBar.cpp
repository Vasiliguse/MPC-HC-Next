#include "stdafx.h"
#include "MainFrm.h"
#include "PlayerNPlayBar.h"
#include "resource.h"
#include "SvgHelper.h"

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
    const COLORREF fg = active ? RGB(45, 226, 197)
                                : (AfxGetAppSettings().bUseDarkTheme ? RGB(139, 160, 179) : RGB(103, 118, 133));

    int size = std::min(r.Width(), r.Height());
    size = std::max(16, size);
    int x = r.CenterPoint().x - size / 2;
    int y = r.CenterPoint().y - size / 2;

    // Reuse the existing resource-backed SVG strip used by the main toolbar so the sidebar
    // follows the same icon family without introducing a second icon asset pipeline.
    CSvgImage svg;
    if (svg.Load(IDF_SVG_NPLAY_NAV)) {
        int fullW = 0;
        int fullH = 0;
        if (svg.GetOriginalSize(fullW, fullH) && fullW >= 16 && fullH > 0) {
            const int slotW = fullW / 5;
            const int slot = std::clamp(icon, 0, 4);
            int rasterW = slotW;
            int rasterH = fullH;
            if (HBITMAP bitmap = svg.Rasterize(rasterW, rasterH)) {
                CBitmap source;
                source.Attach(bitmap);

                CDC memdc;
                if (memdc.CreateCompatibleDC(&dc)) {
                    CBitmap* oldBitmap = memdc.SelectObject(&source);

                    // Keep inactive SVG artwork neutral while preserving alpha; active items get
                    // a cyan accent by drawing a second, low-cost vector cue at the hit target.
                    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                    const int side = std::min(size, slotW);
                    const int drawX = x + (size - side) / 2;
                    const int drawY = y + (size - side) / 2;
                    AlphaBlend(dc.m_hDC, drawX, drawY, side, side,
                               memdc.m_hDC, slot * slotW, 0, slotW, slotW, blend);

                    if (active) {
                        CPen accentPen(PS_SOLID, std::max(1, dc.GetDeviceCaps(LOGPIXELSX) / 96), fg);
                        CPen* oldPen = dc.SelectObject(&accentPen);
                        CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
                        dc.RoundRect(drawX - 2, drawY - 2, drawX + side + 2, drawY + side + 2, 6, 6);
                        dc.SelectObject(oldBrush);
                        dc.SelectObject(oldPen);
                    }

                    memdc.SelectObject(oldBitmap);
                }

                source.DeleteObject();
                return;
            }
        }
    }

    // Deterministic vector fallback if the shared SVG resource is unavailable.
    CPen pen(PS_SOLID, std::max(1, dc.GetDeviceCaps(LOGPIXELSX) / 48), fg);
    CPen* oldPen = dc.SelectObject(&pen);
    CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));

    const int cx = r.CenterPoint().x;
    const int cy = r.CenterPoint().y;
    const int s = std::max(5, std::min(r.Width(), r.Height()) / 4);

    switch (icon) {
        case 0: // home
            dc.MoveTo(cx - s, cy + s / 2);
            dc.LineTo(cx, cy - s);
            dc.LineTo(cx + s, cy + s / 2);
            dc.MoveTo(cx - s * 3 / 4, cy);
            dc.LineTo(cx - s * 3 / 4, cy + s);
            dc.LineTo(cx + s * 3 / 4, cy + s);
            dc.LineTo(cx + s * 3 / 4, cy);
            break;
        case 2: // video
            dc.RoundRect(cx - s, cy - s * 3 / 4, cx + s, cy + s * 3 / 4, s / 3, s / 3);
            dc.MoveTo(cx - s / 4, cy - s / 3);
            dc.LineTo(cx + s / 3, cy);
            dc.LineTo(cx - s / 4, cy + s / 3);
            break;
        case 9: // audio
            dc.MoveTo(cx - s, cy - s / 3);
            dc.LineTo(cx - s / 3, cy - s / 3);
            dc.LineTo(cx + s / 3, cy - s);
            dc.LineTo(cx + s / 3, cy + s);
            dc.LineTo(cx - s / 3, cy + s / 3);
            dc.LineTo(cx - s, cy + s / 3);
            dc.LineTo(cx - s, cy - s / 3);
            dc.Arc(cx - s / 4, cy - s, cx + s + 2, cy + s, cx + s / 2, cy + s / 2, cx + s / 2, cy - s / 2);
            break;
        default:
            dc.MoveTo(cx - s, cy - s);
            dc.LineTo(cx + s, cy - s);
            dc.MoveTo(cx - s, cy);
            dc.LineTo(cx + s / 2, cy);
            dc.MoveTo(cx - s, cy + s);
            dc.LineTo(cx + s, cy + s);
            break;
    }

    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);
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
        SetCapture();
    }
    __super::OnLButtonDown(nFlags, point);
}

void CPlayerNPlayBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    const int hit = HitTest(point);
    const int pressed = m_pressedItem;
    m_pressedItem = -1;

    if (GetCapture() == this) {
        ReleaseCapture();
    }

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

void CPlayerNPlayBar::OnCaptureChanged(CWnd* pWnd)
{
    m_pressedItem = -1;
    __super::OnCaptureChanged(pWnd);
}

BEGIN_MESSAGE_MAP(CPlayerNPlayBar, CPlayerBar)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()
