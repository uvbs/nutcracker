/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	
*********************************************************************/

#include "NutcrackerPCH.h"
#include "UIDefs.h"
#include "FileEditorWnd.h"
#include "FileEditorGroupWnd.h"
#include "FileEditorStyles.h"
#include "Workspace.h"
#include "BreakpointsWnd.h"

#include "art/xpm/breakpoint_on.xpm"
#include "art/xpm/breakpoint_off.xpm"
#include "art/xpm/breakpoint_invalid.xpm"
#include "art/xpm/debugcursor.xpm"

namespace nutcracker
{

enum
{
	MARGIN_LINE_NUMBERS=0,
	MARGIN_BREAKPOINTS,
	MARGIN_FOLD,
	ID_TextCtrl
};

// Markers
enum
{
	MARK_BREAKPOINT_ON=0,
	MARK_BREAKPOINT_OFF,
	MARK_BREAKPOINT_INVALID,
	MARK_DEBUGCURSOR
};

BEGIN_EVENT_TABLE(FileEditorWnd, FileEditorWnd_Auto)
	EVT_STC_CHARADDED(ID_FileEditorTextCtrl, FileEditorWnd::OnCharacterAdded)
	EVT_STC_UPDATEUI(ID_FileEditorTextCtrl, FileEditorWnd::OnUpdateUI)
	EVT_STC_DOUBLECLICK(ID_FileEditorTextCtrl, FileEditorWnd::OnDoubleClick)
	EVT_STC_MODIFIED(ID_FileEditorTextCtrl, FileEditorWnd::OnTextChanged)
	EVT_STC_SAVEPOINTREACHED(ID_FileEditorTextCtrl, FileEditorWnd::OnTextSavePoint)
	EVT_CHAR_HOOK(FileEditorWnd::OnCharHook)
	EVT_TIMER(wxID_ANY, FileEditorWnd::OnTimer)
END_EVENT_TABLE()


FileEditorWnd::FileEditorWnd(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: FileEditorWnd_Auto(parent,id,pos,size,style)
{
	m_textCtrl->SetSizeHints(wxSize(640, 350));

}

FileEditorWnd::~FileEditorWnd()
{
	if (auto file = getFile())
	{
		file->dirty = false;
	}

	gWorkspace->breakpoints.iterate(m_file, [&](Breakpoint& brk)
	{
		brk.markerHandle = -1;
		brk.line = brk.savedline;
	});

	if (gBreakpointsWnd->IsShownOnScreen())
		gBreakpointsWnd->updateState();
}

std::shared_ptr<File> FileEditorWnd::getFile()
{
	return m_file;
}

void FileEditorWnd::setCommonStyle()
{
	m_textCtrl->SetMarginWidth(MARGIN_LINE_NUMBERS, 50);
	m_textCtrl->SetMarginType(MARGIN_LINE_NUMBERS, wxSTC_MARGIN_NUMBER);

	m_textCtrl->SetMarginWidth(MARGIN_BREAKPOINTS, 16);
	m_textCtrl->SetMarginType(MARGIN_BREAKPOINTS, wxSTC_MARGIN_SYMBOL);
	m_textCtrl->SetMarginSensitive(MARGIN_BREAKPOINTS, true);

	m_textCtrl->SetWrapMode(wxSTC_WRAP_NONE);

	m_textCtrl->MarkerDefineBitmap(MARK_BREAKPOINT_ON, wxImage(breakpoint_on_xpm));
	m_textCtrl->MarkerDefineBitmap(MARK_BREAKPOINT_OFF, wxImage(breakpoint_off_xpm));
	m_textCtrl->MarkerDefineBitmap(MARK_BREAKPOINT_INVALID, wxImage(breakpoint_invalid_xpm));
	m_textCtrl->MarkerDefineBitmap(MARK_DEBUGCURSOR, wxImage(debugcursor_xpm));

	// ---- Enable code folding
	m_textCtrl->SetMarginWidth(MARGIN_FOLD, 15);
	m_textCtrl->SetMarginType(MARGIN_FOLD, wxSTC_MARGIN_SYMBOL);
	m_textCtrl->SetMarginMask(MARGIN_FOLD, wxSTC_MASK_FOLDERS);
	m_textCtrl->SetMarginSensitive(MARGIN_FOLD, true);
	m_textCtrl->SetFoldMarginColour(true, wxColour(220, 220, 220));
	m_textCtrl->SetFoldMarginHiColour(true, clDefaultBkg);
	//wxColour foldMarginColour(clDefaultBkg);
	//m_textCtrl->StyleSetBackground(MARGIN_FOLD, foldMarginColour);

	// Properties found from http://www.scintilla.org/SciTEDoc.html
	m_textCtrl->SetProperty(wxT("fold"), wxT("1"));
	m_textCtrl->SetProperty(wxT("fold.comment"), wxT("1"));
	m_textCtrl->SetProperty(wxT("fold.compact"), wxT("1"));
	m_textCtrl->SetProperty(wxT("fold.preprocessor"), wxT("1"));
	// Disable automatic detection of inactive code due to #if #else #endif,
	// since lots of defines will be outside the file
	m_textCtrl->SetProperty("lexer.cpp.track.preprocessor", "0");

	wxColour grey(100, 100, 100);
	wxColour fcolour(240, 240, 240);
	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDER, fcolour);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDER, grey);

	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDEROPEN, fcolour);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDEROPEN, grey);

	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDERSUB, grey);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDERSUB, grey);

	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDEREND, fcolour);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDEREND, grey);

	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDEROPENMID, fcolour);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDEROPENMID, grey);

	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDERMIDTAIL, grey);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDERMIDTAIL, grey);

	m_textCtrl->MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER);
	m_textCtrl->MarkerSetForeground(wxSTC_MARKNUM_FOLDERTAIL, grey);
	m_textCtrl->MarkerSetBackground(wxSTC_MARKNUM_FOLDERTAIL, grey);
	// ---- End of code folding part

	// Indentation settings
	auto tabWidth = m_textCtrl->GetTabWidth();
	m_textCtrl->SetUseTabs(true);
	m_textCtrl->SetIndent(4);
	m_textCtrl->SetTabWidth(4);
	m_textCtrl->SetTabIndents(true);
	m_textCtrl->SetBackSpaceUnIndents(true);

	m_textCtrl->SetWhitespaceForeground(true, gWhiteSpaceColour);

	// Set indicator styles
	for (int i = 0; i < (int)gIndicatorStyles.size(); i++)
	{
		m_textCtrl->IndicatorSetStyle(i, gIndicatorStyles[i].style);
		m_textCtrl->IndicatorSetForeground(i, gIndicatorStyles[i].colour);
		m_textCtrl->IndicatorSetAlpha(i, gIndicatorStyles[i].alpha);
	}

	m_textCtrl->SetCaretLineBackground(clCurrentLineBkg);
	m_textCtrl->SetCaretLineVisible(true);
}

