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
    m_navBitmap.DeleteObject();
    m_navBitmapWidth = 0;
    m_navBitmapHeight = 0;

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

    CSvgImage svg;
    if (svg.Load(IDF_SVG_NPLAY_NAV)) {
        int width = 0;
        int height = 0;
        if (svg.GetOriginalSize(width, height) && width >= 16 && height > 0) {
            if (HBITMAP bitmap = svg.Rasterize(width, height)) {
                m_navBitmap.Attach(bitmap);
                m_navBitmapWidth = width;
                m_navBitmapHeight = height;
            }
        }
    }

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

    const int top = m_pMainFrame->ScaleY(94);
    const int row = m_pMainFrame->ScaleY(42);
    const int gap = m_pMainFrame->ScaleY(5);
    const int margin = m_pMainFrame->ScaleX(12);

    for (size_t i = 0; i < m_items.size(); ++i) {
        const int y = top + static_cast<int>(i) * (row + gap);
        m_items[i].rect = CRect(margin, y, rc.Width() - margin, y + row);
    }
}

void CPlayerNPlayBar::DrawIcon(CDC& dc, const CRect& r, int icon, bool active)
{
    const COLORREF fg = active ? RGB(45, 226, 197)
                                : (AfxGetAppSettings().bUseDarkTheme ? RGB(139, 160, 179) : RGB(103, 118, 133));

    int size = std::min(r.Width(), r.Height());
    size = std::max(16, size);
    int x = r.CenterPoint().x - size / 2;
    int y = r.CenterPoint().y - size / 2;

    if (m_navBitmap.GetSafeHandle() && m_navBitmapWidth >= 16 && m_navBitmapHeight > 0) {
        const int slotW = m_navBitmapWidth / 10;
        const int slot = std::clamp(icon, 0, 4) + (active ? 5 : 0);
        if (slotW > 0 && slotW <= m_navBitmapHeight) {
            CDC memdc;
            if (memdc.CreateCompatibleDC(&dc)) {
                CBitmap* oldBitmap = memdc.SelectObject(&m_navBitmap);
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                const int side = std::min(size, slotW);
                const int drawX = x + (size - side) / 2;
                const int drawY = y + (size - side) / 2;
                AlphaBlend(dc.m_hDC, drawX, drawY, side, side,
                           memdc.m_hDC, slot * slotW, 0, slotW, slotW, blend);
                memdc.SelectObject(oldBitmap);
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
        case 3: // audio
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
    const auto scaleX = [this](int value) { return m_pMainFrame->ScaleX(value); };
    const auto scaleY = [this](int value) { return m_pMainFrame->ScaleY(value); };

    CRect brand(scaleX(12), scaleY(12), rc.Width() - scaleX(12), scaleY(72));
    CBrush brandBrush(panel);
    dc.FillRect(brand, &brandBrush);
    dc.SelectObject(&brandBrush);
    dc.RoundRect(brand, CPoint(std::min(scaleX(12), scaleY(12)), std::min(scaleX(12), scaleY(12))));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CPen brandPen(PS_SOLID, 1, border);
    CPen* oldPen = dc.SelectObject(&brandPen);
    CBrush* oldBrush = dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    dc.RoundRect(brand, CPoint(scaleX(12), scaleX(12)));
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);

    const int logoSize = scaleY(34);
    CRect logo(brand.left + scaleX(12), brand.CenterPoint().y - logoSize / 2,
               brand.left + scaleX(12) + logoSize, brand.CenterPoint().y + logoSize / 2);
    CBrush logoBrush(accent);
    dc.SelectObject(&logoBrush);
    dc.RoundRect(logo, CPoint(std::min(scaleX(9), scaleY(9)), std::min(scaleX(9), scaleY(9))));
    dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
    CBrush logoCut(bg);
    CPoint tri[3] = {
        { logo.left + scaleX(13), logo.top + scaleY(9) },
        { logo.left + scaleX(13), logo.bottom - scaleY(9) },
        { logo.right - scaleX(9), logo.CenterPoint().y }
    };
    dc.SelectObject(&logoCut);
    dc.Polygon(tri, 3);

    CFont* oldFont = dc.SelectObject(&m_font);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(text);
    CRect title(brand.left + scaleX(56), brand.top + scaleY(11), brand.right - scaleX(10), brand.top + scaleY(35));
    dc.DrawTextW(L"N PLAY", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CFont* oldSmall = dc.SelectObject(&m_smallFont);
    dc.SetTextColor(accent);
    CRect subtitle(brand.left + scaleX(57), brand.top + scaleY(35), brand.right - scaleX(10), brand.bottom - scaleY(7));
    dc.DrawTextW(L"MEDIA PLAYER", subtitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.SelectObject(oldSmall);
    dc.SelectObject(oldFont);

    // Navigation.
    for (size_t i = 0; i < m_items.size(); ++i) {
        const bool active = static_cast<int>(i) == m_activeItem;
        const bool hot = static_cast<int>(i) == m_hotItem;
        const bool pressed = static_cast<int>(i) == m_pressedItem;
        CRect item = m_items[i].rect;

        if (active || hot || pressed) {
            const COLORREF itemColor = pressed
                ? (dark ? RGB(19, 61, 78) : RGB(216, 235, 242))
                : (active ? panelActive : panelHot);
            CBrush itemBrush(itemColor);
            dc.SelectObject(&itemBrush);
            dc.RoundRect(item, CPoint(std::min(scaleX(9), scaleY(9)), std::min(scaleX(9), scaleY(9))));
            dc.SelectObject(oldBrush);

            CPen itemPen(PS_SOLID, 1, pressed ? accent : (active ? RGB(35, 91, 116) : border));
            CPen* savedPen = dc.SelectObject(&itemPen);
            dc.SelectObject((CBrush*)GetStockObject(NULL_BRUSH));
            dc.RoundRect(item, CPoint(scaleX(9), scaleX(9)));
            dc.SelectObject(savedPen);

            CBrush accentBrush(accent);
            CRect stripe(item.left, item.top + scaleY(9), item.left + scaleX(3), item.bottom - scaleY(9));
            dc.FillRect(stripe, &accentBrush);
        }

        DrawIcon(dc, CRect(item.left + scaleX(13), item.top + scaleY(8),
                           item.left + scaleX(39), item.bottom - scaleY(8)),
                 m_items[i].icon, active);

        CFont* old = dc.SelectObject(&m_font);
        dc.SetTextColor(active ? text : muted);
        CRect label(item.left + scaleX(51), item.top, item.right - scaleX(10), item.bottom);
        dc.DrawTextW(m_items[i].label, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(old);
    }

    // Playlist section. Hide it when the dock is too short so it never overlaps navigation.
    if (rc.Height() >= scaleY(468)) {
        const int sectionY = rc.Height() - scaleY(132);
        CFont* old = dc.SelectObject(&m_smallFont);
        dc.SetTextColor(muted);
        CRect section(scaleX(16), sectionY, rc.Width() - scaleX(16), sectionY + scaleY(22));
        dc.DrawTextW(L"PLAYLISTS", section, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CPen sectionPen(PS_SOLID, 1, border);
        dc.SelectObject(&sectionPen);
        dc.MoveTo(scaleX(16), sectionY + scaleY(24));
        dc.LineTo(rc.Width() - scaleX(16), sectionY + scaleY(24));

        CRect listItem(scaleX(12), sectionY + scaleY(34), rc.Width() - scaleX(12), sectionY + scaleY(74));
        CBrush listBrush(dark ? RGB(11, 22, 35) : RGB(255, 255, 255));
        dc.SelectObject(&listBrush);
        dc.RoundRect(listItem, CPoint(std::min(scaleX(8), scaleY(8)), std::min(scaleX(8), scaleY(8))));
        dc.SelectObject(oldBrush);
        dc.SelectObject(&sectionPen);
        dc.RoundRect(listItem, CPoint(scaleY(8), scaleY(8)));

        dc.SetTextColor(text);
        CRect listLabel(listItem.left + scaleX(12), listItem.top, listItem.right - scaleX(12), listItem.bottom);
        dc.DrawTextW(L"Default Playlist", listLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CFont* oldMain = dc.SelectObject(&m_font);
        dc.SetTextColor(accent);
        CRect addItem(scaleX(12), sectionY + scaleY(82), rc.Width() - scaleX(12), sectionY + scaleY(122));
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

void CPlayerNPlayBar::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    const int count = static_cast<int>(m_items.size());
    if (count <= 0) {
        __super::OnKeyDown(nChar, nRepCnt, nFlags);
        return;
    }

    int next = m_activeItem;
    switch (nChar) {
        case VK_UP:
        case VK_LEFT:
            next = (m_activeItem + count - 1) % count;
            break;
        case VK_DOWN:
        case VK_RIGHT:
            next = (m_activeItem + 1) % count;
            break;
        case VK_HOME:
            next = 0;
            break;
        case VK_END:
            next = count - 1;
            break;
        case VK_RETURN:
        case VK_SPACE:
            if (!m_pMainFrame) {
                return;
            }
            switch (m_activeItem) {
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
            return;
        default:
            __super::OnKeyDown(nChar, nRepCnt, nFlags);
            return;
    }

    if (next != m_activeItem) {
        SetActiveItem(next);
    }
}

void CPlayerNPlayBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();
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
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
