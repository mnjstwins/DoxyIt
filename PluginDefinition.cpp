// This file is part of DoxyIt.
// 
// Copyright (C)2013 Justin Dailey <dail8859@yahoo.com>
// 
// DoxyIt is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "Utils.h"
#include "Parsers.h"
#include "Version.h"
#include "SettingsDialog.h"
#include "AboutDialog.h"

// --- Local variables ---
static bool do_active_commenting;	// active commenting - create or extend a document block
//static bool do_active_wrapping;	// active wrapping - wrap text inside of document blocks...todo

static NppData nppData;
static SciFnDirect pSciMsg;			// For direct scintilla call
static sptr_t pSciWndData;			// For direct scintilla call
static SettingsDialog sd;			// The settings dialog
static HANDLE _hModule;				// For dialog initialization

// --- Menu callbacks ---
static void doxyItFunction();
static void doxyItFile();
static void activeCommenting();
//static void activeWrapping();
static void showSettings();
static void showAbout();

// --- Global variables ---
ShortcutKey sk = {true, true, true, 'D'};
FuncItem funcItem[nbFunc] = {
	{TEXT("DoxyIt - Function"), doxyItFunction,   0, false, &sk},
	{TEXT("DoxyIt - File"),     doxyItFile,       0, false, NULL},
	{TEXT(""),                  NULL,             0, false, NULL}, // separator
	{TEXT("Active commenting"), activeCommenting, 0, false, NULL},
	{TEXT(""),                  NULL,             0, false, NULL}, // separator
	{TEXT("Settings..."),       showSettings,     0, false, NULL},
	{TEXT("About..."),          showAbout,        0, false, NULL}
};


LRESULT SendScintilla(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return pSciMsg(pSciWndData, Msg, wParam, lParam);
}

LRESULT SendNpp(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return SendMessage(nppData._nppHandle, Msg, wParam, lParam);
}

bool updateScintilla()
{
	HWND curScintilla;

	// Get the current scintilla
	int which = -1;
	SendNpp(NPPM_GETCURRENTSCINTILLA, SCI_UNUSED, (LPARAM)&which);
	if(which == -1) return false;
	curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

	// Get the function and pointer to it for more efficient calls
	pSciMsg = (SciFnDirect) SendMessage(curScintilla, SCI_GETDIRECTFUNCTION, 0, 0);
	pSciWndData = (sptr_t) SendMessage(curScintilla, SCI_GETDIRECTPOINTER, 0, 0);

	return true;
}

// --- Configuration ---

void getIniFilePath(TCHAR *iniPath, int size)
{
	SendNpp(NPPM_GETPLUGINSCONFIGDIR, size, (LPARAM) iniPath);
	_tcscat_s(iniPath, size, TEXT("\\"));
	_tcscat_s(iniPath, size, NPP_PLUGIN_NAME);
	_tcscat_s(iniPath, size, TEXT(".ini"));
}

void configSave()
{
	TCHAR iniPath[MAX_PATH];
	int len = sizeof(parsers) / sizeof(parsers[0]);

	getIniFilePath(iniPath, MAX_PATH);

	// [DoxyIt]
	WritePrivateProfileString(NPP_PLUGIN_NAME, TEXT("active_commenting"), BOOLTOSTR(do_active_commenting), iniPath);
	WritePrivateProfileString(NPP_PLUGIN_NAME, TEXT("version"), VERSION_LINEAR_TEXT, iniPath);
	WritePrivateProfileString(NPP_PLUGIN_NAME, TEXT("version_stage"), VERSION_STAGE, iniPath);

	for(int i = 0; i < len; ++i)
	{
		const Parser *p = &parsers[i];
		const ParserDefinition *pd = &p->pd;
		std::wstring ws;

		// Wrap everything in quotes to preserve whitespace
		ws = TEXT("\"") + toWideString(pd->doc_start) + TEXT("\"");
		WritePrivateProfileString(p->lang.c_str(), TEXT("doc_start"), ws.c_str(), iniPath);

		ws = TEXT("\"") + toWideString(pd->doc_line) + TEXT("\"");
		WritePrivateProfileString(p->lang.c_str(), TEXT("doc_line_"), ws.c_str(), iniPath);

		ws = TEXT("\"") + toWideString(pd->doc_end) + TEXT("\"");
		WritePrivateProfileString(p->lang.c_str(), TEXT("doc_end__"), ws.c_str(), iniPath);

		ws = TEXT("\"") + toWideString(pd->command_prefix) + TEXT("\"");
		WritePrivateProfileString(p->lang.c_str(), TEXT("command_prefix"), ws.c_str(), iniPath);

		// Encode \r\n as literal "\r\n" in the ini file
		ws = TEXT("\"") + toWideString(stringReplace(std::string(pd->function_format), "\r\n", "\\r\\n")) + TEXT("\"");
		WritePrivateProfileString(p->lang.c_str(), TEXT("function_format"), ws.c_str(), iniPath);

		// Encode \r\n as literal "\r\n" in the ini file
		ws = TEXT("\"") + toWideString(stringReplace(std::string(pd->file_format), "\r\n", "\\r\\n")) + TEXT("\"");
		WritePrivateProfileString(p->lang.c_str(), TEXT("file_format"), ws.c_str(), iniPath);

		WritePrivateProfileString(p->lang.c_str(), TEXT("align"), BOOLTOSTR(pd->align), iniPath);
	}
}

