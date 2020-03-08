/*
 * Copyright (c) 2005 Alexander Gottwald
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 *
 * Authors:	Alexander Gottwald, Colin Harrison
 */
#include "window/util.h"
#include "window/wizard.h"
#include "resources/resources.h"
#include "config.h"
#include <prsht.h>
#include <commctrl.h>
#include <htmlhelp.h>

#include <stdexcept>

#include <X11/Xlib.h>

LPCWSTR FilePath (HWND hwnd, bool PuTTY);
BOOL FileRead (LPCWSTR lpstrFileName);

char *wcconvert(LPCWSTR wstr)
{
    int chars = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (chars == 0)
	throw win32_error("WideCharToMultiByte");
    char *mbstr = new char[chars];
    chars = WideCharToMultiByte(CP_ACP, 0, wstr, -1, mbstr, chars, NULL, NULL);
    if (chars == 0)
	throw win32_error("WideCharToMultiByte");
    return mbstr;
}

wchar_t *mbconvert(const char *mbstr)
{
    int chars = MultiByteToWideChar(CP_ACP, 0, mbstr, -1, NULL, 0);
    if (chars == 0)
	throw win32_error("MultiByteToWideChar");
    wchar_t *wstr = new wchar_t[chars];
    chars = MultiByteToWideChar(CP_ACP, 0, mbstr, -1, wstr, chars);
    if (chars == 0)
	throw win32_error("MultiByteToWideChar");
    return wstr;
}

bool stripspace(wchar_t *s, wchar_t *t)
{
    bool strip = false;
    while (*t == L' ') {
	strip = true;
	*t++;
    }
    while (*t != L'\0') {
	if (*t == L'.' || *t == L'-' || *t == L'_');
	else if (*t == L' ' || !iswalnum(*t)) {
	   strip = true;
	   break;
	}
	*s++ = *t++;
    }
    *s = L'\0';
    return strip;
}

/// @brief Send WM_ENDSESSION to all program windows.
/// This will shutdown the started xserver
BOOL CALLBACK KillWindowsProc(HWND hwnd, LPARAM lParam)
{
    SendMessage(hwnd, WM_ENDSESSION, 0, 0);
    return TRUE;
}

/// @brief Actual wizard implementation.
/// This is based on generic CWizard but handles the special dialogs
class CMyWizard : public CWizard 
{
    public:
    private:
	CConfig config; /// Storage for config options.
	bool passwordMode;
    public:
        /// @brief Constructor.
        /// Set wizard pages.
        CMyWizard() : CWizard() 
        {
    	    passwordMode = false;

            AddPage(IDD_DISPLAY, IDS_DISPLAY_TITLE, IDS_DISPLAY_SUBTITLE);
            AddPage(IDD_CLIENTS, IDS_CLIENTS_TITLE, IDS_CLIENTS_SUBTITLE);
            AddPage(IDD_PROGRAM, IDS_PROGRAM_TITLE, IDS_PROGRAM_SUBTITLE);
            AddPage(IDD_XDMCP, IDS_XDMCP_TITLE, IDS_XDMCP_SUBTITLE);
            AddPage(IDD_CLIPBOARD, IDS_CLIPBOARD_TITLE, IDS_CLIPBOARD_SUBTITLE);
            AddPage(IDD_FINISH, IDS_FINISH_TITLE, IDS_FINISH_SUBTITLE);
        }
        
        virtual void InitPasswordMode()
        {
    	    passwordMode = true;

	    ClearPages();
	    AddPage(IDD_PROGRAM, IDS_PASSWORD_TITLE, IDS_PASSWORD_SUBTITLE);
        }

	virtual void LoadConfig(LPCWSTR filename, bool validate)
	{
	    try {
		config.Load(filename, validate);
	    } catch (std::runtime_error &e)
	    {
		printf("Error: %s\n", e.what());
	    }
	}

	virtual void StripStart()
	{
	    wchar_t *pwchar;
	    wchar_t buffer[256];
	    if (!config.user.empty())
	    {
		pwchar = (wchar_t*)config.user.c_str();
		if (stripspace(buffer, pwchar)) config.user = buffer;
	    }
	    if (!config.host.empty())
	    {
		pwchar = (wchar_t*)config.host.c_str();
		if (stripspace(buffer, pwchar)) config.host = buffer;
	    }
	    if (!config.xdmcp_host.empty())
	    {
		pwchar = (wchar_t*)config.xdmcp_host.c_str();
		if (stripspace(buffer, pwchar)) config.xdmcp_host = buffer;
	    }
	    if (!config.font_server.empty())
	    {
		pwchar = (wchar_t*)config.font_server.c_str();
		if (stripspace(buffer, pwchar)) config.font_server = buffer;
	    }
	}

	virtual BOOL PasswordStart()
	{
	    if (config.password_start && config.password.empty()) return TRUE;
	    return FALSE;
	}

	virtual BOOL PasswordStartPopulated()
	{
	    if (config.client != CConfig::StartProgram || config.clientstart != CConfig::PuTTY ||
		config.program.empty() || config.host.empty()) return FALSE;
	    return TRUE;
	}

