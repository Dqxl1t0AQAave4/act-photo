#pragma once

#include "plot.h"


// PlotStatic

class PlotStatic : public CStatic
{
	DECLARE_DYNAMIC(PlotStatic)

public:
	PlotStatic();
	virtual ~PlotStatic();

protected:
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    virtual void DrawItem(LPDRAWITEMSTRUCT /*lpDrawItemStruct*/);
    plot::layer_drawable plot_layer;
    bool symmetric;
private:
    CBitmap *buffer;
};