void configLoad()
{
	TCHAR iniPath[MAX_PATH];
	TCHAR tbuffer[512]; // "relatively" large
	std::string buffer;
	int len = sizeof(parsers) / sizeof(parsers[0]);
	//int version;

	getIniFilePath(iniPath, MAX_PATH);

	// [DoxyIt]
	GetPrivateProfileString(NPP_PLUGIN_NAME, TEXT("active_commenting"), TEXT("true"), tbuffer, MAX_PATH, iniPath);
	buffer = toString(tbuffer);
	do_active_commenting = (buffer == "true");

	// NPPM_SETMENUITEMCHECK does not seem to work unless the 
	// menu item is actually clicked, so lets do it manually
	if(do_active_commenting) CheckMenuItem(GetMenu(nppData._nppHandle), funcItem[3]._cmdID, MF_CHECKED);

	// Don't need these for now
	//version = GetPrivateProfileInt(NPP_PLUGIN_NAME, TEXT("version"), 0, iniPath);
	//version_stage = GetPrivateProfileString(NPP_PLUGIN_NAME, TEXT("version_stage"), TEXT(""), tbuffer, MAX_PATH, iniPath);

	for(int i = 0; i < len; ++i)
	{
		Parser *p = &parsers[i];

		// NOTE: We cant use the default value because GetPrivateProfileString strips the whitespace,
		// also, wrapping it in quotes doesn't seem to work either. So...use "!!!" as the default text
		// and if we find that the value wasn't found and we have "!!!" then use the default value in the
		// parser, else, use what we pulled from the file.
		GetPrivateProfileString(p->lang.c_str(), TEXT("doc_start"), TEXT("!!!"), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		if(buffer == "!!!") p->pd.doc_start = p->pd_default.doc_start;
		else p->pd.doc_start = buffer;

		GetPrivateProfileString(p->lang.c_str(), TEXT("doc_line_"), TEXT("!!!"), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		if(buffer == "!!!") p->pd.doc_line = p->pd_default.doc_line;
		else p->pd.doc_line = buffer;

		GetPrivateProfileString(p->lang.c_str(), TEXT("doc_end__"), TEXT("!!!"), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		if(buffer == "!!!") p->pd.doc_end = p->pd_default.doc_end;
		else p->pd.doc_end = buffer;

		GetPrivateProfileString(p->lang.c_str(), TEXT("command_prefix"), TEXT("!!!"), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		if(buffer == "!!!") p->pd.command_prefix = p->pd_default.command_prefix;
		else p->pd.command_prefix = buffer;

		GetPrivateProfileString(p->lang.c_str(), TEXT("function_format"), TEXT("!!!"), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		if(buffer == "!!!") p->pd.function_format = p->pd_default.function_format;
		else p->pd.function_format = stringReplace(buffer, "\\r\\n", "\r\n"); // Un-encode "\r\n" as \r\n

		GetPrivateProfileString(p->lang.c_str(), TEXT("file_format"), TEXT("!!!"), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		if(buffer == "!!!") p->pd.file_format = p->pd_default.file_format;
		else p->pd.file_format = stringReplace(buffer, "\\r\\n", "\r\n"); // Un-encode "\r\n" as \r\n

		GetPrivateProfileString(p->lang.c_str(), TEXT("align"), BOOLTOSTR(p->pd_default.align), tbuffer, 512, iniPath);
		buffer = toString(tbuffer);
		p->pd.align = (buffer == "true");
	}

	// Write out the file if it doesn't exist yet
	//if(!PathFileExists(iniPath)) configSave();
}

void pluginInit(HANDLE hModule)
{
	_hModule = hModule;
}

void pluginCleanUp()
{
}

void setNppInfo(NppData notepadPlusData)
{
	nppData = notepadPlusData;

	sd.init((HINSTANCE) _hModule, nppData);
}


// --- Menu call backs ---

void doxyItFunction()
{
	std::string doc_block;
	int startLine, endLine;
	char *indent = NULL;

	if(!updateScintilla()) return;

	doc_block = Parse();

	// Don't issue any warning messages, let Parse() handle that for us since it knows
	// about the error. Just return if it is a zero length string
	if(doc_block.length() == 0) 
		return;

	// Keep track of where we started
	startLine = SendScintilla(SCI_LINEFROMPOSITION, SendScintilla(SCI_GETCURRENTPOS));

	// Get the whitespace of the next line so we can insert it in front of 
	// all the lines of the document block that is going to be inserted
	indent = getLineIndentStr(startLine + 1);
	
	SendScintilla(SCI_BEGINUNDOACTION);
	SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) doc_block.c_str());
	endLine = SendScintilla(SCI_LINEFROMPOSITION, SendScintilla(SCI_GETCURRENTPOS)); // get the end of the document block
	if(indent) insertBeforeLines(indent, startLine, endLine + 1);
	SendScintilla(SCI_ENDUNDOACTION);

	if(indent) delete[] indent;
}

void doxyItFile()
{
	std::string doc_block;
	const ParserDefinition *pd;

	if(!updateScintilla()) return;

	pd = getCurrentParserDefinition();
	if(!pd)
	{
		::MessageBox(NULL, TEXT("Unrecognized language type."), NPP_PLUGIN_NAME, MB_OK|MB_ICONERROR);
		return;
	}

	doc_block = FormatFileBlock(pd);

	SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) doc_block.c_str());
}

void activeCommenting()
{
	do_active_commenting = !do_active_commenting;
	SendNpp(NPPM_SETMENUITEMCHECK, funcItem[3]._cmdID, (LPARAM) do_active_commenting);
}

void showSettings()
{
	if(!updateScintilla()) return;
	sd.doDialog();
}

void showAbout()
{
	CreateDialog((HINSTANCE) _hModule, MAKEINTRESOURCE(IDD_ABOUTDLG), nppData._nppHandle, abtDlgProc);
}


// --- Notification callbacks ---

void doxyItNewLine()
{
	std::string indentation;
	const ParserDefinition *pd;
	const char *eol;
	char *previousLine, *found = NULL;
	int curLine;

	if(!updateScintilla()) return;

	pd = getCurrentParserDefinition();
	if(!pd) return;

	eol = getEolStr();

	curLine = (int) SendScintilla(SCI_LINEFROMPOSITION, SendScintilla(SCI_GETCURRENTPOS));

	previousLine = getLine(curLine - 1);

	// NOTE: we cannot use getLineIndentStr() because doc_start or doc_line may start with whitespace
	// which we don't want counted towards the indentation string.

	if(found = strstr(previousLine, pd->doc_line.c_str()))
	{
		indentation.append(previousLine, found - previousLine);

		// doc_line should have only whitespace in front of it
		if(isWhiteSpace(indentation))
		{
			SendScintilla(SCI_BEGINUNDOACTION);
			SendScintilla(SCI_DELLINELEFT);	// Clear any automatic indentation
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) indentation.c_str());
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) pd->doc_line.c_str());
			SendScintilla(SCI_ENDUNDOACTION);
			SendScintilla(SCI_CHOOSECARETX);
		}

	}
	// If doc_start is relatively long we do not want the user typing the entire line, just the first 3 should suffice.
	// Also, if doc_end is found, this means a doc block was closed. This allows e.g. /** inline comments */
	else if((found = strstr(previousLine, pd->doc_start.substr(0, 3).c_str())) &&
		strstr(previousLine, pd->doc_end.c_str()) == 0)
	{
		indentation.append(previousLine, found - previousLine);

		if(isWhiteSpace(indentation))
		{
			int pos;
			unsigned int i = 0;

			// Count the characters in common so we can add the rest
			while(i < pd->doc_start.length() && found[i] == pd->doc_start.at(i)) ++i;

			SendScintilla(SCI_BEGINUNDOACTION);
			SendScintilla(SCI_DELLINELEFT);			// Clear any automatic indentation
			SendScintilla(SCI_DELETEBACK);			// Clear the newline
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) &pd->doc_start.c_str()[i]);	// Fill the rest of doc_start
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) eol);
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) indentation.c_str());
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) pd->doc_line.c_str());
			pos = SendScintilla(SCI_GETCURRENTPOS);	// Save this position so we can restore it
			SendScintilla(SCI_LINEEND);				// Skip any text the user carried to next line
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) eol);
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) indentation.c_str());
			SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) pd->doc_end.c_str());
			SendScintilla(SCI_ENDUNDOACTION);
			SendScintilla(SCI_CHOOSECARETX);

			// Restore the position
			SendScintilla(SCI_GOTOPOS, pos);
		}
	}

	delete[] previousLine;
}

void handleNotification(SCNotification *notifyCode)
{
	static bool do_newline = false;
	NotifyHeader nh = notifyCode->nmhdr;
	int ch = notifyCode->ch;

	switch(nh.code)
	{
	case SCN_UPDATEUI: // Now is when we can check to see if we do the commenting
		if(do_newline)
		{
			do_newline = false;
			if(!updateScintilla()) return;
			doxyItNewLine();
		}
		break;
	case SCN_CHARADDED:
		// Set a flag so that all line endings can trigger the commenting
		if((ch == '\r' || ch == '\n') && do_active_commenting) do_newline = true;
		break;
	case NPPN_READY:
		configLoad();
		InitializeParsers();
		break;
	case NPPN_SHUTDOWN:
		configSave();
		CleanUpParsers();
		break;
	case NPPN_BUFFERACTIVATED:
	case NPPN_LANGCHANGED:
		// Don't actually need the parser here, but this forces it to updates the current reference
		getCurrentParser(true);
		break;
	}
	return;
}