void FileEditorWnd::setFile(const std::shared_ptr<File>& file, int line, int col, bool columnIsOffset)
{
	if (m_file != file)
	{
		m_file = file;
		m_file->dirty = false;

		// Find the language we want
		UTF8String ext = file->extension;
		LanguageInfo* lang = nullptr;
		for (auto& l : gLanguages)
		{
			if (cz::find(l.fileextensions, ext) != l.fileextensions.end())
			{
				lang = &l;
				break;
			}
		}

		m_textCtrl->Freeze();

		// Set the default shared style before StyleClearAll, because as specified in scintilla documentation,
		// StyleClearAll will reset all other styles to wxSTC_STYLE_DEFAULT
		setStyle(m_textCtrl, gSharedStyles[0]);
		m_textCtrl->StyleClearAll();
		if (lang)
			m_textCtrl->SetLexer(lang->lexer);
		setCommonStyle();
		setStyles(m_textCtrl, gSharedStyles);
		// Set styles specific to the language
		if (lang)
			setStyles(m_textCtrl, lang->styles);

		updateViewOptions();
	
		m_textCtrl->SetReadOnly(false);
		m_textCtrl->LoadFile(file->fullpath.c_str(), wxTEXT_TYPE_ANY);
		m_textCtrl->Connect(wxEVT_STC_MARGINCLICK, wxStyledTextEventHandler(FileEditorWnd::OnMarginClick), NULL, this);
		m_textCtrl->Thaw();
		m_textCtrl->SetSavePoint();
		m_textCtrl->EmptyUndoBuffer();

		gWorkspace->breakpoints.iterate(m_file, [&](Breakpoint& brk)
		{
			syncBreakpoint(brk);
		});

		updateErrorMarkers();
		updateMarkers();

		m_textCtrl->Fit();
		this->Layout();
	}

	if (line >= 0 && col >= 0)
	{
		if (columnIsOffset)
		{
			// Column is a real offset into the text (doesn't take into account tab width)
			int pos = m_textCtrl->PositionFromLine(line);
			m_textCtrl->GotoPos(pos + col);
		} else{
			// Column is a visual offset into the text (takes into account tab width)
			m_textCtrl->GotoPos(m_textCtrl->FindColumn(line, col));
		}
	}

	m_textCtrl->VerticalCentreCaret();
}

