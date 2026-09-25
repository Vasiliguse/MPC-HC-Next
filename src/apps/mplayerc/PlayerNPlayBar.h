#pragma once

#include "PlayerBar.h"

class CMainFrame;

class CPlayerNPlayBar : public CPlayerBar
{
    DECLARE_DYNAMIC(CPlayerNPlayBar)

public:
    explicit CPlayerNPlayBar(CMainFrame* pMainFrame = nullptr);
    virtual ~CPlayerNPlayBar();

    BOOL Create(CWnd* pParentWnd, UINT defDockBarID);
    virtual void ReloadTranslatableResources() override;

protected:
    struct NavItem {
        LPCWSTR label;
        int icon;
        CRect rect;
    };

    CMainFrame* m_pMainFrame = nullptr;
    CFont m_font;
    CFont m_smallFont;
    int m_hotItem = -1;
    int m_activeItem = 0;
    std::vector<NavItem> m_items;

    void LayoutItems();
    void DrawIcon(CDC& dc, const CRect& r, int icon, bool active) const;
    int HitTest(CPoint point) const;

    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()
};
