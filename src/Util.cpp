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

#include "stdafx.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include "Util.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")


namespace {

constexpr UINT_PTR GROUPBOX_SUBCLASS_ID = 1;
constexpr UINT_PTR BUTTON_SUBCLASS_ID = 2;

enum class PreferredAppMode {
	Default,
	AllowDark,
	ForceDark,
	ForceLight,
};

template<typename T> T GetUxThemeOrdinal(WORD Ordinal)
{
	const HMODULE hModule = ::GetModuleHandle(TEXT("uxtheme.dll"));
	if (!hModule)
		return nullptr;

	return reinterpret_cast<T>(::GetProcAddress(hModule, MAKEINTRESOURCEA(Ordinal)));
}

BOOLEAN WINAPI ShouldAppsUseDarkMode_()
{
	typedef BOOLEAN (WINAPI *ShouldAppsUseDarkModeFunc)();

	const auto pShouldAppsUseDarkMode = GetUxThemeOrdinal<ShouldAppsUseDarkModeFunc>(132);
	if (!pShouldAppsUseDarkMode)
		return FALSE;

	return pShouldAppsUseDarkMode();
}

BOOL WINAPI AllowDarkModeForWindow_(HWND hwnd, BOOL fAllow)
{
	typedef BOOL (WINAPI *AllowDarkModeForWindowFunc)(HWND, BOOL);

	const auto pAllowDarkModeForWindow = GetUxThemeOrdinal<AllowDarkModeForWindowFunc>(133);
	if (!pAllowDarkModeForWindow)
		return FALSE;

	return pAllowDarkModeForWindow(hwnd, fAllow);
}

void WINAPI RefreshImmersiveColorPolicyState_()
{
	typedef void (WINAPI *RefreshImmersiveColorPolicyStateFunc)();

	const auto pRefreshImmersiveColorPolicyState =
		GetUxThemeOrdinal<RefreshImmersiveColorPolicyStateFunc>(104);
	if (!pRefreshImmersiveColorPolicyState)
		return;

	pRefreshImmersiveColorPolicyState();
}

PreferredAppMode WINAPI SetPreferredAppMode_(PreferredAppMode Mode)
{
	typedef PreferredAppMode (WINAPI *SetPreferredAppModeFunc)(PreferredAppMode);

	const auto pSetPreferredAppMode = GetUxThemeOrdinal<SetPreferredAppModeFunc>(135);
	if (!pSetPreferredAppMode)
		return PreferredAppMode::Default;

	return pSetPreferredAppMode(Mode);
}

}	// namespace


// GCD (Greatest Common Denominator)
static int GCD(int m, int n)
{
	if (m != 0 && n != 0) {
		int r;

		do {
			r = m % n;
			m = n;
			n = r;
		} while (r != 0);
	} else {
		m = 0;
	}

	return m;
}

void ReduceFraction(int *pNum, int *pDenom)
{
	if (*pDenom < 0) {
		*pDenom = -*pDenom;
		*pNum = -*pNum;
	}

	if (*pDenom != 0 && *pNum != 0) {
		int d = GCD(*pDenom, *pNum);

		if (d != 0) {
			*pNum /= d;
			*pDenom /= d;
		}
	}
}

bool IsVideoInfo(const AM_MEDIA_TYPE *pmt)
{
	return pmt != nullptr
		&& (pmt->formattype == FORMAT_VideoInfo || pmt->formattype == FORMAT_MPEGVideo)
		&& pmt->cbFormat >= sizeof(VIDEOINFOHEADER)
		&& pmt->pbFormat != nullptr;
}

bool IsVideoInfo2(const AM_MEDIA_TYPE *pmt)
{
	return pmt != nullptr
		&& (pmt->formattype == FORMAT_VideoInfo2 || pmt->formattype == FORMAT_MPEG2_VIDEO)
		&& pmt->cbFormat >= sizeof(VIDEOINFOHEADER2)
		&& pmt->pbFormat != nullptr;
}

bool IsMpeg1VideoInfo(const AM_MEDIA_TYPE *pmt)
{
	return pmt != nullptr
		&& pmt->formattype == FORMAT_MPEGVideo
		&& pmt->cbFormat >= sizeof(MPEG1VIDEOINFO)
		&& pmt->pbFormat != nullptr;
}

bool IsMpeg2VideoInfo(const AM_MEDIA_TYPE *pmt)
{
	return pmt != nullptr
		&& pmt->formattype == FORMAT_MPEG2_VIDEO
		&& pmt->cbFormat >= sizeof(MPEG2VIDEOINFO)
		&& pmt->pbFormat != nullptr;
}