void FileEditorWnd::checkReload()
{
	auto file = getFile();
	FileTime t = FileTime::get(file->fullpath, FileTime::kModified);
	if (t<= file->filetime)
		return;

	m_file.reset();
	setFile(file, -1, 0);
}

void FileEditorWnd::syncBreakpoint(Breakpoint& brk)
{
	auto deleteAll = [&]()
	{
		m_textCtrl->MarkerDelete(brk.line, MARK_BREAKPOINT_INVALID);
		m_textCtrl->MarkerDelete(brk.line, MARK_BREAKPOINT_ON);
		m_textCtrl->MarkerDelete(brk.line, MARK_BREAKPOINT_OFF);
	};

	if (brk.markerHandle == -1)
	{
		CZ_ASSERT(brk.line != -1);
		deleteAll();
		brk.markerHandle = m_textCtrl->MarkerAdd(brk.line, brk.enabled ? MARK_BREAKPOINT_ON : MARK_BREAKPOINT_OFF);
	}
	else
	{
		int line = m_textCtrl->MarkerLineFromHandle(brk.markerHandle);
		brk.line = line;
		int markers = m_textCtrl->MarkerGet(line);
		if (brk.enabled && (markers&(1 << MARK_BREAKPOINT_OFF)))
		{
			deleteAll();
			brk.markerHandle = m_textCtrl->MarkerAdd(brk.line, MARK_BREAKPOINT_ON);
		}
		else if (!brk.enabled && (markers&(1 << MARK_BREAKPOINT_ON)))
		{
			deleteAll();
			brk.markerHandle = m_textCtrl->MarkerAdd(brk.line, MARK_BREAKPOINT_OFF);
		}
	}
}

void FileEditorWnd::syncBreakInfo(BreakInfo& brk)
{
	if (brk.file != m_file)
		return;

	m_textCtrl->MarkerDeleteAll(MARK_DEBUGCURSOR);
	m_textCtrl->MarkerAdd(brk.line - 1, MARK_DEBUGCURSOR);
	setFile(m_file, brk.line - 1, 0);
}


void FileEditorWnd::OnMarginClick(wxStyledTextEvent& event)
{
	switch(event.GetMargin())
	{
		case MARGIN_BREAKPOINTS:
		{
			int linenumber = m_textCtrl->LineFromPosition(event.GetPosition());
			int markers = m_textCtrl->MarkerGet(linenumber);

			if (markers&((1<<MARK_BREAKPOINT_ON)|(1<<MARK_BREAKPOINT_INVALID)))
			{
				gWorkspace->breakpoints.remove(m_file, linenumber);
				m_textCtrl->MarkerDelete(linenumber, MARK_BREAKPOINT_ON);
			}
			else if (markers&(1 << MARK_BREAKPOINT_OFF))
			{
				auto b = gWorkspace->breakpoints.get(m_file, linenumber);
				CZ_ASSERT(b && b->enabled==false);
				b->enabled = true;
				b->markerHandle = -1;
				syncBreakpoint(*b);
			}
			else
			{
				auto handle =m_textCtrl->MarkerAdd(linenumber, MARK_BREAKPOINT_ON);
				gWorkspace->breakpoints.add(m_file, linenumber, handle);
			}
			
			gBreakpointsWnd->updateState();
			//updateMarkers();
		}
		break;
		case MARGIN_FOLD:
		{
			int lineClick = m_textCtrl->LineFromPosition(event.GetPosition());
			int levelClick = m_textCtrl->GetFoldLevel(lineClick);

			if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0)
			{
				m_textCtrl->ToggleFold(lineClick);
			}
		}
		break;
	}
}

