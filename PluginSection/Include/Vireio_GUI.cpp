/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

File <Vireio_GUI.cpp>
and Class <Vireio_GUI>:
Copyright (C) 2015 Denis Reischl

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown
v2.0.4 onwards 2014 by Grant Bagwell, Simon Brown and Neil Schneider

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#include "Vireio_GUI.h"

/**
* Small control helper.
***/
bool InRect(RECT rc, POINT pt)
{
	return (pt.x >= rc.left) && (pt.y >= rc.top) && (pt.x <= rc.right) && (pt.y <= rc.bottom);
}

/**
* Constructor.
***/
Vireio_GUI::Vireio_GUI(SIZE sSize, LPCWSTR szFont, BOOL bItalic, DWORD dwFontSize, COLORREF dwColorFront, COLORREF dwColorBack) :
m_eActiveControlAction(Vireio_Control_Action::None),
m_dwActiveControl(0),
m_bMouseBoundToControl(false),
m_dwCurrentPage(0)
{
	// set the size, colors, font size and the font name
	CopyMemory(&m_sGUISize, &sSize, sizeof(SIZE));
	m_dwColorFront = dwColorFront;
	m_dwColorBack = dwColorBack;
	m_dwFontSize = dwFontSize;
	m_szFontName = std::wstring(szFont);

	// create bitmap, set control update to true
	HWND hwnd = GetActiveWindow();
	HDC hdc = GetDC(hwnd);
	m_hBitmapControl = CreateCompatibleBitmap(hdc, (int)sSize.cx, (int)sSize.cy);
	if (!m_hBitmapControl)
		OutputDebugString(L"Failed to create bitmap!");
	m_bControlUpdate = true;

	// create the font
	m_hFont = CreateFont(dwFontSize, 0, 0, 0, 0, bItalic,
		FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
		m_szFontName.c_str());
}

/**
* Destructor.
***/
Vireio_GUI::~Vireio_GUI() {}