bool GetAvgTimePerFrame(const AM_MEDIA_TYPE *pmt, REFERENCE_TIME *prtAvgTimePerFrame)
{
	REFERENCE_TIME rt;

	if (IsVideoInfo(pmt) || IsVideoInfo2(pmt)) {
		rt = ((const VIDEOINFOHEADER*)pmt->pbFormat)->AvgTimePerFrame;
	} else {
		return false;
	}

	if (rt < 1)
		rt = 1;

	*prtAvgTimePerFrame = rt;

	return true;
}

bool GetBitmapInfoHeader(const AM_MEDIA_TYPE *pmt, BITMAPINFOHEADER *pbmih)
{
	if (!pbmih)
		return false;

	if (IsVideoInfo(pmt)) {
		const VIDEOINFOHEADER *pvih = (const VIDEOINFOHEADER*)pmt->pbFormat;
		*pbmih = pvih->bmiHeader;
		return true;
	} else if (IsVideoInfo2(pmt)) {
		const VIDEOINFOHEADER2 *pvih2 = (const VIDEOINFOHEADER2*)pmt->pbFormat;
		*pbmih = pvih2->bmiHeader;
		return true;
	}

	ZeroMemory(pbmih, sizeof(BITMAPINFOHEADER));

	return false;
}

BITMAPINFOHEADER *GetBitmapInfoHeader(AM_MEDIA_TYPE *pmt)
{
	if (IsVideoInfo(pmt)) {
		VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER*)pmt->pbFormat;
		return &pvih->bmiHeader;
	} else if (IsVideoInfo2(pmt)) {
		VIDEOINFOHEADER2 *pvih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
		return &pvih2->bmiHeader;
	}

	return nullptr;
}

const BITMAPINFOHEADER *GetBitmapInfoHeader(const AM_MEDIA_TYPE *pmt)
{
	if (IsVideoInfo(pmt)) {
		const VIDEOINFOHEADER *pvih = (const VIDEOINFOHEADER*)pmt->pbFormat;
		return &pvih->bmiHeader;
	} else if (IsVideoInfo2(pmt)) {
		const VIDEOINFOHEADER2 *pvih2 = (const VIDEOINFOHEADER2*)pmt->pbFormat;
		return &pvih2->bmiHeader;
	}

	return nullptr;
}

CLSID GetConnectedFilterCLSID(CBasePin *pPin)
{
	CLSID clsid = GUID_NULL;

	if (pPin) {
		IPin *pConnectedPin = pPin->GetConnected();

		if (pConnectedPin) {
			PIN_INFO pi;

			if (SUCCEEDED(pConnectedPin->QueryPinInfo(&pi))) {
				if (pi.pFilter) {
					pi.pFilter->GetClassID(&clsid);
					pi.pFilter->Release();
				}
			}
		}
	}

	return clsid;
}

bool IsMediaTypeInterlaced(const AM_MEDIA_TYPE *pmt)
{
	return IsVideoInfo2(pmt)
		&& (((const VIDEOINFOHEADER2*)pmt->pbFormat)->dwInterlaceFlags & AMINTERLACE_IsInterlaced);
}

SIZE GetDXVASurfaceSize(const SIZE &Size, bool fMPEG2)
{
	/*
		NOTE: from ffmpeg
		decoding MPEG-2 requires additional alignment on some Intel GPUs,
		but it causes issues for H.264 on certain AMD GPUs.....
	*/
	const int Align = fMPEG2 ? 32 : 16;

	SIZE AlignedSize;
	AlignedSize.cx = (Size.cx + (Align - 1)) & ~(Align - 1);
	AlignedSize.cy = (Size.cy + (Align - 1)) & ~(Align - 1);

	return AlignedSize;
}