	virtual BOOL StartPopulated()
	{
	    if (config.client == CConfig::StartProgram)
	    {
		if (config.clientstart == CConfig::Local)
		{
			if (config.program.empty()) return FALSE;
		}
		else if (config.clientstart == CConfig::PuTTY || config.clientstart == CConfig::OpenSSH)
		{
			if (config.program.empty() || config.host.empty()) return FALSE;
		}
		else return FALSE;
	    }
	    if (config.client == CConfig::XDMCP)
	    {
			if (!config.broadcast && config.xdmcp_host.empty()) return FALSE;
	    }
	    return TRUE;
	}

        /// @brief Handle the PSN_WIZNEXT message.
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise. 
	virtual BOOL WizardNext(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    switch (PageID(index))
	    {
		case IDD_DISPLAY:
                    // Check for select window mode
		    if (IsDlgButtonChecked(hwndDlg, IDC_MULTIWINDOW))
			config.window = CConfig::MultiWindow;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_FULLSCREEN))
			config.window = CConfig::Fullscreen;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_WINDOWED))
			config.window = CConfig::Windowed;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_NODECORATION))
			config.window = CConfig::Nodecoration;
		    else
		    {
			SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
			return TRUE;
		    }
                    // Get selected display number
		    {
			wchar_t buffer[512];
			GetDlgItemTextW(hwndDlg, IDC_DISPLAY, buffer, 512);
			buffer[511] = L'\0';
			config.display = buffer;
                    }
                    // Check for valid input
                    if (config.display.empty())
			SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
                    else
                        SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_CLIENTS);
		    return TRUE;
		case IDD_CLIENTS:
                    // Check for select client startup method
		    if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT))
		    {
			config.client = CConfig::StartProgram;
			SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_PROGRAM);
		    } else if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP))
		    {
			config.client = CConfig::XDMCP;
			SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_XDMCP);
		    } else if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_NONE))
		    {
			config.client = CConfig::NoClient;
			SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_CLIPBOARD);
		    } else
			SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
		    return TRUE;
		case IDD_PROGRAM:
                    // Read program, user and host name
		    {
			bool strip = false;
			wchar_t buffer[512];
			GetDlgItemTextW(hwndDlg, IDC_CLIENT_PASSWORD, buffer, 512);
			config.password = buffer;
			if (config.password.empty())
				config.password_start = false;
			else
				config.password_start = true;
			GetDlgItemTextW(hwndDlg, IDC_CLIENT_USER, buffer, 512);
			buffer[511] = L'\0';
			if (stripspace(buffer, buffer))
			{
				SetDlgItemTextW(hwndDlg, IDC_CLIENT_USER, buffer);
				strip = true;
			}
			config.user = buffer;
			GetDlgItemTextW(hwndDlg, IDC_CLIENT_HOST, buffer, 512);
			buffer[511] = L'\0';
			if (stripspace(buffer, buffer))
			{
				SetDlgItemTextW(hwndDlg, IDC_CLIENT_HOST, buffer);
				strip = true;
			}
			config.host = buffer;
			GetDlgItemTextW(hwndDlg, IDC_CLIENT_PROGRAM, buffer, 512);
			buffer[511] = L'\0';
			config.program = buffer;
			if (strip)
			{
				SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
				return TRUE;
			}
		    }
		    if (IsDlgButtonChecked(hwndDlg, IDC_COMPRESS))
			config.compress = true;
		    else
			config.compress = false;
                    // Check local or remote client
		    if (IsDlgButtonChecked(hwndDlg, IDC_CLIENT_LOCAL))
			config.clientstart = CConfig::Local;
		    else
		    {
			int reads = 0;
			BOOL puttyset = IsDlgButtonChecked(hwndDlg, IDC_CLIENT_REMOTE_PUTTY);
			wchar_t buffer[MAX_PATH];
			for (reads = 0; reads < 2; reads++)
			{
			    buffer[0] = L'\0';
			    if (!config.protocol_path.empty()) wcscpy (buffer, config.protocol_path.c_str());
			    if (puttyset) wcscat(buffer, L"plink.exe");
			    else wcscat(buffer, L"ssh.exe");
			    if (!FileRead(buffer))
			    {
				if (reads < 1) config.protocol_path = FilePath(hwndDlg, puttyset);
				else
				{
					SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
					return TRUE;
				}
			    }
			}
			if (puttyset) config.clientstart = CConfig::PuTTY;
			else config.clientstart = CConfig::OpenSSH;
		    }
                    // Check for valid input
		    if ((config.clientstart != CConfig::Local) && (config.host.empty() || config.program.empty()))
			SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
		    else
			SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_CLIPBOARD);
		    return TRUE;
		case IDD_XDMCP:
                    // Check for broadcast
		    if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_BROADCAST))
			config.broadcast = true;
		    else if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_QUERY))
			config.broadcast = false;
		    else
		    {
			SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
			return TRUE;
		    }
                    // Check for indirect mode
		    if (IsDlgButtonChecked(hwndDlg, IDC_XDMCP_INDIRECT))
			config.indirect = true;
		    else
			config.indirect = false;
                    // Read hostname
		    {
			wchar_t buffer[512];
			GetDlgItemTextW(hwndDlg, IDC_XDMCP_HOST, buffer, 512);
			buffer[511] = L'\0';
			if (stripspace(buffer, buffer))
			{
				SetDlgItemTextW(hwndDlg, IDC_XDMCP_HOST, buffer);
				SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
				return TRUE;
			}
			config.xdmcp_host = buffer;
		    }
                    // Check for valid input
		    if (!config.broadcast && config.xdmcp_host.empty())
			SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
		    else	
			SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_CLIPBOARD);
		    return TRUE;
                case IDD_CLIPBOARD:
                    // check for clipboard
                    if (IsDlgButtonChecked(hwndDlg, IDC_CLIPBOARD))
                        config.clipboard = true;
                    else
                        config.clipboard = false;
		    // check for access control
		    if (IsDlgButtonChecked(hwndDlg, IDC_NO_ACCESS_CONTROL))
			config.no_access_control = true;
		    else
			config.no_access_control = false;
                    // read parameters
		    {
			wchar_t buffer[512];
                        GetDlgItemTextW(hwndDlg, IDC_FONT_SERVER, buffer, 512);
                        buffer[511] = L'\0';
			if (stripspace(buffer, buffer))
			{
				SetDlgItemTextW(hwndDlg, IDC_FONT_SERVER, buffer);
				SetWindowLong(hwndDlg, DWL_MSGRESULT, -1);
				return TRUE;
			}
			config.font_server = buffer;
			GetDlgItemTextW(hwndDlg, IDC_EXTRA_PARAMS, buffer, 512);
			buffer[511] = L'\0';
			config.extra_params = buffer;
			GetDlgItemTextW(hwndDlg, IDC_EXTRA_SSH, buffer, 512);
			buffer[511] = L'\0';
			config.extra_ssh = buffer;
		    }
                    SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_FINISH);
                    return TRUE;
		default:
		    break;
	    }
	    return FALSE;
	}
        /// @brief Handle PSN_WIZFINISH message.
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise. 
	virtual BOOL WizardFinish(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("finish %d\n", index);
#endif
	    if (passwordMode)
		switch (PageID(index))
		{
		    case IDD_PROGRAM:
                	// Read program, user and host name
			{
			    wchar_t buffer[512];
			    GetDlgItemTextW(hwndDlg, IDC_CLIENT_PASSWORD, buffer, 512);
			    buffer[511] = L'\0';
			    config.password = buffer;
			    if (config.password.empty())
				config.password_start = false;
			    else
				config.password_start = true;
			}
			return TRUE;
		    default:
			break;
		}
	    return FALSE;
	}
        /// @brief Handle PSN_WIZBACK message.
        /// Basicly handles switching to proper page (skipping XDMCP or program page
        /// if required).
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise. 
	virtual BOOL WizardBack(HWND hwndDlg, unsigned index)
	{
	    switch (PageID(index))
	    {
		case IDD_PROGRAM:
		case IDD_XDMCP:
		    SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_CLIENTS);
		    return TRUE;
                case IDD_CLIPBOARD:
		    switch (config.client)
		    {
			case CConfig::NoClient:	
			    SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_CLIENTS);
			    return TRUE;
			case CConfig::StartProgram:
			    SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_PROGRAM);
			    return TRUE;
			case CConfig::XDMCP:
			    SetWindowLong(hwndDlg, DWL_MSGRESULT, IDD_XDMCP);
			    return TRUE;
		    }
		    break;
	    }
	    return FALSE;
	}
        /// @brief Handle PSN_SETACTIVE message.
        /// @param hwndDlg Handle to active page dialog.
        /// @param index Index of current page.
        /// @return TRUE if the message was handled. FALSE otherwise. 
	virtual BOOL WizardActivate(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    switch (PageID(index))
	    {
		case IDD_CLIENTS:
                    // Enable or disable XDMCP radiobutton and text
		    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP), config.window != CConfig::MultiWindow);
		    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP_DESC), config.window != CConfig::MultiWindow);
		    break;
	    }
	    return FALSE;
	}
	/// @brief Handle F1 and PSN_HELP message.
	/// @param hwndDlg Handle to active page dialog.
	/// @param index Index of current page.
	virtual void WizardHelp(HWND hwndDlg, unsigned index)
	{
#ifdef _DEBUG
	    printf("%s %d\n", __FUNCTION__, index);
#endif
	    int offset = 0;
	    int idd = PageID(index);
	    if (idd==IDD_DISPLAY) offset = 0;
	    else if (idd==IDD_CLIENTS) offset = 1;
	    else if (idd==IDD_PROGRAM) offset = 2;
	    else if (idd==IDD_XDMCP) offset = 3;
	    else if (idd==IDD_CLIPBOARD) offset = 4;
	    else if (idd==IDD_FINISH) offset = 5;
	    HtmlHelp(hwndDlg, L"XLaunch.chm", HH_HELP_CONTEXT, 500 + offset);
	}

    protected:
        /// @brief Enable or disable the control for remote clients.
        /// @param hwndDlg Handle to active page dialog.
        /// @param state State of control group.
	void EnableRemoteProgramGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_COMPRESS), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_HOST), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_USER), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_HOST_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_USER_DESC), state);
	}
	void EnableRemotePassword(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PASSWORD), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PASSWORD_DESC), state);
	}
	void EnableLocalRemoteGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROGRAM), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_PROGRAM_DESC), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_LOCAL), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_REMOTE_PUTTY), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_CLIENT_REMOTE_SSH), state);
	}
        /// @brief Enable or disable the control for XDMCP connection.
        /// @param hwndDlg Handle to active page dialog.
        /// @param state State of control group.
	void EnableXDMCPQueryGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP_HOST), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_XDMCP_INDIRECT), state);
	}
	/// @brief Enable or disable the control for no access control.
	/// @param hwndDlg Handle to active page dialog.
	/// @param state State of control group.
	void EnableNoAccessControlGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_NO_ACCESS_CONTROL), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_NO_ACCESS_CONTROL_DESC), state);
	}
	/// @brief Enable or disable the control for SSH extras.
	/// @param hwndDlg Handle to active page dialog.
	/// @param state State of control group.
	void EnableSSHExtrasGroup(HWND hwndDlg, BOOL state)
	{
	    EnableWindow(GetDlgItem(hwndDlg, IDC_EXTRA_SSH), state);
	    EnableWindow(GetDlgItem(hwndDlg, IDC_EXTRA_SSH_DESC), state);
	}
        /// @brief Fill program box with default values.
        /// @param hwndDlg Handle to active page dialog.
	void FillProgramBox(HWND hwndDlg)
	{
	    HWND cbwnd = GetDlgItem(hwndDlg, IDC_CLIENT_PROGRAM);
	    if (cbwnd == NULL)
		return;
	    SendMessage(cbwnd, CB_RESETCONTENT, 0, 0);
	    for (int i = 0; i < PROG_NUMBER; i++)
		if (!config.progs[i].empty())
			SendMessageW(cbwnd, CB_ADDSTRING, 0, (LPARAM)config.progs[i].c_str());
	    SendMessage(cbwnd, CB_SETCURSEL, 0, 0);
	}
	LPCWSTR ShowSaveDialog(HWND parent, LPCWSTR filename)
	{
	    wchar_t szTitle[512];
	    wchar_t szFilter[512];
	    wchar_t szFileTitle[512];
	    wchar_t szFile[MAX_PATH];
	    HINSTANCE hInst = GetModuleHandle(NULL);
	    
	    LoadStringW(hInst, IDS_SAVE_TITLE, szTitle, sizeof(szTitle)/sizeof(wchar_t));
	    LoadStringW(hInst, IDS_SAVE_FILETITLE, szFileTitle, sizeof(szFileTitle)/sizeof(wchar_t));
	    LoadStringW(hInst, IDS_SAVE_FILTER, szFilter, sizeof(szFilter)/sizeof(wchar_t));
	    for (unsigned i=0; szFilter[i]; i++) 
		if (szFilter[i] == L'%') 
		    szFilter[i] = L'\0'; 

	    wcscpy(szFile, filename);

	    OPENFILENAMEW ofn;
	    memset(&ofn, 0, sizeof(OPENFILENAMEW));
	    ofn.lStructSize = sizeof(OPENFILENAMEW); 
	    ofn.hwndOwner = parent; 
	    ofn.lpstrFilter = szFilter; 
	    ofn.lpstrFile= szFile; 
	    ofn.nMaxFile = sizeof(szFile)/ sizeof(*szFile); 
	    ofn.lpstrFileTitle = szFileTitle; 
	    ofn.nMaxFileTitle = sizeof(szFileTitle); 
	    ofn.lpstrInitialDir = L'\0'; 
	    ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT; 
	    ofn.lpstrTitle = szTitle;

	    if (GetSaveFileNameW(&ofn))
	    {
		try {
      		    config.Save(ofn.lpstrFile, false);
		} catch (std::runtime_error &e)
		{
		    printf("Error: %s\n", e.what());
		}
		return (ofn.lpstrFile);
	    } 
	    else return (L'\0');
	}
    public:
	   
        /// @brief Handle messages fo the dialog pages.
        /// @param hwndDlg Handle of active dialog.
        /// @param uMsg Message code.
        /// @param wParam Message parameter.
        /// @param lParam Message parameter.
        /// @param psp Handle to sheet paramters. 
        virtual INT_PTR PageDispatch(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, PROPSHEETPAGEW *psp)
        {
            HWND hwnd;
            switch (uMsg)
            {
                case WM_INITDIALOG:
                    switch (PageID(PageIndex(psp)))
                    {
                        case IDD_DISPLAY:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init display dialog. Enable correct check buttons
			    switch (config.window)
			    {
				default:
				case CConfig::MultiWindow:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_MULTIWINDOW);
				    break;
				case  CConfig::Fullscreen:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_FULLSCREEN);
				    break;
				case  CConfig::Windowed:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_WINDOWED);
				    break;
				case  CConfig::Nodecoration:
				    CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, IDC_NODECORATION);
				    break;
			    }
                            // Set display number
                            SetDlgItemTextW(hwndDlg, IDC_DISPLAY, config.display.c_str());
                            break;
                        case IDD_CLIENTS:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init client dialog. Enable correct check buttons
			    switch (config.client)
			    {
				default:
				case CConfig::NoClient:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_NONE, IDC_CLIENT, IDC_CLIENT_NONE);
				    break;
				case CConfig::StartProgram:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_NONE, IDC_CLIENT, IDC_CLIENT);
				    break;
				case CConfig::XDMCP:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_NONE, IDC_CLIENT, IDC_XDMCP);
				    break;
			    }
                            break;
			case IDD_PROGRAM:
			    psp->dwFlags |= PSP_HASHELP;
                           // Init start dialog. Enable correct check buttons
			    switch (config.clientstart)
 			    {
				default:
				case CConfig::Local:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_LOCAL, IDC_CLIENT_REMOTE_SSH, IDC_CLIENT_LOCAL);
				    EnableRemotePassword(hwndDlg, false);
				    EnableRemoteProgramGroup(hwndDlg, false);
				    break;
				case CConfig::PuTTY:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_LOCAL, IDC_CLIENT_REMOTE_SSH, IDC_CLIENT_REMOTE_PUTTY);
				    EnableRemotePassword(hwndDlg, true);
				    EnableRemoteProgramGroup(hwndDlg, true);
				    break;
				case CConfig::OpenSSH:
				    CheckRadioButton(hwndDlg, IDC_CLIENT_LOCAL, IDC_CLIENT_REMOTE_SSH, IDC_CLIENT_REMOTE_SSH);
				    EnableRemotePassword(hwndDlg, false);
				    EnableRemoteProgramGroup(hwndDlg, true);
				    break;
                            }
                            // Fill combo boxes
			    FillProgramBox(hwndDlg);
                            // Set edit fields
			    if (!config.program.empty())
			       	SetDlgItemTextW(hwndDlg, IDC_CLIENT_PROGRAM, config.program.c_str());
			    SetDlgItemTextW(hwndDlg, IDC_CLIENT_HOST, config.host.c_str());
			    SetDlgItemTextW(hwndDlg, IDC_CLIENT_USER, config.user.c_str());
			    SetDlgItemTextW(hwndDlg, IDC_CLIENT_PASSWORD, config.password.c_str());
			    CheckDlgButton(hwndDlg, IDC_COMPRESS, config.compress?BST_CHECKED:BST_UNCHECKED);

			    if (passwordMode)
			    {
				wchar_t szDesc[64];
			        EnableLocalRemoteGroup(hwndDlg, false);
			        EnableRemoteProgramGroup(hwndDlg, false);
			        EnableRemotePassword(hwndDlg, true);

				LoadStringW(GetModuleHandle(NULL), IDS_REENTER_PASS, szDesc, sizeof(szDesc)/sizeof(wchar_t));
			        SetDlgItemTextW(hwndDlg, IDC_CLIENT_PASSWORD_DESC, szDesc);
			    }
			    break;
			case IDD_XDMCP:
			    psp->dwFlags |= PSP_HASHELP;
                            // Init XDMCP dialog. Check broadcast and indirect button
                            CheckRadioButton(hwndDlg, IDC_XDMCP_QUERY, IDC_XDMCP_BROADCAST, config.broadcast?IDC_XDMCP_BROADCAST:IDC_XDMCP_QUERY);
                            CheckDlgButton(hwndDlg, IDC_XDMCP_INDIRECT, config.indirect?BST_CHECKED:BST_UNCHECKED);
			    EnableXDMCPQueryGroup(hwndDlg, config.broadcast?FALSE:TRUE);
                            // Set hostname
			    SetDlgItemTextW(hwndDlg, IDC_XDMCP_HOST, config.xdmcp_host.c_str());
			    break;
                        case IDD_CLIPBOARD:
			    psp->dwFlags |= PSP_HASHELP;
                            CheckDlgButton(hwndDlg, IDC_CLIPBOARD, config.clipboard?BST_CHECKED:BST_UNCHECKED);
			    CheckDlgButton(hwndDlg, IDC_NO_ACCESS_CONTROL, config.no_access_control?BST_CHECKED:BST_UNCHECKED);
                            SetDlgItemTextW(hwndDlg, IDC_FONT_SERVER, config.font_server.c_str());
                            SetDlgItemTextW(hwndDlg, IDC_EXTRA_PARAMS, config.extra_params.c_str());
                            SetDlgItemTextW(hwndDlg, IDC_EXTRA_SSH, config.extra_ssh.c_str());
                            break;
			case IDD_FINISH:
			    psp->dwFlags |= PSP_HASHELP;
			    break;

                    }
		case WM_NOTIFY:
		    LPNMHDR lpnm = (LPNMHDR) lParam;
		    if (lpnm->code == PSN_SETACTIVE)
		    {
			switch (PageID(PageIndex(psp)))
			{
			    case IDD_CLIENTS:
				if (config.window == CConfig::MultiWindow && IsDlgButtonChecked(hwndDlg, IDC_XDMCP))
				{
				    CheckDlgButton(hwndDlg, IDC_XDMCP, BST_UNCHECKED);
				    CheckDlgButton(hwndDlg, IDC_CLIENT_NONE, BST_CHECKED);
				}
				break;
			    case IDD_CLIPBOARD:
				if (config.client == CConfig::XDMCP || config.clientstart == CConfig::PuTTY || config.clientstart == CConfig::OpenSSH)
				{
				    CheckDlgButton(hwndDlg, IDC_NO_ACCESS_CONTROL, BST_UNCHECKED);
				    EnableNoAccessControlGroup(hwndDlg, false);
				}
				else
				    EnableNoAccessControlGroup(hwndDlg, true);
				if (config.clientstart ==  CConfig::PuTTY || config.clientstart ==  CConfig::OpenSSH)
				    EnableSSHExtrasGroup(hwndDlg, true);
				else EnableSSHExtrasGroup(hwndDlg, false);
				break;
			    case IDD_FINISH:
				if (config.clientstart == CConfig::PuTTY && !config.password.empty())
				    EnableWindow(GetDlgItem(hwndDlg, IDC_SAVEPASSWORD), true);
				else
				{
				    CheckDlgButton(hwndDlg, IDC_SAVEPASSWORD, BST_UNCHECKED);
				    EnableWindow(GetDlgItem(hwndDlg, IDC_SAVEPASSWORD), false);
				}
				break;
			}
		    }
		    break;
                case WM_COMMAND:
                    // Handle control messages
                    switch (LOWORD(wParam))
                    {
                        // Handle clicks on images. Check proper radiobutton
                        case IDC_MULTIWINDOW_IMG:
                        case IDC_FULLSCREEN_IMG:
                        case IDC_WINDOWED_IMG:
                        case IDC_NODECORATION_IMG:
                            CheckRadioButton(hwndDlg, IDC_MULTIWINDOW, IDC_NODECORATION, LOWORD(wParam)-4);
                            SetFocus(GetDlgItem(hwndDlg, LOWORD(wParam)-4));
                            break;
                        // Disable unavailable controls 
			case IDC_CLIENT_REMOTE_SSH:
			    EnableRemotePassword(hwndDlg, false);
			    EnableRemoteProgramGroup(hwndDlg, true);
			    break;
                        case IDC_CLIENT_REMOTE_PUTTY:
			    EnableRemotePassword(hwndDlg, true);
			    EnableRemoteProgramGroup(hwndDlg, true);
			    break;
                        case IDC_CLIENT_LOCAL:
			    EnableRemotePassword(hwndDlg, false);
			    EnableRemoteProgramGroup(hwndDlg, false);
                            break;
			case IDC_XDMCP_QUERY:
			case IDC_XDMCP_BROADCAST:
			    EnableXDMCPQueryGroup(hwndDlg, LOWORD(wParam) == IDC_XDMCP_QUERY);
			    break;
			case IDC_FINISH_SAVE:
			    // check for save password
			    if (IsDlgButtonChecked(hwndDlg, IDC_SAVEPASSWORD))
				config.password_save = true;
			    else
				config.password_save = false;
			    {
				wchar_t szCaption[MAX_CAPTION];
				LPCWSTR filename = ShowSaveDialog(hwndDlg, L"config.xlaunch");
				if (!filename) break;
			    	if (!FileRead(filename)) {
				    LoadStringW(GetModuleHandle(NULL), IDS_CANNOT_OPEN, szCaption, sizeof(szCaption)/sizeof(wchar_t));
				    MessageBoxW(hwndDlg, filename, szCaption, MB_OK | MB_ICONEXCLAMATION);
				}
			    }
			    break;
                    }
            }
            // pass messages to parent
            return CWizard::PageDispatch(hwndDlg, uMsg, wParam, lParam, psp);
        }
        
        /// @brief Try to connect to server.
        /// Repeat until successful, server died or maximum number of retries
        /// reached.
        Display *WaitForServer(LPCWSTR display)
        {
            int     ncycles  = 120;         /* # of cycles to wait */
            int     cycles;                 /* Wait cycle count */
            Display *xd;

            for (cycles = 0; cycles < ncycles; cycles++) {
                if ((xd = XOpenDisplay(wcconvert(display)))) {
                    return xd;
                }
                else {
                    Sleep(100L);
                    continue;
                }
            }
            return NULL;
        }
               
        /// @brief Do the actual start of Xming and clients
	void StartUp()
	{
	    std::wstring buffer;
	    std::wstring client;
	    bool msdos = false;

            // Construct display strings
	    std::wstring display_id = L":" + config.display;
	    std::wstring display = L"127.0.0.1" + display_id + L".0";

#ifdef _DEBUG
            // Debug only: Switch to Xming installation directory
	    SetCurrentDirectory("C:\\Programme\\Xming");
#endif	    

            // Build Xming commandline
	    buffer = L"Xming " + display_id + L" ";
	    switch (config.window)
	    {
		case CConfig::MultiWindow:
		    buffer += L"-multiwindow ";
		    break;
		case CConfig::Fullscreen:
		    buffer += L"-fullscreen ";
		    break;
		case CConfig::Nodecoration:
		    buffer += L"-nodecoration ";
		    break;
		default:
		    break;
	    }
            // Add XDMCP parameter
	    if (config.client == CConfig::XDMCP)
	    {
		if (config.broadcast)
		    buffer += L"-broadcast ";
		else 
		{
		    if (config.indirect)
			buffer += L"-indirect ";
		    else
			buffer += L"-query ";
		    buffer += config.xdmcp_host;
            buffer += L" ";
		}
	    }
            if (config.clipboard)
                buffer += L"-clipboard ";
	    if (config.no_access_control)
		buffer += L"-ac ";
            if (!config.font_server.empty())
            {
                buffer += L"-fp tcp/";
                buffer += config.font_server;
                buffer += L":7100 ";
            }
            if (!config.extra_params.empty())
            {
                buffer += config.extra_params;
                buffer += L" ";
            }
            
            // Construct client commandline
	    if (config.client == CConfig::StartProgram)
	    {
		if (config.clientstart != CConfig::Local)
		{
                    std::wstring host = config.host;
                    if (!config.user.empty())
                        host = config.user + L"@" + config.host;
		    if (!config.protocol_path.empty())
			client = config.protocol_path.c_str();
		    if (config.clientstart == CConfig::PuTTY)
		    {
			client += L"plink -ssh -X ";
			if (!config.password.empty())
			{
				client += L"-pw ";
				client += config.password.c_str();
				client += L" ";
			}
		    }
		    else
		    {
			if (!GetEnvironmentVariable("SSH_AGENT_PID",0,0)) msdos = true;
			client += L"ssh -Y ";
		    }
		    if (config.compress)
			client += L"-C ";
		    if (!config.extra_ssh.empty())
		    {
			client += config.extra_ssh;
			client += L" ";
		    }
		    client += host.c_str();
		    client += L" ";
		}
		client += config.program.c_str();
		if (!wcscmp(config.program.c_str(), L"testplink") && config.clientstart == CConfig::PuTTY)
		    MessageBoxW(NULL, client.c_str(), L"XLaunch Test 'plink command line'", MB_OK | MB_ICONINFORMATION);
	    }

            // Prepare program startup
     	    STARTUPINFOW si, sic;
	    PROCESS_INFORMATION pi, pic;
	    HANDLE handles[2];
	    DWORD hcount = 0; 
            Display *dpy = NULL;

	    ZeroMemory( &si, sizeof(si) );
	    si.cb = sizeof(si);
	    ZeroMemory( &pi, sizeof(pi) );
	    ZeroMemory( &sic, sizeof(sic) );
	    sic.cb = sizeof(sic);
	    ZeroMemory( &pic, sizeof(pic) );

	    // Start Xming process. 
#ifdef _DEBUG
	    printf("%s\n", buffer.c_str());
#endif
	    if( !CreateProcessW( NULL, (WCHAR*)buffer.c_str(), NULL, NULL, 
                        FALSE, 0, NULL, NULL, &si, &pi )) 
		throw win32_error("CreateProcess failed");
	    handles[hcount++] = pi.hProcess;

	    if (!client.empty())
	    {
                // Set DISPLAY variable
		SetEnvironmentVariableW(L"DISPLAY",display.c_str());

                // Wait for server to startup
                dpy = WaitForServer(display.c_str());
                if (dpy == NULL)
                {
		    wchar_t szCaption[MAX_CAPTION];
		    LoadStringW(GetModuleHandle(NULL), IDS_OPEN_FAIL, szCaption, sizeof(szCaption)/sizeof(wchar_t));
		    MessageBoxW(NULL, display.c_str(), szCaption, MB_OK | MB_ICONEXCLAMATION);
                    while (hcount--)
                        TerminateProcess(handles[hcount], (DWORD)-1);
                }
                
#ifdef _DEBUG
		printf("%s\n", client.c_str());
#endif

                // Hide a console window, unless msdos is true
		sic.dwFlags = STARTF_USESHOWWINDOW;
		sic.wShowWindow = SW_HIDE;
		if (msdos) sic.wShowWindow = SW_NORMAL;

		// Start the child process. 
		if( !CreateProcessW( NULL, (WCHAR*)client.c_str(), NULL, NULL,
                            FALSE, 0, NULL, NULL, &sic, &pic )) 
		{
		    wchar_t szCaption[MAX_CAPTION];
		    LoadStringW(GetModuleHandle(NULL), IDS_PROGRAM_START_FAIL, szCaption, sizeof(szCaption)/sizeof(wchar_t));
                    MessageBoxW(NULL, client.c_str(), szCaption, MB_OK | MB_ICONEXCLAMATION);
                    while (hcount--)
                        TerminateProcess(handles[hcount], (DWORD)-1);
		}
		handles[hcount++] = pic.hProcess;
	    }

	    // Wait until any child process exits.
	    DWORD ret = WaitForMultipleObjects(hcount, handles, FALSE, INFINITE );

#ifdef _DEBUG
	    printf("killing process!\n");
#endif
            // Check if Xming is still running
	    DWORD exitcode;
	    GetExitCodeProcess(pi.hProcess, &exitcode);
	    unsigned counter = 0;
	    while (exitcode == STILL_ACTIVE)
	    {
		if (++counter > 10)
		    TerminateProcess(pi.hProcess, (DWORD)-1);
		else
		    // Shutdown Xming (the soft way!)
		    EnumThreadWindows(pi.dwThreadId, KillWindowsProc, 0);
		Sleep(500);
		GetExitCodeProcess(pi.hProcess, &exitcode);
	    }
	    // Kill the client
    	    TerminateProcess(pic.hProcess, (DWORD)-1);

	    // Close process and thread handles. 
	    CloseHandle( pi.hProcess );
	    CloseHandle( pi.hThread );
	    CloseHandle( pic.hProcess );
	    CloseHandle( pic.hThread );
	}
};

