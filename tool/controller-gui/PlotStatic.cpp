// PlotStatic.cpp : implementation file
//

#include "stdafx.h"
#include "PlotStatic.h"


// PlotStatic

IMPLEMENT_DYNAMIC(PlotStatic, CStatic)

PlotStatic::PlotStatic()
    : buffer(nullptr)
{
}

PlotStatic::~PlotStatic()
{
    delete buffer;
}


BEGIN_MESSAGE_MAP(PlotStatic, CStatic)
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()



// PlotStatic message handlers




void PlotStatic::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CStatic::OnDrawItem(nIDCtl, lpDrawItemStruct);
}


void PlotStatic::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC *pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
    CDC dc; dc.CreateCompatibleDC(pDC);

    dc.SetBkMode(TRANSPARENT);

    RECT bounds;  GetClientRect(&bounds);
    plot::screen_t screen(0, bounds.right, 0, bounds.bottom);

    if (buffer == nullptr)
    {
        RECT bounds;  GetClientRect(&bounds);
        buffer = new CBitmap;
        buffer->CreateCompatibleBitmap(pDC, bounds.right, bounds.bottom);
    }

    auto old = dc.SelectObject(buffer);

    if (symmetric)
    {
        int min = min(bounds.bottom, bounds.right);
        screen.xmin = (bounds.right - min) / 2;
        screen.xmax = screen.xmin + min;
        screen.ymin = (bounds.bottom - min) / 2;
        screen.ymax = screen.ymin + min;
    }

    plot::viewport vp(screen, {});

    CBrush white(RGB(255, 255, 255));
    dc.FillRect(&bounds, &white);

    this->plot_layer.draw(dc, vp);

    pDC->BitBlt(0, 0, bounds.right, bounds.bottom, &dc, 0, 0, SRCCOPY);

    dc.SelectObject(old);
}