bool IsWindows8OrGreater()
{
	OSVERSIONINFOEX osvi = {sizeof(OSVERSIONINFOEX)};
	osvi.dwMajorVersion = 6;
	osvi.dwMinorVersion = 2;

	DWORDLONG ConditionMask = 0;
	ConditionMask |= ::VerSetConditionMask(ConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	ConditionMask |= ::VerSetConditionMask(ConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

	return ::VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, ConditionMask) != FALSE;
}

bool IsDarkThemeSupported()
{
	return GetUxThemeOrdinal<void *>(132) != nullptr
		&& GetUxThemeOrdinal<void *>(133) != nullptr;
}

bool SetWindowDarkTheme(HWND hwnd, bool fDark)
{
	if (!::IsWindow(hwnd) || !IsDarkThemeSupported())
		return false;

	return SUCCEEDED(::SetWindowTheme(hwnd, fDark ? L"DarkMode_Explorer" : nullptr, nullptr));
}

bool SetAppAllowDarkMode(bool fAllow)
{
	if (!IsDarkThemeSupported())
		return false;

	SetPreferredAppMode_(fAllow ? PreferredAppMode::AllowDark : PreferredAppMode::Default);
	RefreshImmersiveColorPolicyState_();

	return true;
}

bool SetWindowAllowDarkMode(HWND hwnd, bool fAllow)
{
	if (!::IsWindow(hwnd) || !IsDarkThemeSupported())
		return false;

	return AllowDarkModeForWindow_(hwnd, fAllow) != FALSE;
}

bool SetWindowFrameDarkMode(HWND hwnd, bool fDarkMode)
{
	if (!::IsWindow(hwnd) || !IsDarkThemeSupported())
		return false;

	const BOOL fDark = fDarkMode ? TRUE : FALSE;

	if (SUCCEEDED(::DwmSetWindowAttribute(hwnd, 20, &fDark, sizeof(fDark))))
		return true;

	return SUCCEEDED(::DwmSetWindowAttribute(hwnd, 19, &fDark, sizeof(fDark)));
}

bool IsHighContrast()
{
	HIGHCONTRAST HighContrast = {sizeof(HighContrast)};

	return ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HighContrast), &HighContrast, FALSE)
		&& !!(HighContrast.dwFlags & HCF_HIGHCONTRASTON);
}

bool IsDarkMode()
{
	return IsDarkThemeSupported() && !IsHighContrast() && ShouldAppsUseDarkMode_() != FALSE;
}

bool IsDarkModeSettingChanged(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(wParam);

	if (Message == WM_SETTINGCHANGE) {
		LPCTSTR pszName = reinterpret_cast<LPCTSTR>(lParam);

		if (pszName != nullptr && ::lstrcmpi(pszName, TEXT("ImmersiveColorSet")) == 0)
			return true;
	}

	return false;
}

CDialogDarkModeSupport::CDialogDarkModeSupport()
	: m_fInitialized(false)
	, m_fAllowDarkMode(false)
	, m_fDarkMode(false)
	, m_BackColor(RGB(32, 32, 32))
	, m_FaceColor(RGB(45, 45, 48))
	, m_TextColor(RGB(241, 241, 241))
	, m_DisabledTextColor(RGB(140, 140, 140))
	, m_hBackBrush(nullptr)
	, m_hFaceBrush(nullptr)
{
}

CDialogDarkModeSupport::~CDialogDarkModeSupport()
{
	FreeBrushes();
}

void CDialogDarkModeSupport::Initialize(HWND hwnd)
{
	if (!::IsWindow(hwnd))
		return;

	SetAppAllowDarkMode(true);
	m_fAllowDarkMode = SetWindowAllowDarkMode(hwnd, true);
	m_fDarkMode = IsDarkMode();
	if ((::GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) == 0)
		SetWindowFrameDarkMode(hwnd, m_fDarkMode);

	UpdateColors();
	InitializeControls(hwnd);
	UpdateControlColors(hwnd);
	Redraw(hwnd);
	m_fInitialized = true;
}

bool CDialogDarkModeSupport::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, INT_PTR *pResult)
{
	switch (uMsg) {
	case WM_ERASEBKGND:
		if (m_fDarkMode && m_hFaceBrush) {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			RECT rc;
			::GetClientRect(hwnd, &rc);
			::FillRect(hdc, &rc, m_hFaceBrush);
			if (pResult)
				*pResult = TRUE;
			return true;
		}
		break;

	case WM_PRINTCLIENT:
		if (m_fDarkMode && m_hFaceBrush) {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			RECT rc;
			::GetClientRect(hwnd, &rc);
			::FillRect(hdc, &rc, m_hFaceBrush);
			if (pResult)
				*pResult = TRUE;
			return true;
		}
		break;

	case WM_CTLCOLORBTN:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
		if (m_fDarkMode) {
			if (pResult)
				*pResult = HandleCtlColor(uMsg, wParam, lParam);
			return pResult != nullptr && *pResult != FALSE;
		}
		break;

	case WM_NOTIFY:
		if (m_fDarkMode && pResult) {
			*pResult = HandleNotify(lParam);
			return *pResult != 0;
		}
		break;

	case WM_SETTINGCHANGE:
		if (m_fAllowDarkMode && IsDarkModeSettingChanged(hwnd, uMsg, wParam, lParam)) {
			const bool fDarkMode = IsDarkMode();

			if (m_fDarkMode != fDarkMode) {
				m_fDarkMode = fDarkMode;
				if ((::GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) == 0)
					SetWindowFrameDarkMode(hwnd, fDarkMode);
				UpdateColors();
				InitializeControls(hwnd);
				UpdateControlColors(hwnd);
				Redraw(hwnd);
			}
		}
		break;

	case WM_THEMECHANGED:
	case WM_SYSCOLORCHANGE:
		UpdateColors();
		InitializeControls(hwnd);
		UpdateControlColors(hwnd);
		Redraw(hwnd);
		break;

	case WM_DESTROY:
		FreeBrushes();
		m_fInitialized = false;
		break;
	}

	return false;
}

