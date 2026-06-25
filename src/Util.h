/*
 *  TVTest DTV Video Decoder
 *  Copyright (C) 2015-2022 DBCTRADO
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <commctrl.h>


template<typename T> inline void SafeDelete(T *&p)
{
	if (p) {
		delete p;
		p = nullptr;
	}
}

template<typename T> inline void SafeDeleteArray(T *&p)
{
	if (p) {
		delete [] p;
		p = nullptr;
	}
}

inline int PopCount(uint32_t v)
{
	// return _mm_popcnt_u32(v);
	v = v - ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return (((v + (v >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

void ReduceFraction(int *pNum, int *pDenom);

bool IsVideoInfo(const AM_MEDIA_TYPE *pmt);
bool IsVideoInfo2(const AM_MEDIA_TYPE *pmt);
bool IsMpeg1VideoInfo(const AM_MEDIA_TYPE *pmt);
bool IsMpeg2VideoInfo(const AM_MEDIA_TYPE *pmt);
bool GetAvgTimePerFrame(const AM_MEDIA_TYPE *pmt, REFERENCE_TIME *prtAvgTimePerFrame);
bool GetBitmapInfoHeader(const AM_MEDIA_TYPE *pmt, BITMAPINFOHEADER *pbmih);
BITMAPINFOHEADER *GetBitmapInfoHeader(AM_MEDIA_TYPE *pmt);
const BITMAPINFOHEADER *GetBitmapInfoHeader(const AM_MEDIA_TYPE *pmt);
CLSID GetConnectedFilterCLSID(CBasePin *pPin);
bool IsMediaTypeInterlaced(const AM_MEDIA_TYPE *pmt);
SIZE GetDXVASurfaceSize(const SIZE &Size, bool fMPEG2);
bool IsWindows8OrGreater();

bool IsDarkThemeSupported();
bool SetWindowDarkTheme(HWND hwnd, bool fDark);
bool SetAppAllowDarkMode(bool fAllow);
bool SetWindowAllowDarkMode(HWND hwnd, bool fAllow);
bool SetWindowFrameDarkMode(HWND hwnd, bool fDarkMode);
bool IsDarkMode();
bool IsHighContrast();
bool IsDarkModeSettingChanged(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

class CDialogDarkModeSupport
{
public:
	CDialogDarkModeSupport();
	~CDialogDarkModeSupport();

	void Initialize(HWND hwnd);
	bool HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, INT_PTR *pResult);

private:
	bool m_fInitialized;
	bool m_fAllowDarkMode;
	bool m_fDarkMode;
	COLORREF m_BackColor;
	COLORREF m_FaceColor;
	COLORREF m_TextColor;
	COLORREF m_DisabledTextColor;
	HBRUSH m_hBackBrush;
	HBRUSH m_hFaceBrush;

	void UpdateColors();
	void InitializeControls(HWND hwnd);
	void UpdateControlColors(HWND hwnd);
	LRESULT HandleCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam) const;
	LRESULT HandleNotify(LPARAM lParam) const;
	LRESULT CustomDrawButton(LPARAM lParam) const;
	void DrawButton(HWND hwnd, HDC hdc) const;
	void DrawGroupBox(HWND hwnd, HDC hdc) const;
	bool SetControlDarkTheme(HWND hwnd, bool fDark);
	void Redraw(HWND hwnd) const;
	void FreeBrushes();

	static BOOL CALLBACK InitializeControlsProc(HWND hwnd, LPARAM lParam);
	static BOOL CALLBACK UpdateControlColorsProc(HWND hwnd, LPARAM lParam);
	static LRESULT CALLBACK GroupBoxSubclassProc(
		HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	static LRESULT CALLBACK ButtonSubclassProc(
		HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
};