/**
* Provides the rendered bitmap for the current frame.
* @returns The current GUI bitmap, nullptr if no changes.
***/
HBITMAP Vireio_GUI::GetGUI()
{
	if (!m_hBitmapControl)
	{
		// create bitmap, set control update to true
		HWND hwnd = GetActiveWindow();
		HDC hdc = GetDC(hwnd);
		m_hBitmapControl = CreateCompatibleBitmap(hdc, (int)m_sGUISize.cx, (int)m_sGUISize.cy);
		if (!m_hBitmapControl)
			OutputDebugString(L"Failed to create bitmap!");
		m_bControlUpdate = true;
	}

	if (m_bControlUpdate)
	{
		// get control bitmap dc
		HDC hdcImage = CreateCompatibleDC(NULL);
		HBITMAP hbmOld = (HBITMAP)SelectObject(hdcImage, m_hBitmapControl);
		HFONT hOldFont;

		// clear the background
		RECT rc;
		SetRect(&rc, 0, 0, (int)m_sGUISize.cx, (int)m_sGUISize.cy);
		FillRect(hdcImage, &rc, (HBRUSH)CreateSolidBrush(m_dwColorBack));

		// create font
		if (!m_hFont)
			m_hFont = CreateFont(m_dwFontSize, 0, 0, 0, 0, FALSE,
			FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
			m_szFontName.c_str());

		// Select the variable stock font into the specified device context. 
		if (hOldFont = (HFONT)SelectObject(hdcImage, m_hFont))
		{
			SetTextColor(hdcImage, m_dwColorFront);
			SetBkColor(hdcImage, m_dwColorBack);

			// verify page index
			if (m_dwCurrentPage >= (UINT)m_asPages.size()) m_dwCurrentPage = (UINT)m_asPages.size() - 1;

			// loop through controls for this page, draw them
			if (m_asPages.size())
			for (UINT dwI = 0; dwI < (UINT)m_asPages[m_dwCurrentPage].m_asControls.size(); dwI++)
			{
				// render control depending on type
				switch (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType)
				{
					case StaticListBox:
						DrawStaticListBox(hdcImage, m_asPages[m_dwCurrentPage].m_asControls[dwI]);
						break;
					case ListBox:
						DrawListBox(hdcImage, m_asPages[m_dwCurrentPage].m_asControls[dwI]);
						break;
					case SpinControl:
						DrawSpinControl(hdcImage, m_asPages[m_dwCurrentPage].m_asControls[dwI]);
						break;
					case EditLine:
						break;
					case Button:
						DrawButton(hdcImage, m_asPages[m_dwCurrentPage].m_asControls[dwI]);
						break;
					case Switch:
						DrawSwitch(hdcImage, m_asPages[m_dwCurrentPage].m_asControls[dwI]);
						break;
					default:
						break;
				}
			}

			// Restore the original font.        
			SelectObject(hdcImage, hOldFont);
		}

		// draw the arrows... first, the arrow background
		RECT sRect;
		sRect.top = m_sGUISize.cy - m_dwFontSize * 4;
		sRect.bottom = m_sGUISize.cy;
		sRect.left = 0;
		sRect.right = m_sGUISize.cx;
		FillRect(hdcImage, &sRect, (HBRUSH)(COLOR_SCROLLBAR + 1));

		// left arrow.. arrows are adjusted by the font size and GUI size
		POINT asPoints[3];
		asPoints[0].x = m_sGUISize.cx >> 4;
		asPoints[0].y = sRect.top + m_dwFontSize * 2;
		asPoints[1].x = (m_sGUISize.cx >> 1) - (m_sGUISize.cx >> 3);
		asPoints[1].y = sRect.top + (m_dwFontSize >> 2);
		asPoints[2].x = asPoints[1].x;
		asPoints[2].y = sRect.bottom - (m_dwFontSize >> 2);
		SelectObject(hdcImage, GetStockObject(DC_PEN));
		SelectObject(hdcImage, GetStockObject(DC_BRUSH));
		if ((m_bMouseBoundToControl) && (m_sMouseCoords.y > (LONG)(m_sGUISize.cy - m_dwFontSize * 4)) && (m_sMouseCoords.x < (LONG)(m_sGUISize.cx >> 1)))
		{
			SetDCPenColor(hdcImage, m_dwColorFront ^ m_dwColorFront);
			SetDCBrushColor(hdcImage, m_dwColorFront ^ m_dwColorFront);
		}
		else
		{
			SetDCPenColor(hdcImage, m_dwColorFront);
			SetDCBrushColor(hdcImage, m_dwColorFront);
		}
		if (m_dwCurrentPage > 0)
			Polygon(hdcImage, asPoints, 3);

		// right arrow
		asPoints[0].x = m_sGUISize.cx - (m_sGUISize.cx >> 4);
		asPoints[0].y = sRect.top + m_dwFontSize * 2;
		asPoints[1].x = (m_sGUISize.cx >> 1) + (m_sGUISize.cx >> 3);
		asPoints[1].y = sRect.top + (m_dwFontSize >> 2);
		asPoints[2].x = asPoints[1].x;
		asPoints[2].y = sRect.bottom - (m_dwFontSize >> 2);
		if ((m_bMouseBoundToControl) && (m_sMouseCoords.y > (LONG)(m_sGUISize.cy - m_dwFontSize * 4)) && (m_sMouseCoords.x >= (LONG)(m_sGUISize.cx >> 1)))
		{
			SetDCPenColor(hdcImage, m_dwColorFront ^ m_dwColorFront);
			SetDCBrushColor(hdcImage, m_dwColorFront ^ m_dwColorFront);
		}
		else
		{
			SetDCPenColor(hdcImage, m_dwColorFront);
			SetDCBrushColor(hdcImage, m_dwColorFront);
		}
		if ((m_asPages.size() > 1) && (m_dwCurrentPage < (m_asPages.size() - 1)))
			Polygon(hdcImage, asPoints, 3);

		// line between
		sRect.left = (m_sGUISize.cx >> 1) - (m_sGUISize.cx >> 7);
		sRect.right = (m_sGUISize.cx >> 1) + (m_sGUISize.cx >> 7);
		FillRect(hdcImage, &sRect, (HBRUSH)(COLOR_BACKGROUND + 1));

		SelectObject(hdcImage, hbmOld);
		DeleteDC(hdcImage);

		// next update only by request, return updated bitmap
		m_bControlUpdate = false;
		return m_hBitmapControl;
	}

	return nullptr;
}