void CDialogDarkModeSupport::UpdateColors()
{
	FreeBrushes();

	if (m_fDarkMode) {
		m_hBackBrush = ::CreateSolidBrush(m_BackColor);
		m_hFaceBrush = ::CreateSolidBrush(m_FaceColor);
	}
}

void CDialogDarkModeSupport::InitializeControls(HWND hwnd)
{
	SetControlDarkTheme(hwnd, m_fDarkMode);
	::EnumChildWindows(hwnd, InitializeControlsProc, reinterpret_cast<LPARAM>(this));
}

void CDialogDarkModeSupport::UpdateControlColors(HWND hwnd)
{
	::EnumChildWindows(hwnd, UpdateControlColorsProc, reinterpret_cast<LPARAM>(this));
}

LRESULT CDialogDarkModeSupport::HandleCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam) const
{
	const HWND hwnd = reinterpret_cast<HWND>(lParam);
	const HDC hdc = reinterpret_cast<HDC>(wParam);
	bool fFaceColor = false;
	bool fTransparent = false;
	bool fGroupBox = false;

	TCHAR szClassName[64] = TEXT("");
	if (::GetClassName(hwnd, szClassName, _countof(szClassName)) < 1)
		return FALSE;

	switch (uMsg) {
	case WM_CTLCOLORBTN:
		fFaceColor = true;
		if (::lstrcmpi(szClassName, TEXT("BUTTON")) == 0) {
			const DWORD Style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
			fGroupBox = (Style & BS_TYPEMASK) == BS_GROUPBOX;
			fTransparent = fGroupBox || (Style & (BS_AUTOCHECKBOX | BS_AUTO3STATE | BS_AUTORADIOBUTTON | BS_CHECKBOX | BS_RADIOBUTTON)) != 0;
		}
		break;

	case WM_CTLCOLORSTATIC:
		fFaceColor = true;
		fTransparent = true;
		break;

	case WM_CTLCOLOREDIT:
		{
			if (::GetClassName(hwnd, szClassName, _countof(szClassName)) == 8
					&& ::lstrcmpi(szClassName, TEXT("COMBOBOX")) == 0) {
				fFaceColor = true;
			}
		}
		break;

	case WM_CTLCOLORLISTBOX:
		{
			if (::GetClassName(hwnd, szClassName, _countof(szClassName)) == 9
					&& ::lstrcmpi(szClassName, TEXT("ComboLBox")) == 0) {
				fFaceColor = true;
			}
		}
		break;
	}

	const HBRUSH hBrush = fFaceColor ? m_hFaceBrush : m_hBackBrush;
	if (!hBrush)
		return FALSE;

	::SetTextColor(hdc, ::IsWindowEnabled(hwnd) ? m_TextColor : m_DisabledTextColor);
	if (fTransparent) {
		::SetBkMode(hdc, TRANSPARENT);
	} else {
		::SetBkMode(hdc, OPAQUE);
		::SetBkColor(hdc, fFaceColor ? m_FaceColor : m_BackColor);
	}

	if (fGroupBox)
		return reinterpret_cast<LRESULT>(m_hFaceBrush ? m_hFaceBrush : hBrush);

	return reinterpret_cast<LRESULT>(hBrush);
}

LRESULT CDialogDarkModeSupport::HandleNotify(LPARAM lParam) const
{
	const NMHDR *pnmh = reinterpret_cast<const NMHDR*>(lParam);
	if (!pnmh || pnmh->code != NM_CUSTOMDRAW)
		return 0;

	TCHAR szClassName[64] = TEXT("");
	if (::GetClassName(pnmh->hwndFrom, szClassName, _countof(szClassName)) < 1)
		return 0;

	if (::lstrcmpi(szClassName, TEXT("BUTTON")) == 0)
		return CustomDrawButton(lParam);

	return 0;
}