void FileEditorWnd::updateMarkers()
{
	/*
	m_textCtrl->MarkerDeleteAll(MARK_BREAKPOINT_ON);
	m_textCtrl->MarkerDeleteAll(MARK_BREAKPOINT_OFF);
	m_textCtrl->MarkerDeleteAll(MARK_BREAKPOINT_INVALID);
	m_textCtrl->MarkerDeleteAll(MARK_DEBUGCURSOR);

	// TODO

	auto& debugpos = gSession->getLastSourcePos(m_file->isCSource());
	if (debugpos.first == m_file)
		m_textCtrl->MarkerAdd(debugpos.second-1, MARK_DEBUGCURSOR);
	*/
}

void FileEditorWnd::updateErrorMarkers()
{
	m_textCtrl->SetIndicatorCurrent(kINDIC_Warning);
	m_textCtrl->IndicatorClearRange(0, m_textCtrl->GetLength());
	m_textCtrl->SetIndicatorCurrent(kINDIC_Error);
	m_textCtrl->IndicatorClearRange(0, m_textCtrl->GetLength());

	auto file = getFile();

	for (auto& e : file->errorLines)
	{
		int line = e.line;
		m_textCtrl->SetIndicatorCurrent( e.isError ? kINDIC_Error : kINDIC_Warning);
		/*
		int length = m_textCtrl->GetLineLength(line);
		int startpos = m_textCtrl->GetLineEndPosition(line)-length;
		*/
		int startpos = m_textCtrl->PositionFromLine(line);
		int length = m_textCtrl->GetLineLength(line);
		m_textCtrl->IndicatorFillRange(startpos, length);
	}
}

void FileEditorWnd::OnTextChanged(wxStyledTextEvent& event)
{
	int flags = event.GetModificationType();
	if ((flags&wxSTC_MOD_INSERTTEXT) == wxSTC_MOD_INSERTTEXT ||
		(flags&wxSTC_MOD_DELETETEXT) == wxSTC_MOD_DELETETEXT)
	{
		auto file = getFile();
		file->dirty = true;
		gFileEditorGroupWnd->setPageTitle(m_file);

		gWorkspace->breakpoints.iterate(m_file, [&](Breakpoint& brk)
		{
			if (brk.markerHandle != -1)
				brk.line = m_textCtrl->MarkerLineFromHandle(brk.markerHandle);
		});

		if (gBreakpointsWnd->IsShownOnScreen())
			gBreakpointsWnd->updateState();
	}
}

void FileEditorWnd::OnTextSavePoint(wxStyledTextEvent& event)
{
	m_file->dirty = false;
	gFileEditorGroupWnd->setPageTitle(m_file);
}

void FileEditorWnd::OnUpdateUI(wxStyledTextEvent& event)
{
	//
	// Process brace matching
	// Based on http://permalink.gmane.org/gmane.comp.lib.scintilla.net/146
	//
	{
		auto isbrace = [](char ch)
		{
			return ch == ']' || ch == '[' || ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == '<' || ch == '>';
		};

		int brace_position = m_textCtrl->GetCurrentPos() - 1;
		char ch = (char)m_textCtrl->GetCharAt(brace_position);
		if (isbrace(ch))
		{
			int has_match = m_textCtrl->BraceMatch(brace_position);
			if (has_match > -1)
				m_textCtrl->BraceHighlight(has_match, brace_position);
			else
				m_textCtrl->BraceBadLight(brace_position);
		}
		else
		{
			brace_position = m_textCtrl->GetCurrentPos();
			char ch = (char)m_textCtrl->GetCharAt(brace_position);
			if (isbrace(ch))
			{
				int has_match = m_textCtrl->BraceMatch(brace_position);
				if (has_match > -1)
					m_textCtrl->BraceHighlight(has_match, brace_position);
				else
					m_textCtrl->BraceBadLight(brace_position);
			}
			else
			{
				m_textCtrl->BraceHighlight(-1, -1);
			}
		}
	}

	//
	// Disable word matching if we don't have a selection
	//
	if (!m_textCtrl->GetSelectedText().Length())
	{
		m_textCtrl->SetIndicatorCurrent(kINDIC_WordMatch);
		m_textCtrl->IndicatorClearRange(0, m_textCtrl->GetLength());
	}
}