int main(int argc, char **argv)
{
    try {
        InitCommonControls();
        CMyWizard dialog;
	wchar_t szMessage[MAX_MESSAGE];
	wchar_t szCaption[MAX_CAPTION];

	bool skip_wizard = false;

	if (argc != 1 && argc != 3)
	{
		LoadStringW(GetModuleHandle(NULL), IDS_USAGE, szMessage, sizeof(szMessage)/sizeof(wchar_t));
		LoadStringW(GetModuleHandle(NULL), IDS_WRONG_OPTIONS, szCaption, sizeof(szCaption)/sizeof(wchar_t));
		MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONEXCLAMATION);
		return -1;
	}

	for (int i = 1; i < argc; i++)
	{
	    if (argv[i] == NULL)
		continue;
	    
	    std::string arg(argv[i]);
	    if (arg == "-load" && i + 1 < argc)
	    {
		i++;
		if (!FileRead(mbconvert(argv[i])))
		{
			LoadStringW(GetModuleHandle(NULL), IDS_CANNOT_LOAD, szCaption, sizeof(szCaption)/sizeof(wchar_t));
			MessageBoxW(NULL, mbconvert(argv[i]), szCaption, MB_OK | MB_ICONEXCLAMATION);
			return -1;
		}
		dialog.LoadConfig(mbconvert(argv[i]), false);
		dialog.StripStart();
		continue;
	    }
	    else if (arg == "-validate" && i + 1 < argc)
	    {
		i++;
		if (!FileRead(mbconvert(argv[i])))
		{
			LoadStringW(GetModuleHandle(NULL), IDS_CANNOT_VALIDATE, szCaption, sizeof(szCaption)/sizeof(wchar_t));
			MessageBoxW(NULL, mbconvert(argv[i]), szCaption, MB_OK | MB_ICONEXCLAMATION);
			return -1;
		}
		dialog.LoadConfig(mbconvert(argv[i]), true);
		dialog.StripStart();
		continue;
	    }
	    else if (arg == "-run" && i + 1 < argc)
	    {
		i++;
		if (!FileRead(mbconvert(argv[i])))
		{
			LoadStringW(GetModuleHandle(NULL), IDS_CANNOT_RUN, szCaption, sizeof(szCaption)/sizeof(wchar_t));
			MessageBoxW(NULL, mbconvert(argv[i]), szCaption, MB_OK | MB_ICONEXCLAMATION);
			return -1;
		}
		dialog.LoadConfig(mbconvert(argv[i]), false);

		dialog.StripStart();

		if (dialog.PasswordStart())
		{
		    if (dialog.PasswordStartPopulated())
			dialog.InitPasswordMode();
		    else
			{
				LoadStringW(GetModuleHandle(NULL), IDS_PASS_ENTRY_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
				LoadStringW(GetModuleHandle(NULL), IDS_PASS_ENTRY_CAP, szCaption, sizeof(szCaption)/sizeof(wchar_t));
				MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONINFORMATION);
			}
		}
		else if (!dialog.StartPopulated())
		{
			LoadStringW(GetModuleHandle(NULL), IDS_START_POP_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
			LoadStringW(GetModuleHandle(NULL), IDS_START_POP_CAP, szCaption, sizeof(szCaption)/sizeof(wchar_t));
			MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONINFORMATION);
		}
		else
		{
		    skip_wizard = true;
		}
		continue;
	    }
	    else
	    {
			LoadStringW(GetModuleHandle(NULL), IDS_UNKNOWN_OPTION, szCaption, sizeof(szCaption)/sizeof(wchar_t));
			MessageBoxW(NULL, mbconvert(arg.c_str()), szCaption, MB_OK | MB_ICONEXCLAMATION);
			return -1;
	    }
	}

	int ret = 0; 
        if (skip_wizard || (ret =dialog.ShowModal()) != 0)
	    dialog.StartUp();
#ifdef _DEBUG
	printf("return %d\n", ret);
#endif
	return 0;
    } catch (std::runtime_error &e)
    {
        printf("Error: %s\n", e.what());
        return -1;
    }
}