LRESULT CDialogDarkModeSupport::CustomDrawButton(LPARAM lParam) const
{
	NMCUSTOMDRAW *pnmcd = reinterpret_cast<NMCUSTOMDRAW*>(lParam);
	if (!pnmcd || pnmcd->dwDrawStage != CDDS_PREPAINT)
		return 0;

	const HWND hwnd = pnmcd->hdr.hwndFrom;
	const DWORD Style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
	const DWORD Type = Style & BS_TYPEMASK;

	if ((Style & BS_PUSHLIKE) != 0)
		return 0;

	int Part = 0;
	int State = 0;
	const bool fChecked = Button_GetCheck(hwnd) == BST_CHECKED;
	const bool fDisabled = (pnmcd->uItemState & CDIS_DISABLED) != 0 || !::IsWindowEnabled(hwnd);
	const bool fHot = (pnmcd->uItemState & CDIS_HOT) != 0;
	const bool fPressed = (pnmcd->uItemState & CDIS_SELECTED) != 0;
	const bool fIndeterminate = Button_GetCheck(hwnd) == BST_INDETERMINATE;

	switch (Type) {
	case BS_CHECKBOX:
	case BS_AUTOCHECKBOX:
	case BS_3STATE:
	case BS_AUTO3STATE:
		Part = BP_CHECKBOX;
		if (fChecked) {
			State = fPressed ? CBS_CHECKEDPRESSED :
				fDisabled ? CBS_CHECKEDDISABLED :
				fHot ? CBS_CHECKEDHOT :
				CBS_CHECKEDNORMAL;
		} else if (fIndeterminate) {
			State = fPressed ? CBS_MIXEDPRESSED :
				fDisabled ? CBS_MIXEDDISABLED :
				fHot ? CBS_MIXEDHOT :
				CBS_MIXEDNORMAL;
		} else {
			State = fPressed ? CBS_UNCHECKEDPRESSED :
				fDisabled ? CBS_UNCHECKEDDISABLED :
				fHot ? CBS_UNCHECKEDHOT :
				CBS_UNCHECKEDNORMAL;
		}
		break;

	case BS_RADIOBUTTON:
	case BS_AUTORADIOBUTTON:
		Part = BP_RADIOBUTTON;
		if (fChecked) {
			State = fPressed ? RBS_CHECKEDPRESSED :
				fDisabled ? RBS_CHECKEDDISABLED :
				fHot ? RBS_CHECKEDHOT :
				RBS_CHECKEDNORMAL;
		} else {
			State = fPressed ? RBS_UNCHECKEDPRESSED :
				fDisabled ? RBS_UNCHECKEDDISABLED :
				fHot ? RBS_UNCHECKEDHOT :
				RBS_UNCHECKEDNORMAL;
		}
		break;

	default:
		return 0;
	}

	const HTHEME hTheme = ::OpenThemeData(hwnd, VSCLASS_BUTTON);
	if (!hTheme)
		return 0;

	SIZE CheckSize = {0, 0};
	::GetThemePartSize(hTheme, pnmcd->hdc, Part, State, nullptr, TS_DRAW, &CheckSize);

	::FillRect(pnmcd->hdc, &pnmcd->rc, m_hFaceBrush);

	RECT rcMark = pnmcd->rc;
	rcMark.top += ((rcMark.bottom - rcMark.top) - CheckSize.cy) / 2;
	rcMark.right = rcMark.left + CheckSize.cx;
	rcMark.bottom = rcMark.top + CheckSize.cy;
	::DrawThemeBackground(hTheme, pnmcd->hdc, Part, State, &rcMark, nullptr);

	TCHAR szText[256] = TEXT("");
	const int TextLength = ::GetWindowText(hwnd, szText, _countof(szText));
	if (TextLength > 0) {
		SIZE Size = {0, 0};
		::GetTextExtentPoint32(pnmcd->hdc, TEXT("0"), 1, &Size);
		RECT rcText = pnmcd->rc;
		rcText.left = rcMark.right + Size.cx / 2;

		UINT Format = DT_LEFT;
		if ((Style & BS_MULTILINE) != 0)
			Format |= DT_WORDBREAK;
		else
			Format |= DT_SINGLELINE | DT_VCENTER;
		if ((::SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) != 0)
			Format |= DT_HIDEPREFIX;

		const HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(hwnd, WM_GETFONT, 0, 0));
		const HGDIOBJ hOldFont = hFont ? ::SelectObject(pnmcd->hdc, hFont) : nullptr;
		const int OldBkMode = ::SetBkMode(pnmcd->hdc, TRANSPARENT);
		const COLORREF OldTextColor = ::SetTextColor(
			pnmcd->hdc, fDisabled ? m_DisabledTextColor : m_TextColor);
		::DrawText(pnmcd->hdc, szText, TextLength, &rcText, Format);
		::SetTextColor(pnmcd->hdc, OldTextColor);
		::SetBkMode(pnmcd->hdc, OldBkMode);
		if (hOldFont)
			::SelectObject(pnmcd->hdc, hOldFont);
	}

	::CloseThemeData(hTheme);
	return CDRF_SKIPDEFAULT;
}