void FileEditorWnd::OnDoubleClick(wxStyledTextEvent& event)
{
	int flags = wxSTC_FIND_WHOLEWORD|wxSTC_FIND_MATCHCASE;
	wxString txt = m_textCtrl->GetSelectedText().Trim();

	// The check for empty is required, as it seems if I double click on an empty line so that the selected text is empty, the loop
	// below will never finished.
	// The check for empty after trimming is not required, but there is no point in using it
	if (txt.size()==0)
		return;

	int pos=m_textCtrl->FindText(0, m_textCtrl->GetLength(), txt, flags );
	while(pos!=-1)
	{
		m_textCtrl->SetIndicatorCurrent(kINDIC_WordMatch);
		m_textCtrl->IndicatorFillRange(pos, txt.Length());
		pos=m_textCtrl->FindText(pos+1, m_textCtrl->GetLength(), txt, flags );
	}
}

void FileEditorWnd::onPageChanged()
{
	updateErrorMarkers();
	updateMarkers();
}

int FileEditorWnd::findCurrentWordStart(int pos, std::string& token)
{
	int col; // Column not taking into account the tab width
	m_textCtrl->GetCurLine(&col);
	int line = m_textCtrl->LineFromPosition(pos);

	auto isValidSymbolChar = [](char ch) {
		return ch == '_' || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
	};

	int start = pos;
	while (isValidSymbolChar(m_textCtrl->GetCharAt(start - 1)))
	{
		start--;
	}

	const char* data = m_textCtrl->GetCharacterPointer();
	token.append(data + start, data + pos);
	return (col - (token.size()))+1; // Scintilla lines/rows are 0 based, so add 1
}

void FileEditorWnd::showAutoComplete()
{
}

void FileEditorWnd::showCallTip(bool updatePos)
{
}

void FileEditorWnd::OnCharacterAdded(wxStyledTextEvent& event)
{
	char chr = (char)event.GetKey();
	int currentLine = m_textCtrl->GetCurrentLine();
	if (chr== '\n')
	{
		int lineInd = 0;
		if (currentLine>0)
			lineInd = m_textCtrl->GetLineIndentation(currentLine-1);
		if (lineInd==0)
			return;

		m_textCtrl->SetLineIndentation(currentLine, lineInd);
		m_textCtrl->LineEnd();
		return;
	}

	char previousChr = m_textCtrl->GetCharAt(m_textCtrl->GetCurrentPos() - 2 );
	if (chr == '.' || (chr == '>' && previousChr == '-'))
		showAutoComplete();
	else if (chr == '(')
		showCallTip(true);
	else if (chr == ')')
	{
		if (m_textCtrl->CallTipActive())
			m_textCtrl->CallTipCancel();
	}
	else if (m_textCtrl->CallTipActive())
		showCallTip(false);
}

void FileEditorWnd::save()
{
	auto file = getFile();
	if (!file)
		return;

	try {
		m_textCtrl->SaveFile(file->fullpath.widen());
		file->dirty = false;
		file->filetime = FileTime::get(file->fullpath, FileTime::kModified);
		fireAppEvent(AppEventFileSaved(this));

		gWorkspace->breakpoints.iterate(m_file, [&](Breakpoint& brk)
		{
			brk.savedline = brk.line;
		});
	}
	catch(std::exception& e)
	{
		showException(e);
	}
}

