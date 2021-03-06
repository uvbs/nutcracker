/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	
*********************************************************************/

#include "NutcrackerPCH.h"
#include "WatchesWnd.h"
#include "Workspace.h"

namespace nutcracker
{

BEGIN_EVENT_TABLE(WatchesWnd, BaseWatchesWnd)
	EVT_SHOW(WatchesWnd::OnShow)
	EVT_SET_FOCUS(WatchesWnd::OnFocus)
	EVT_ENTER_WINDOW(WatchesWnd::OnEnterWindow)
	EVT_LEAVE_WINDOW(WatchesWnd::OnLeaveWindow)
END_EVENT_TABLE()

WatchesWnd::WatchesWnd(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: BaseWatchesWnd(true, parent,id,pos,size,style)
{
}

WatchesWnd::~WatchesWnd()
{
}

int WatchesWnd::getSelectedWatchID(wxRect* rectDst, int* colDst)
{
	auto currItem = m_tree->GetSelection();
	if (currItem.IsOk())
	{
		if (rectDst)
		{
			auto mousePos = wxGetMousePosition();
			mousePos -= m_tree->GetScreenPosition();
			auto data = m_tree->GetDataView();
			*rectDst = data->GetItemRect(data->GetCurrentItem());

			int w = 0;
			for (int c = 0; c < static_cast<int>(data->GetColumnCount()); c++)
			{
				auto col = data->GetColumnAt(c);
				if (mousePos.x >= w && mousePos.x < w + col->GetWidth())
				{
					rectDst->x = w;
					rectDst->width = col->GetWidth();

					// Transform from the tree control coordinates to our window coordinates
					rectDst->SetPosition(
						this->ScreenToClient(data->GetMainWindow()->ClientToScreen(rectDst->GetPosition())));

					if (colDst)
						*colDst = c;
					break;
				}
				w += col->GetWidth();
			}
		}

		return m_treeData.findByWxTreeListItem(currItem)->getItemId().id[0];
	}
	return 0;
}

void WatchesWnd::updateState()
{
	// Remember what item we have selected
	// Note: We only get the selected item if we didn't specify explicitly the one we want selected
	if (m_selectedWatchID == 0)
		m_selectedWatchID = getSelectedWatchID();

	m_treeData.clear();
	m_treeData.startRefresh();

	int idcounter = 0;
	auto root = m_treeData.setupRoot(TreeCtrlUtil::TreeItemID(idcounter++));

	std::shared_ptr<TreeCtrlUtil::TreeListCtrlItemData> selectedEntry;
	gWorkspace->iterateWatches([&](const Watch* watch, const WatchValue* value)
	{
		auto entry = addEntry(root, static_cast<const TableEntry&>(*value), idcounter, watch->id);
		// We pick any ">=" so that when we delete a watch the cursor select the next one
		if (!selectedEntry && watch->id>=m_selectedWatchID)
			selectedEntry = entry;
	});

	// Add an empty line at the end to allow adding new watches
	auto emptyRow = m_treeData.addItem(root, TreeCtrlUtil::TreeItemID(idcounter++), "");
	if (!selectedEntry)
		selectedEntry = emptyRow;

	m_tree->Select(selectedEntry->getWxItem());

	adjustSize(this->GetSize().GetX());
	m_treeData.endRefresh();
	m_selectedWatchID = 0;
}

void WatchesWnd::adjustSize(int width)
{
	/*
	int col1Size = m_grid->GetColSize(0);
	int remaining = width - col1Size;
	m_grid->SetColSize(1,std::max(m_infoColMinSize, remaining));
	m_grid->SetMargins(0 - wxSYS_VSCROLL_X, 0);
	*/
}


void WatchesWnd::OnShow(wxShowEvent& evt)
{
	if (evt.IsShown())
		updateState();
	evt.Skip();
}

void WatchesWnd::OnItemActivated(wxTreeListEvent& event)
{
	wxTreeListItem item = event.GetItem();
	if (!item.IsOk()) return;

	wxRect rect;
	int col;
	int id = getSelectedWatchID(&rect, &col);
	m_selectedWatchID = id;

	if (col != 0)
		return;

	CZ_ASSERT(m_txtCtrl == nullptr);
	m_txtCtrl = new wxTextCtrl(this, BaseWatchesWnd::ID_TXTCTRL, wxT(""), rect.GetPosition(), rect.GetSize());
	m_txtCtrl->Connect(BaseWatchesWnd::ID_TXTCTRL, wxEVT_CHAR_HOOK, wxKeyEventHandler(WatchesWnd::OnTextChar), NULL, this);
	m_txtCtrl->Connect(BaseWatchesWnd::ID_TXTCTRL, wxEVT_KILL_FOCUS, wxFocusEventHandler(WatchesWnd::OnTextKillFocus), NULL, this);
	m_txtCtrl->Connect(BaseWatchesWnd::ID_TXTCTRL, wxEVT_LEAVE_WINDOW, wxMouseEventHandler(WatchesWnd::OnTextLeaveWindow), nullptr, this);
	m_txtCtrl->Connect(BaseWatchesWnd::ID_TXTCTRL, wxEVT_ENTER_WINDOW, wxMouseEventHandler(WatchesWnd::OnTextEnterWindow), nullptr, this);

	// This is necessary, otherwise there will be a bunch of problems with Mouse Events. The wxTreeListCtrl seems to steal
	// events from the text box. This is probably because even though the text box in on top visually, it's the same leve
	// regarding event handling?
	m_tree->Lower();
	m_txtCtrl->Raise();
	
	m_txtCtrl->SetFocus();

	const Watch* watch = (id != 0) ? gWorkspace->getWatchByID(id) : nullptr;
	if (watch !=0)
	{
		m_txtCtrl->SetValue(watch->name);
	}
}

void WatchesWnd::OnTextChar(wxKeyEvent& event)
{
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		m_tree->SetFocus();
		return;
	}
	else if (event.GetKeyCode() == WXK_RETURN)
	{
		wxString str = m_txtCtrl->GetValue().Trim();
		if (str != wxT(""))
			m_selectedWatchID = gWorkspace->addWatch(str.c_str().AsChar(), m_selectedWatchID);
		m_tree->SetFocus();
		return;
	}

	event.Skip();
}

void WatchesWnd::OnCharHook(wxKeyEvent& event)
{
	if (m_txtCtrl)
	{
		event.Skip();
		return;
	}

	if (event.GetKeyCode() == WXK_DELETE)
	{
		auto current = m_tree->GetSelection();
		if (!current.IsOk())
			return;
		auto data = m_treeData.findByWxTreeListItem(current);
		m_selectedWatchID = getSelectedWatchID();
		gWorkspace->removeWatchByID(data->getItemId().id[0]);
	}
	else
	{
		event.Skip();
	}
}

void WatchesWnd::OnFocus(wxFocusEvent& event)
{
	//czDebug(ID_Log, "%s", __FUNCTION__);
	event.Skip();
}

void WatchesWnd::OnEnterWindow(wxMouseEvent& event)
{
	//czDebug(ID_Log, "%s", __FUNCTION__);
	event.Skip();
}

void WatchesWnd::OnLeaveWindow(wxMouseEvent& event)
{
	//czDebug(ID_Log, "%s", __FUNCTION__);
	event.Skip();
}

void WatchesWnd::OnTextKillFocus(wxFocusEvent& event)
{
	//czDebug(ID_Log, "%s", __FUNCTION__);
	delete m_txtCtrl;
	m_txtCtrl = nullptr;
}

void WatchesWnd::OnTextEnterWindow(wxMouseEvent& event)
{
	//czDebug(ID_Log, "%s", __FUNCTION__);
	event.Skip();
}

void WatchesWnd::OnTextLeaveWindow(wxMouseEvent& event)
{
	//czDebug(ID_Log, "%s", __FUNCTION__);
	event.Skip();
}

void WatchesWnd::OnSize(wxSizeEvent& evt)
{
	auto size = evt.GetSize();
	adjustSize(size.GetX());
	evt.Skip();
}

} // namespace nutcracker