void CDialogDarkModeSupport::DrawGroupBox(HWND hwnd, HDC hdc) const
{
	RECT rc;
	::GetClientRect(hwnd, &rc);

	TCHAR szText[256] = TEXT("");
	const int TextLength = ::GetWindowText(hwnd, szText, _countof(szText));
	const HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(hwnd, WM_GETFONT, 0, 0));
	const HGDIOBJ hOldFont = hFont ? ::SelectObject(hdc, hFont) : nullptr;
	const int OldBkMode = ::SetBkMode(hdc, TRANSPARENT);

	TEXTMETRIC tm = {};
	::GetTextMetrics(hdc, &tm);
	RECT rcBox = rc;
	rcBox.top += (tm.tmHeight - tm.tmDescent) / 2;

	::FillRect(hdc, &rc, m_hFaceBrush);

	if (TextLength > 0) {
		RECT rcText = {};
		UINT Format = DT_SINGLELINE;
		if ((::SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) != 0)
			Format |= DT_HIDEPREFIX;
		::DrawText(hdc, szText, TextLength, &rcText, Format | DT_CALCRECT);

		const int Margin = 8;
		::OffsetRect(&rcText, Margin, rc.top);
		RECT rcBack = rcText;
		::InflateRect(&rcBack, 2, 0);
		::ExcludeClipRect(hdc, rcBack.left, rcBack.top, rcBack.right, rcBack.bottom);

		HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(96, 96, 96));
		const HGDIOBJ hOldPen = ::SelectObject(hdc, hPen);
		const HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
		::Rectangle(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom);
		::SelectObject(hdc, hOldBrush);
		::SelectObject(hdc, hOldPen);
		::DeleteObject(hPen);
		::SelectClipRgn(hdc, nullptr);

		::FillRect(hdc, &rcBack, m_hFaceBrush);

		const COLORREF OldTextColor = ::SetTextColor(hdc, m_TextColor);
		::DrawText(hdc, szText, TextLength, &rcText, Format);
		::SetTextColor(hdc, OldTextColor);
	} else {
		HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(96, 96, 96));
		const HGDIOBJ hOldPen = ::SelectObject(hdc, hPen);
		const HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
		::Rectangle(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom);
		::SelectObject(hdc, hOldBrush);
		::SelectObject(hdc, hOldPen);
		::DeleteObject(hPen);
	}

	::SetBkMode(hdc, OldBkMode);
	if (hOldFont)
		::SelectObject(hdc, hOldFont);
}