/**
* Draws a static list box.
* @param hdc The handle to a device context (DC) for the client area.
* @param sControl The list box control (both static and versatile).
***/
void Vireio_GUI::DrawStaticListBox(HDC hdc, Vireio_Control& sControl)
{
	// only list box controls
	if ((sControl.m_eControlType != Vireio_Control_Type::ListBox) &&
		(sControl.m_eControlType != Vireio_Control_Type::StaticListBox))
		return;

	// get position and size pointers
	POINT* psPos = &sControl.m_sPosition;
	SIZE* psSize = &sControl.m_sSize;
	int nY = (int)psPos->y;
	if (sControl.m_eControlType == Vireio_Control_Type::ListBox) { nY -= (int)sControl.m_sListBox.m_fScrollPosY; }

	// loop through entries
	if (sControl.m_sStaticListBox.m_paszEntries)
	for (UINT dwJ = 0; dwJ < (UINT)sControl.m_sStaticListBox.m_paszEntries->size(); dwJ++)
	{
		if ((nY >= (int(m_dwFontSize) * -1)) && (nY <= int(psPos->y + psSize->cy)))
			// output the list entry text
			TextOut(hdc,
			psPos->x,
			nY,
			((*(sControl.m_sStaticListBox.m_paszEntries))[dwJ]).c_str(),
			(int)((*(sControl.m_sStaticListBox.m_paszEntries))[dwJ]).length());

		// next line
		nY += (int)m_dwFontSize;
	}

	// draw an empty field at the bottom of the list
	RECT sRect;
	sRect.top = (LONG)psPos->y + psSize->cy;
	sRect.bottom = (LONG)psPos->y + psSize->cy + m_dwFontSize + 1;
	sRect.left = (LONG)psPos->x;
	sRect.right = (LONG)psPos->x + psSize->cx;
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorBack));

	// and an empty field at the right side of the list
	sRect.top = (LONG)psPos->y;
	sRect.bottom = (LONG)psPos->y + psSize->cy + m_dwFontSize + 1;
	sRect.left = (LONG)psPos->x + psSize->cx;
	sRect.right = m_sGUISize.cx;
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorBack));
}

/**
* Draws a static list box.
* @param hdc The handle to a device context (DC) for the client area.
* @param sControl The list box control.
***/
void Vireio_GUI::DrawListBox(HDC hdc, Vireio_Control& sControl)
{
	// first, draw the text using the static list box drawing method
	DrawStaticListBox(hdc, sControl);

	// get position and size pointers
	POINT* psPos = &sControl.m_sPosition;
	SIZE* psSize = &sControl.m_sSize;

	// draw the scroll bar
	UINT dwFullTextSizeY = (UINT)sControl.m_sListBox.m_paszEntries->size() * m_dwFontSize;
	if (dwFullTextSizeY > (UINT)psSize->cy)
	{
		// get max scrollbar y position
		float fMaxScrollBarY = (float)dwFullTextSizeY - (float)psSize->cy;

		// the bar size
		float fBarSizeY;
		INT nBarSizeY;
		fBarSizeY = ((float)psSize->cy / (float)dwFullTextSizeY) * psSize->cy;
		nBarSizeY = (INT)UINT(fBarSizeY);

		// and position,
		float fBarPosY;
		INT nBarPosY;
		fBarPosY = (sControl.m_sListBox.m_fScrollPosY / fMaxScrollBarY) * ((float)psSize->cy - fBarSizeY);
		nBarPosY = (INT)UINT(fBarPosY);

		// and draw
		RECT sRect;
		sRect.top = (LONG)psPos->y + (LONG)nBarPosY;
		sRect.bottom = (LONG)psPos->y + (LONG)nBarPosY + (LONG)nBarSizeY + 1;
		sRect.left = (LONG)psPos->x + psSize->cx;
		sRect.right = (LONG)psPos->x + psSize->cx + m_dwFontSize;
		FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorFront));
	}
}