void FileEditorWnd::getPositionForParser(int& line, int& column, bool adjustForSymbol)
{
	// The control gives as a zero-based line number. libclang expects 1.. based
	int currentChar = m_textCtrl->GetCharAt(m_textCtrl->GetCurrentPos());
	line = m_textCtrl->GetCurrentLine() + 1;
	column = 0;
	// This gets us the true offset in the line, without taking into account the tab width, which is what libclang
	// expects
	m_textCtrl->GetCurLine(&column);

	// It seems if we place the cursor at the first character, libclang considers that position to belong to the
	// previous cursor, and not what what word represents, so we advance the column if the character to the right
	// is a valid start of a C identifier
	if (adjustForSymbol)
	{
		if ((currentChar >= 'A' && currentChar <= 'Z') || (currentChar >= 'a' && currentChar <= 'z') || currentChar == '_')
			column++;
	}
}

void FileEditorWnd::OnCharHook(wxKeyEvent& event)
{
	int modifiers = event.GetModifiers();
	auto keycode = event.GetKeyCode();

	if (modifiers == wxMOD_CONTROL && keycode == 'S') // Save
	{
		save();
	}
	else if (modifiers == wxMOD_CONTROL && keycode == 'R')
	{
		m_textCtrl->DiscardEdits();
		m_textCtrl->LoadFile(m_file->fullpath.widen());
	}
	/*
	else if (modifiers == wxMOD_CONTROL && keycode == WXK_F7) // Compile current file
	{
		try
		{
			buildutil::compileFile(BuildVariables(), m_file, wxStringToUtf8(gMainFrame->getActiveConfigurationName()));
			fireCustomAppEvent(CAE_BuildFinished);
		}
		catch (std::exception& e)
		{
			showException(e);
		}

		this->SetFocus();
	}
	else if (modifiers == wxMOD_CONTROL && keycode == '-') // Previous cursor position
	{
		gFileEditorGroupWnd->cursorHistory_Previous();
	}
	else if (modifiers == (wxMOD_CONTROL | wxMOD_SHIFT) && keycode == '-') // Next cursor position
	{
		gFileEditorGroupWnd->cursorHistory_Next();
	}
	else if (modifiers == wxMOD_CONTROL && keycode == ' ') // Explicitly shows auto complete
	{
		showAutoComplete();
	}
	else if (modifiers == (wxMOD_CONTROL | wxMOD_SHIFT) && keycode == ' ') // Explicitly show the calltip for function call
	{
		showCallTip(true);
	}
	else if (modifiers == wxMOD_ALT && keycode == 'O') // switch between C and H files
	{
		auto ext = m_file->getExtension();
		auto basename = Filename(m_file->name).getBaseFilename();
		UTF8String othername;
		if (ext == "c")
			othername = basename + ".h";
		else if (ext == "h")
			othername = basename + ".c";

		if (othername != "")
		{
			auto f = cz::apcpuide::gWorkspace->findFile(othername);
			if (f)
				gFileEditorGroupWnd->gotoFile(f, -1, -1);
		}
	}
	else if (modifiers == wxMOD_ALT && keycode == 'M') // Search symbols in current file
	{
		FindSymbolDlg dlg(this, m_file->getFullPath().c_str(), FINDSYMBOL_FUNCTIONS);
		dlg.setDelays(250, 250);
		dlg.setFilter("");
		dlg.ShowModal();
		UTF8String file; int line; int col;
		if (dlg.getResult(file, line, col))
			gFileEditorGroupWnd->gotoFile(file, line, col);
	}
	else if (modifiers == (wxMOD_CONTROL | wxMOD_SHIFT) && keycode == 'G') // Open header file under cursor (in #include statement)
	{
		int line, column;
		getPositionForParser(line, column);
		if (column == 0)
			column++;
		auto filename =
			gWorkspace->getCParser().findHeaderAtPos(m_file->getFullPath().c_str(), line, column);
		if (filename != "")
		{
			gFileEditorGroupWnd->cursorHistory_AddHistory(gFileEditorGroupWnd->getCurrentLocation());
			gFileEditorGroupWnd->gotoFile(UTF8String(filename.c_str()));
		}
	}
	else if (modifiers == (wxMOD_CONTROL | wxMOD_ALT) && (keycode == WXK_F11 || keycode == WXK_F12)) // Goto declaration
	{
		int line, column;
		getPositionForParser(line, column);
		cparser::Location2 loc =
			gWorkspace->getCParser().findDeclaration(m_file->getFullPath().c_str(), line, column);
		if (loc.isValid())
		{
			gFileEditorGroupWnd->cursorHistory_AddHistory(gFileEditorGroupWnd->getCurrentLocation());
			gFileEditorGroupWnd->gotoFile(UTF8String(loc.filename.c_str()), loc.line, loc.column - 1, true);
		}
	}
	else if (modifiers == 0 && (keycode == WXK_F11 || keycode == WXK_F12)) // Goto definition
	{
		int line, column;
		getPositionForParser(line, column);
		cparser::Location2 loc =
			gWorkspace->getCParser().findDefinition(m_file->getFullPath().c_str(), line, column);
		if (loc.isValid())
		{
			gFileEditorGroupWnd->cursorHistory_AddHistory(gFileEditorGroupWnd->getCurrentLocation());
			gFileEditorGroupWnd->gotoFile(UTF8String(loc.filename.c_str()), loc.line, loc.column - 1, true);
		}
	}
	*/
	else // Skip this event (nothing we have to do)
	{
		event.Skip();
	}
}