void CDialogDarkModeSupport::DrawButton(HWND hwnd, HDC hdc) const
{
	const DWORD Style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
	const DWORD Type = Style & BS_TYPEMASK;
	if ((Type != BS_RADIOBUTTON && Type != BS_AUTORADIOBUTTON
			&& Type != BS_CHECKBOX && Type != BS_AUTOCHECKBOX
			&& Type != BS_3STATE && Type != BS_AUTO3STATE)
			|| (Style & BS_PUSHLIKE) != 0) {
		return;
	}

	const HTHEME hTheme = ::OpenThemeData(hwnd, VSCLASS_BUTTON);
	if (!hTheme)
		return;

	const bool fChecked = Button_GetCheck(hwnd) == BST_CHECKED;
	const bool fIndeterminate = Button_GetCheck(hwnd) == BST_INDETERMINATE;
	const bool fDisabled = !::IsWindowEnabled(hwnd);
	int Part = 0;
	int State = 0;

	if (Type == BS_RADIOBUTTON || Type == BS_AUTORADIOBUTTON) {
		Part = BP_RADIOBUTTON;
		State = fChecked
			? (fDisabled ? RBS_CHECKEDDISABLED : RBS_CHECKEDNORMAL)
			: (fDisabled ? RBS_UNCHECKEDDISABLED : RBS_UNCHECKEDNORMAL);
	} else {
		Part = BP_CHECKBOX;
		if (fChecked) {
			State = fDisabled ? CBS_CHECKEDDISABLED : CBS_CHECKEDNORMAL;
		} else if (fIndeterminate) {
			State = fDisabled ? CBS_MIXEDDISABLED : CBS_MIXEDNORMAL;
		} else {
			State = fDisabled ? CBS_UNCHECKEDDISABLED : CBS_UNCHECKEDNORMAL;
		}
	}

	RECT rcClient;
	::GetClientRect(hwnd, &rcClient);
	::FillRect(hdc, &rcClient, m_hFaceBrush);

	SIZE MarkSize = {0, 0};
	::GetThemePartSize(hTheme, hdc, Part, State, nullptr, TS_DRAW, &MarkSize);

	RECT rcMark = rcClient;
	rcMark.top += ((rcMark.bottom - rcMark.top) - MarkSize.cy) / 2;
	rcMark.right = rcMark.left + MarkSize.cx;
	rcMark.bottom = rcMark.top + MarkSize.cy;
	::DrawThemeBackground(hTheme, hdc, Part, State, &rcMark, nullptr);

	TCHAR szText[256] = TEXT("");
	const int TextLength = ::GetWindowText(hwnd, szText, _countof(szText));
	if (TextLength > 0) {
		SIZE MarginSize = {0, 0};
		::GetTextExtentPoint32(hdc, TEXT("0"), 1, &MarginSize);
		RECT rcText = rcClient;
		rcText.left = rcMark.right + MarginSize.cx / 2;

		UINT Format = DT_LEFT;
		if ((Style & BS_MULTILINE) != 0)
			Format |= DT_WORDBREAK;
		else
			Format |= DT_SINGLELINE | DT_VCENTER;
		if ((::SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) != 0)
			Format |= DT_HIDEPREFIX;

		const HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(hwnd, WM_GETFONT, 0, 0));
		const HGDIOBJ hOldFont = hFont ? ::SelectObject(hdc, hFont) : nullptr;
		const int OldBkMode = ::SetBkMode(hdc, TRANSPARENT);
		const COLORREF OldTextColor = ::SetTextColor(hdc, fDisabled ? m_DisabledTextColor : m_TextColor);
		::DrawText(hdc, szText, TextLength, &rcText, Format);
		::SetTextColor(hdc, OldTextColor);
		::SetBkMode(hdc, OldBkMode);
		if (hOldFont)
			::SelectObject(hdc, hOldFont);
	}

	::CloseThemeData(hTheme);
}

bool CDialogDarkModeSupport::SetControlDarkTheme(HWND hwnd, bool fDark)
{
	TCHAR szClassName[64];

	if (::GetClassName(hwnd, szClassName, _countof(szClassName)) < 1)
		return false;

	if (::lstrcmpi(szClassName, TEXT("BUTTON")) == 0) {
		const DWORD Style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
		if ((Style & BS_TYPEMASK) == BS_GROUPBOX) {
			if (fDark)
				::SetWindowSubclass(hwnd, GroupBoxSubclassProc, GROUPBOX_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
			else
				::RemoveWindowSubclass(hwnd, GroupBoxSubclassProc, GROUPBOX_SUBCLASS_ID);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return true;
		}
		if ((Style & BS_TYPEMASK) == BS_RADIOBUTTON || (Style & BS_TYPEMASK) == BS_AUTORADIOBUTTON) {
			if (fDark)
				::SetWindowSubclass(hwnd, ButtonSubclassProc, BUTTON_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
			else
				::RemoveWindowSubclass(hwnd, ButtonSubclassProc, BUTTON_SUBCLASS_ID);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return true;
		}

		return SetWindowDarkTheme(hwnd, fDark);
	}

	if (::lstrcmpi(szClassName, TEXT("COMBOBOX")) == 0) {
		COMBOBOXINFO cbi = {};
		cbi.cbSize = sizeof(cbi);
		if (!::GetComboBoxInfo(hwnd, &cbi))
			return false;

		const bool fDropDown = (::GetWindowLong(hwnd, GWL_STYLE) & 3) == CBS_DROPDOWN;
		DWORD SelStart = 0, SelEnd = 0;
		if (fDropDown)
			::SendMessage(hwnd, CB_GETEDITSEL, reinterpret_cast<WPARAM>(&SelStart), reinterpret_cast<LPARAM>(&SelEnd));

		const bool fOK =
			SUCCEEDED(::SetWindowTheme(hwnd, fDark ? L"DarkMode_CFD" : nullptr, nullptr))
			&& SetWindowDarkTheme(cbi.hwndList, fDark);

		if (fDropDown)
			::SendMessage(hwnd, CB_SETEDITSEL, SelStart, SelEnd);

		return fOK;
	}

	if (::lstrcmpi(szClassName, TEXT("EDIT")) == 0) {
		return SUCCEEDED(::SetWindowTheme(
			hwnd,
			fDark ? (((::GetWindowLong(hwnd, GWL_STYLE) & ES_MULTILINE) == 0) ? L"DarkMode_CFD" : L"DarkMode_Explorer") : nullptr,
			nullptr));
	}

	if (::lstrcmpi(szClassName, TEXT("LISTBOX")) == 0
			|| ::lstrcmpi(szClassName, TEXT("SCROLLBAR")) == 0)
		return SetWindowDarkTheme(hwnd, fDark);

	if (::lstrcmpi(szClassName, TEXT("STATIC")) == 0)
		return true;

	if (::lstrcmpi(szClassName, TRACKBAR_CLASS) == 0) {
		const DWORD Style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_STYLE));
		if (((Style & TBS_TRANSPARENTBKGND) != 0) != fDark)
			::SetWindowLong(hwnd, GWL_STYLE, Style ^ TBS_TRANSPARENTBKGND);
		return true;
	}

	return SetWindowDarkTheme(hwnd, fDark);
}