/**
* Draws a spin control.
* @param hdc The handle to a device context (DC) for the client area.
* @param sControl The list box control.
***/
void Vireio_GUI::DrawSpinControl(HDC hdc, Vireio_Control& sControl)
{
	if (sControl.m_eControlType != Vireio_Control_Type::SpinControl)
		return;

	// get position and size pointers
	POINT* psPos = &sControl.m_sPosition;
	SIZE* psSize = &sControl.m_sSize;

	// output the entry text
	if (sControl.m_sSpinControl.m_dwCurrentSelection < (UINT)sControl.m_sSpinControl.m_paszEntries->size())
	{
		TextOut(hdc,
			psPos->x + (m_dwFontSize >> 4),
			psPos->y + (m_dwFontSize >> 4),
			((*(sControl.m_sSpinControl.m_paszEntries))[sControl.m_sSpinControl.m_dwCurrentSelection]).c_str(),
			(int)((*(sControl.m_sSpinControl.m_paszEntries))[sControl.m_sSpinControl.m_dwCurrentSelection]).length());
	}

	// clear field right of the text
	RECT sRect;
	SetRect(&sRect, (psPos->x + psSize->cx) - m_dwFontSize, psPos->y, m_sGUISize.cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorBack));
	sRect.right = psPos->x + psSize->cx;

	// draw the arrows... up arrow first
	POINT asPoints[3];
	asPoints[0].x = (psPos->x + psSize->cx) - (m_dwFontSize >> 1);
	asPoints[0].y = psPos->y + (m_dwFontSize >> 3);
	asPoints[1].x = (psPos->x + psSize->cx) - ((m_dwFontSize >> 3) * 7);
	asPoints[1].y = psPos->y + ((m_dwFontSize >> 5) * 15);
	asPoints[2].x = (psPos->x + psSize->cx) - (m_dwFontSize >> 3);
	asPoints[2].y = psPos->y + ((m_dwFontSize >> 5) * 15);
	SelectObject(hdc, GetStockObject(DC_PEN));
	SelectObject(hdc, GetStockObject(DC_BRUSH));
	sRect.bottom -= psSize->cy >> 1;
	if ((m_eActiveControlAction == Vireio_Control_Action::SpinControlArrows) && InRect(sRect, m_sMouseCoords))
	{
		SetDCPenColor(hdc, m_dwColorFront ^ m_dwColorFront);
		SetDCBrushColor(hdc, m_dwColorFront ^ m_dwColorFront);
	}
	else
	{
		SetDCPenColor(hdc, m_dwColorFront);
		SetDCBrushColor(hdc, m_dwColorFront);
	}
	if (sControl.m_sSpinControl.m_dwCurrentSelection > 0)
		Polygon(hdc, asPoints, 3);
	asPoints[0].y = (psPos->y + psSize->cy) - (m_dwFontSize >> 3);
	asPoints[1].y = (psPos->y + psSize->cy) - ((m_dwFontSize >> 5) * 15);
	asPoints[2].y = (psPos->y + psSize->cy) - ((m_dwFontSize >> 5) * 15);
	sRect.bottom += psSize->cy >> 1;
	sRect.top += psSize->cy >> 1;
	if ((m_eActiveControlAction == Vireio_Control_Action::SpinControlArrows) && InRect(sRect, m_sMouseCoords))
	{
		SetDCPenColor(hdc, m_dwColorFront ^ m_dwColorFront);
		SetDCBrushColor(hdc, m_dwColorFront ^ m_dwColorFront);
	}
	else
	{
		SetDCPenColor(hdc, m_dwColorFront);
		SetDCBrushColor(hdc, m_dwColorFront);
	}
	if (sControl.m_sSpinControl.m_dwCurrentSelection < (sControl.m_sSpinControl.m_paszEntries->size() - 1))
		Polygon(hdc, asPoints, 3);

	// draw the border
	SetRect(&sRect, psPos->x, psPos->y, psPos->x + psSize->cx, psPos->y + (m_dwFontSize >> 4));
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorFront));
	SetRect(&sRect, psPos->x, psPos->y + psSize->cy - (m_dwFontSize >> 4), psPos->x + psSize->cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorFront));
	SetRect(&sRect, psPos->x, psPos->y, psPos->x + (m_dwFontSize >> 4), psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorFront));
	SetRect(&sRect, psPos->x + psSize->cx - (m_dwFontSize >> 4), psPos->y, psPos->x + psSize->cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorFront));
}

