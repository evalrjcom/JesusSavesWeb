/*
* Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
*
* This file is part of NetSurf, http://www.netsurf-browser.org/
*
* NetSurf is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* NetSurf is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * \file
 * This is The win32 API about dialog implementation.
 */

#include <stdio.h>
#include <string.h>

#include "utils/config.h"

#include <windows.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "desktop/version.h"

#include "windows/gui.h"
#include "windows/window.h"
#include "windows/about.h"
#include "windows/resourceid.h"

#include "windbg.h"

/**
 * Set a control's text from a UTF-8 string (JESUSSAVES-v1.1.1).
 */
static void about_set_text_utf8(HWND item, const char *utf8)
{
	WCHAR wide[256];

	if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, 256) > 0) {
		SetWindowTextW(item, wide);
	} else {
		SetWindowTextA(item, utf8);
	}
}

/**
 * Initialize the about dialog text fields
 */
static BOOL init_about_dialog(HWND hwnd)
{
	char ver_str[128];
	HWND dlg_itm;
	HFONT hFont;

	/* JESUSSAVES-v1.1.1: version_str is "<ver> (<details>)": the heading shows
	 * "JesusSaves <ver>", the details (base, author, build date) go in the text field.
	 * Text is converted from UTF-8 so the (c) sign displays correctly. */
	const char *details = strchr(netsurf_version, ' ');
	int verlen = details ? (int)(details - netsurf_version) : (int)strlen(netsurf_version);

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_VERSION);
	if (dlg_itm != NULL) {

		hFont=CreateFont (26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
		if (hFont != NULL) {
			NSLOG(netsurf, INFO, "Setting font object");
			SendMessage(dlg_itm, WM_SETFONT, (WPARAM)hFont, 0);
		}

		snprintf(ver_str, sizeof(ver_str), "%s %.*s",
			 messages_get("NetSurf"), verlen, netsurf_version);

		about_set_text_utf8(dlg_itm, ver_str);
	}

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_TEXT);
	if (dlg_itm != NULL && details != NULL) {
		size_t dl;
		snprintf(ver_str, sizeof(ver_str), "%s", details + 1);
		dl = strlen(ver_str);
		if (ver_str[0] == '(' && dl > 1 && ver_str[dl - 1] == ')') {
			ver_str[dl - 1] = 0;
			memmove(ver_str, ver_str + 1, dl - 1);
		}
		about_set_text_utf8(dlg_itm, ver_str);
	}

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_COPYRIGHT);
	if (dlg_itm != NULL) {
		about_set_text_utf8(dlg_itm, messages_get("NetSurfCopyright"));
	}

	return TRUE;
}

/**
 * destroy resources used to create about dialog
 */
static BOOL destroy_about_dialog(HWND hwnd)
{
	HWND dlg_itm;
	HFONT hFont;

	dlg_itm = GetDlgItem(hwnd, IDC_ABOUT_VERSION);
	if (dlg_itm != NULL) {
		hFont = (HFONT)SendMessage(dlg_itm, WM_GETFONT, 0, 0);
		if (hFont != NULL) {
			NSLOG(netsurf, INFO, "Destroyed font object");
			DeleteObject(hFont); 	
		}
	}
		
	return TRUE;

}

static BOOL CALLBACK 
nsws_about_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	switch(msg) {
	case WM_INITDIALOG: 
		return init_about_dialog(hwnd);

	case WM_COMMAND:
		switch(LOWORD(wparam)) {
		case IDOK:
			NSLOG(netsurf, INFO, "OK clicked");
			EndDialog(hwnd, IDOK);
			break;

		case IDCANCEL:
			NSLOG(netsurf, INFO, "Cancel clicked");
			EndDialog(hwnd, IDOK);
			break;

		case IDC_BTN_CREDITS: 
			nsws_window_go(hwnd, "about:credits");
			EndDialog(hwnd, IDOK);
			break;

		case IDC_BTN_CHANGELOG: /* JESUSSAVES-v1.1.1 */
			nsws_window_go(hwnd, "about:changelog");
			EndDialog(hwnd, IDOK);
			break;

		case IDC_BTN_LICENCE:
			nsws_window_go(hwnd, "about:licence");
			EndDialog(hwnd, IDOK);
			break;

		}
		break;

	case WM_CREATE:
		return TRUE;

	case WM_DESTROY:
		return destroy_about_dialog(hwnd);

	}
	return FALSE;
}

void nsw32_about_dialog_init(HINSTANCE hinst, HWND parent)
{
	int ret = DialogBox(hinst, MAKEINTRESOURCE(IDD_ABOUT), parent,
			nsws_about_event_callback);
	if (ret == -1) {
		win32_warning(messages_get("NoMemory"), 0);
		return;
	}
}