void CDialogDarkModeSupport::Redraw(HWND hwnd) const
{
	::RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void CDialogDarkModeSupport::FreeBrushes()
{
	if (m_hBackBrush) {
		::DeleteObject(m_hBackBrush);
		m_hBackBrush = nullptr;
	}
	if (m_hFaceBrush) {
		::DeleteObject(m_hFaceBrush);
		m_hFaceBrush = nullptr;
	}
}

BOOL CALLBACK CDialogDarkModeSupport::InitializeControlsProc(HWND hwnd, LPARAM lParam)
{
	CDialogDarkModeSupport *pThis = reinterpret_cast<CDialogDarkModeSupport*>(lParam);
	return pThis->SetControlDarkTheme(hwnd, pThis->m_fDarkMode) ? TRUE : TRUE;
}

BOOL CALLBACK CDialogDarkModeSupport::UpdateControlColorsProc(HWND hwnd, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(lParam);
	return TRUE;
}

LRESULT CALLBACK CDialogDarkModeSupport::GroupBoxSubclassProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	CDialogDarkModeSupport *pThis = reinterpret_cast<CDialogDarkModeSupport*>(dwRefData);

	switch (uMsg) {
	case WM_PAINT:
		if (pThis && pThis->m_fDarkMode && pThis->m_hFaceBrush) {
			PAINTSTRUCT ps;
			HDC hdc = ::BeginPaint(hwnd, &ps);
			pThis->DrawGroupBox(hwnd, hdc);
			::EndPaint(hwnd, &ps);
			return 0;
		}
		break;

	case WM_ERASEBKGND:
		if (pThis && pThis->m_fDarkMode)
			return 1;
		break;

	case WM_NCDESTROY:
		::RemoveWindowSubclass(hwnd, GroupBoxSubclassProc, uIdSubclass);
		break;
	}

	return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CDialogDarkModeSupport::ButtonSubclassProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	CDialogDarkModeSupport *pThis = reinterpret_cast<CDialogDarkModeSupport*>(dwRefData);

	switch (uMsg) {
	case WM_PAINT:
		if (pThis && pThis->m_fDarkMode && pThis->m_hFaceBrush) {
			PAINTSTRUCT ps;
			HDC hdc = ::BeginPaint(hwnd, &ps);
			pThis->DrawButton(hwnd, hdc);
			::EndPaint(hwnd, &ps);
			return 0;
		}
		break;

	case WM_ERASEBKGND:
		if (pThis && pThis->m_fDarkMode)
			return 1;
		break;

	case WM_ENABLE:
	case BM_SETCHECK:
	case WM_SETTEXT:
		{
			LRESULT Result = ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return Result;
		}

	case WM_NCDESTROY:
		::RemoveWindowSubclass(hwnd, ButtonSubclassProc, uIdSubclass);
		break;
	}

	return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