/**
* Draws a button.
* @param hdc The handle to a device context (DC) for the client area.
* @param sControl The list box control.
***/
void Vireio_GUI::DrawButton(HDC hdc, Vireio_Control& sControl)
{
	if (sControl.m_eControlType != Vireio_Control_Type::Button)
		return;

	// get position and size pointers
	POINT* psPos = &sControl.m_sPosition;
	SIZE* psSize = &sControl.m_sSize;

	// output the text
	TextOut(hdc,
		psPos->x + (m_dwFontSize >> 4),
		psPos->y + (m_dwFontSize >> 4),
		sControl.m_sButton.m_pszText->c_str(),
		sControl.m_sButton.m_pszText->size());

	// clear field right of the text
	RECT sRect;
	SetRect(&sRect, psPos->x + psSize->cx, psPos->y, m_sGUISize.cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorBack));

	// select color wether pressed or not
	UINT dwColor = m_dwColorFront;
	if (sControl.m_sButton.m_bPressed)
		dwColor = m_dwColorFront ^ m_dwColorFront;

	// draw the border
	SetRect(&sRect, psPos->x, psPos->y, psPos->x + psSize->cx, psPos->y + (m_dwFontSize >> 4));
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
	SetRect(&sRect, psPos->x, psPos->y + psSize->cy - (m_dwFontSize >> 4), psPos->x + psSize->cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
	SetRect(&sRect, psPos->x, psPos->y, psPos->x + (m_dwFontSize >> 4), psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
	SetRect(&sRect, psPos->x + psSize->cx - (m_dwFontSize >> 4), psPos->y, psPos->x + psSize->cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
}

/**
* Draws a switch.
* @param hdc The handle to a device context (DC) for the client area.
* @param sControl The list box control.
***/
void Vireio_GUI::DrawSwitch(HDC hdc, Vireio_Control& sControl)
{
	if (sControl.m_eControlType != Vireio_Control_Type::Switch)
		return;

	// get position and size pointers
	POINT* psPos = &sControl.m_sPosition;
	SIZE* psSize = &sControl.m_sSize;

	// output the text
	TextOut(hdc,
		psPos->x + (m_dwFontSize >> 4),
		psPos->y + (m_dwFontSize >> 4),
		sControl.m_sSwitch.m_pszText->c_str(),
		sControl.m_sSwitch.m_pszText->size());

	// clear field right of the text
	RECT sRect;
	SetRect(&sRect, (psPos->x + psSize->cx) - m_dwFontSize, psPos->y, m_sGUISize.cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorBack));

	// draw the true/false indicator
	if (sControl.m_sSwitch.m_bTrue)
	{
		SetRect(&sRect, psPos->x + psSize->cx - ((m_dwFontSize >> 2) * 3), psPos->y + (m_dwFontSize >> 2), psPos->x + psSize->cx - (m_dwFontSize >> 2), psPos->y + psSize->cy - (m_dwFontSize >> 2));
		FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(m_dwColorFront));
	}

	// select color wether pressed or not
	UINT dwColor = m_dwColorFront;
	if (sControl.m_sSwitch.m_bPressed)
		dwColor = m_dwColorFront ^ m_dwColorFront;

	// draw the border
	SetRect(&sRect, psPos->x, psPos->y, psPos->x + psSize->cx, psPos->y + (m_dwFontSize >> 4));
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
	SetRect(&sRect, psPos->x, psPos->y + psSize->cy - (m_dwFontSize >> 4), psPos->x + psSize->cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
	SetRect(&sRect, psPos->x, psPos->y, psPos->x + (m_dwFontSize >> 4), psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
	SetRect(&sRect, psPos->x + psSize->cx - (m_dwFontSize >> 4), psPos->y, psPos->x + psSize->cx, psPos->y + psSize->cy);
	FillRect(hdc, &sRect, (HBRUSH)CreateSolidBrush(dwColor));
}

/**
* Adds a Vireio Control to the specified page.
* @param dwPage The index of the page the control will be added. (=id)
***/
UINT Vireio_GUI::AddControl(UINT dwPage, Vireio_Control& sControl)
{
	// page present ?
	if (dwPage < (UINT)m_asPages.size())
	{
		// add control
		if (m_asPages[dwPage].m_asControls.size() < MAX_CONTROLS_PER_PAGE)
			m_asPages[dwPage].m_asControls.push_back(sControl);

		// create id and return
		return (UINT)(dwPage << 16) + (UINT)m_asPages[dwPage].m_asControls.size() - 1;
	}
	else return 0;
}

/**
* Adds an entry to the specified control.
* @param dwControl The id of the control.
***/
void Vireio_GUI::AddEntry(UINT dwControl, LPCWSTR szString)
{
	// decode id to page and index
	UINT dwPage = dwControl >> 16;
	UINT dwIndex = dwControl & 65535;

	// page present ?
	if (dwPage < (UINT)m_asPages.size())
	{
		// control present ?
		if (dwIndex < (UINT)m_asPages[dwPage].m_asControls.size())
		{
			// add the string depending on the control type
			std::wstring sz = std::wstring(szString);
			switch (m_asPages[dwPage].m_asControls[dwIndex].m_eControlType)
			{
				case StaticListBox:
					if (m_asPages[dwPage].m_asControls[dwIndex].m_sStaticListBox.m_paszEntries)
						m_asPages[dwPage].m_asControls[dwIndex].m_sStaticListBox.m_paszEntries->push_back(sz);
					else OutputDebugString(L"Faulty code: Entry vector nullptr !");
					break;
				case ListBox:
					if (m_asPages[dwPage].m_asControls[dwIndex].m_sListBox.m_paszEntries)
						m_asPages[dwPage].m_asControls[dwIndex].m_sListBox.m_paszEntries->push_back(sz);
					else OutputDebugString(L"Faulty code: Entry vector nullptr !");
					break;
				case SpinControl:
					if (m_asPages[dwPage].m_asControls[dwIndex].m_sSpinControl.m_paszEntries)
						m_asPages[dwPage].m_asControls[dwIndex].m_sSpinControl.m_paszEntries->push_back(sz);
					else OutputDebugString(L"Faulty code: Entry vector nullptr !");
					break;
				default:
					break;
			}

		}
	}
}

/**
* Windows event for the GUI.
***/
Vireio_GUI_Event Vireio_GUI::WindowsEvent(UINT msg, WPARAM wParam, LPARAM lParam)
{
	static POINT sMouseCoordsOld;
	static float fScrollBarPosYBackup;

	// create empty return value
	Vireio_GUI_Event sRet;
	ZeroMemory(&sRet, sizeof(Vireio_GUI_Event));
	sRet.eType = Vireio_GUI_Event_Type::NoEvent;

	// get local mouse cursor
	m_sMouseCoords.x = GET_X_LPARAM(lParam) * 4;
	m_sMouseCoords.y = GET_Y_LPARAM(lParam) * 4;

	// update control
	m_bControlUpdate = true;

	switch (msg)
	{
		// left mouse button down ?
		case WM_LBUTTONDOWN:
			// mouse currently bound to any control ?
			if (!m_bMouseBoundToControl)
			{
				// next/previous page ?
				if (m_sMouseCoords.y > (LONG)(m_sGUISize.cy - m_dwFontSize * 4))
				{
					// left/right ?
					if (m_sMouseCoords.x < (LONG)(m_sGUISize.cx >> 1))
					{
						if (m_dwCurrentPage > 0) m_dwCurrentPage--;
					}
					else
					{
						if (m_dwCurrentPage < (UINT)(m_asPages.size() - 1)) m_dwCurrentPage++;
					}
					m_bMouseBoundToControl = true;
				}
				// loop through active controls for this page
				else
				for (UINT dwI = 0; dwI < (UINT)m_asPages[m_dwCurrentPage].m_asControls.size(); dwI++)
				{
#pragma region List Box
					// is this a control with a side- scrollbar ?
					if (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType == Vireio_Control_Type::ListBox)
					{
						// get position and size pointers, full text size
						POINT* psPos = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sPosition;
						SIZE* psSize = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSize;
						UINT dwFullTextSizeY = (UINT)m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sListBox.m_paszEntries->size() * m_dwFontSize;
						if (dwFullTextSizeY >(UINT)psSize->cy)
						{
							// get max scrollbar y position
							float fMaxScrollBarY = (float)dwFullTextSizeY - (float)psSize->cy;

							// get scroll bar size
							float fBarSizeY;
							INT nBarSizeY;
							fBarSizeY = ((float)psSize->cy / (float)dwFullTextSizeY) * psSize->cy;
							nBarSizeY = (INT)UINT(fBarSizeY);

							// and postion
							float fBarPosY;
							INT nBarPosY;
							fBarPosY = (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sListBox.m_fScrollPosY / fMaxScrollBarY) * ((float)psSize->cy - fBarSizeY);
							nBarPosY = (INT)UINT(fBarPosY);

							// get rectangle and test mouse position
							RECT sRect;
							sRect.top = (LONG)psPos->y + (LONG)nBarPosY;
							sRect.bottom = (LONG)psPos->y + (LONG)nBarPosY + (LONG)nBarSizeY + 1;
							sRect.left = (LONG)psPos->x + psSize->cx;
							sRect.right = (LONG)psPos->x + psSize->cx + m_dwFontSize;
							if (InRect(sRect, m_sMouseCoords))
							{
								// set control to active, control action to ScrollBar and mouse bound bool
								m_dwActiveControl = dwI;
								m_bMouseBoundToControl = true;
								m_eActiveControlAction = Vireio_Control_Action::ScrollBar;

								// set old mouse coords and backup the scroll bar position
								sMouseCoordsOld.x = m_sMouseCoords.x;
								sMouseCoordsOld.y = m_sMouseCoords.y;
								fScrollBarPosYBackup = m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sListBox.m_fScrollPosY;
							}

						}
					}
#pragma endregion
#pragma region Spin Control
					// is this a control with a side- scrollbar ?
					else if (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType == Vireio_Control_Type::SpinControl)
					{
						// get position and size pointers, full text size
						POINT* psPos = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sPosition;
						SIZE* psSize = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSize;

						// get the rectangle of the up/down arrows
						RECT sRect;
						SetRect(&sRect, (psPos->x + psSize->cx) - m_dwFontSize, psPos->y, psPos->x + psSize->cx, psPos->y + psSize->cy);
						if (InRect(sRect, m_sMouseCoords))
						{
							// set control to active, control action to SpinControlArrows and mouse bound bool
							m_dwActiveControl = dwI;
							m_bMouseBoundToControl = true;
							m_eActiveControlAction = Vireio_Control_Action::SpinControlArrows;

							// update selection
							if (m_sMouseCoords.y < psPos->y + (psSize->cy >> 1))
							{
								if (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSpinControl.m_dwCurrentSelection > 0)
									m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSpinControl.m_dwCurrentSelection--;
							}
							else
							{
								if (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSpinControl.m_dwCurrentSelection < (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSpinControl.m_paszEntries->size() - 1))
									m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSpinControl.m_dwCurrentSelection++;
							}
						}
					}
#pragma endregion
#pragma region Button
					// is this a button?
					else if (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType == Vireio_Control_Type::Button)
					{
						// get position and size pointers, full text size
						POINT* psPos = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sPosition;
						SIZE* psSize = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSize;

						// get the rectangle of the button
						RECT sRect;
						SetRect(&sRect, psPos->x, psPos->y, psPos->x + psSize->cx, psPos->y + psSize->cy);
						if (InRect(sRect, m_sMouseCoords))
						{
							// set this control active
							m_dwActiveControl = dwI;
							m_bMouseBoundToControl = true;
							m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sButton.m_bPressed = true;
						}
					}
#pragma endregion
#pragma region Button
					// is this a button?
					else if (m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType == Vireio_Control_Type::Switch)
					{
						// get position and size pointers, full text size
						POINT* psPos = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sPosition;
						SIZE* psSize = &m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSize;

						// get the rectangle of the button
						RECT sRect;
						SetRect(&sRect, psPos->x, psPos->y, psPos->x + psSize->cx, psPos->y + psSize->cy);
						if (InRect(sRect, m_sMouseCoords))
						{
							// set this control active
							m_dwActiveControl = dwI;
							m_bMouseBoundToControl = true;
							m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSwitch.m_bTrue =
								!m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSwitch.m_bTrue;
							m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sSwitch.m_bPressed = true;
						}
					}
#pragma endregion
				}
			}
			break;
			// left mouse button up ?
		case WM_LBUTTONUP:
			// set all buttons to not pressed
			for (UINT dwI = 0; dwI < (UINT)m_asPages[m_dwCurrentPage].m_asControls.size(); dwI++)
			{
				if ((m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType == Vireio_Control_Type::Button) ||
					(m_asPages[m_dwCurrentPage].m_asControls[dwI].m_eControlType == Vireio_Control_Type::Switch))
					m_asPages[m_dwCurrentPage].m_asControls[dwI].m_sButton.m_bPressed = false;
			}
			m_eActiveControlAction = Vireio_Control_Action::None;
			m_bMouseBoundToControl = false;
			break;
			// mouse move ?
		case WM_MOUSEMOVE:
			if (!(wParam & MK_LBUTTON)) m_bMouseBoundToControl = false;
			if (m_bMouseBoundToControl)
			{
				switch (m_eActiveControlAction)
				{
					case None:
						break;
					case ScrollBar:
					{
									  // get y difference
									  INT nYDiff = (INT)m_sMouseCoords.y - (INT)sMouseCoordsOld.y;

									  // get control position and size
									  POINT* psPos = &m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sPosition;
									  SIZE* psSize = &m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sSize;
									  UINT dwFullTextSizeY = (UINT)m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sListBox.m_paszEntries->size() * m_dwFontSize;

									  // get scroll bar size
									  float fBarSizeY;
									  fBarSizeY = ((float)psSize->cy / (float)dwFullTextSizeY) * psSize->cy;

									  // get the maximum scroll bar position in pixel
									  float fMaxScrollBarYInPix = (float)psSize->cy - fBarSizeY;
									  float fMaxScrollBarY = (float)dwFullTextSizeY - (float)psSize->cy;

									  // set new scroll bar position
									  m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sListBox.m_fScrollPosY = fScrollBarPosYBackup + (float)nYDiff * (fMaxScrollBarY / fMaxScrollBarYInPix);

									  // and clamp
									  if (m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sListBox.m_fScrollPosY < 0.0f) m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sListBox.m_fScrollPosY = 0.0f;
									  if (m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sListBox.m_fScrollPosY > fMaxScrollBarY) m_asPages[m_dwCurrentPage].m_asControls[m_dwActiveControl].m_sListBox.m_fScrollPosY = fMaxScrollBarY;

									  // update control
									  m_bControlUpdate = true;
					}
						break;
					default:
						break;
				}
			}
			break;
		case WM_RBUTTONDOWN:
			break;
	}

	return sRet;
}