std::pair<int, int> FileEditorWnd::getCursorLocation()
{
	return std::make_pair(m_textCtrl->GetCurrentLine()+1, m_textCtrl->GetColumn(m_textCtrl->GetCurrentPos()));
}

void FileEditorWnd::setFocusToEditor()
{
	m_textCtrl->SetFocus();
}

bool FileEditorWnd::editorHasFocus()
{
	return m_textCtrl->HasFocus();
}

void FileEditorWnd::updateViewOptions()
{
	m_textCtrl->SetIndentationGuides( gUIState->view_ShowIndentationGuides ? wxSTC_IV_LOOKBOTH : wxSTC_IV_NONE);
	m_textCtrl->SetViewWhiteSpace( gUIState->view_Whitespace ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);
	m_textCtrl->SetViewEOL( gUIState->view_EOL ? true : false);
	m_textCtrl->SetEdgeMode(wxSTC_EDGE_LINE);
	m_textCtrl->SetEdgeColumn(80);
	m_textCtrl->SetEdgeColour(wxColour(210,210,210));
}

void FileEditorWnd::onAppEvent(const AppEvent& evt)
{
	switch (evt.id)
	{
		case AppEventID::ViewOptionsChanged:
			updateViewOptions();
			break;
		case AppEventID::DebugStop:
			m_textCtrl->MarkerDeleteAll(MARK_DEBUGCURSOR);
			break;
		default:
			break;
	};
}

void FileEditorWnd::recolourise(bool reparse)
{
	/*
	m_textCtrl->PrivateLexerCall((int)cparser::LexerCall::Enable, (void*)(reparse ? 1 : 0));
	m_textCtrl->Colourise(0, m_textCtrl->GetLength());
	m_textCtrl->PrivateLexerCall((int)cparser::LexerCall::Disable, nullptr);
	*/
}

void FileEditorWnd::OnTimer(wxTimerEvent& evt)
{
	if (getFile()->extension=="nut")
	{
		// TODO
		/*
		void *res = m_textCtrl->PrivateLexerCall((int)cparser::LexerCall::GetNeedsUpdate, nullptr);
		if (res != (void*)(0))
		{
			recolourise(true);
		}
		*/
	}
}

wxString FileEditorWnd::getWordUnderCursor()
{
	return m_textCtrl->GetTextRange(m_textCtrl->WordStartPosition(m_textCtrl->GetCurrentPos(), true),
									m_textCtrl->WordEndPosition(m_textCtrl->GetCurrentPos(), true));
}

bool FileEditorWnd::markerToLine(int markerHandle, int& line)
{
	int l = m_textCtrl->MarkerLineFromHandle(markerHandle);
	if (l == 1)
		return false;
	line = l;
	return true;
}

} // namespace nutcracker

