////////////////////////////////////////////////////////////////////////////
//	Modified by Axel DominatoR
//	Last updated: 13/08/2015
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UICellItem.h"
#include "uicursor.h"
#include "../inventory_item.h"
#include "../eatable_item.h"
#include "UIDragDropListEx.h"
#include "../xr_level_controller.h"
#include "../../xrEngine/xr_input.h"
#include "../level.h"
#include "object_broker.h"
#include "UIXmlInit.h"
#include "UIProgressBar.h"
#include "../string_table.h"

#include "Weapon.h"
#include "CustomOutfit.h"
#include "ActorHelmet.h"
#include "Magazine.h"
#include "../item_container.h"

CUICellItem* CUICellItem::m_mouse_selected_item = NULL;

CUICellItem::CUICellItem()
{
	m_pParentList		= NULL;
	m_pData				= NULL;
	m_custom_draw		= NULL;
	m_text				= NULL;
//-	m_mark				= NULL;
	m_upgrade			= NULL;
	m_pConditionState	= NULL;
	m_drawn_frame		= 0;
	SetAccelerator		(0);
	m_b_destroy_childs	= true;
	m_selected			= false;
	m_select_armament	= false;
	m_select_equipped	= false;
	m_cur_mark			= false;
	m_has_upgrade		= false;
	
	init();
}

CUICellItem::~CUICellItem()
{
	if (m_b_destroy_childs)
		for (auto child : m_childs)
			child->destroy(true);
	delete_data(m_custom_draw);
}

void CUICellItem::init()
{
	CUIXml	uiXml;
	uiXml.Load( CONFIG_PATH, UI_PATH, "actor_menu_item.xml" );

	m_text								= xr_new<CUIStatic>();
	m_text->SetAutoDelete				(true);
	AttachChild							(m_text);
	CUIXmlInit::InitStatic				(uiXml, "cell_item_text", 0, m_text);
	m_text->Show						(false);

	m_upgrade							= xr_new<CUIStatic>();
	m_upgrade->SetAutoDelete			(true);
	AttachChild							(m_upgrade);
	CUIXmlInit::InitStatic				(uiXml, "cell_item_upgrade", 0, m_upgrade);
	m_upgrade_pos						= m_upgrade->GetWndPos();
	m_upgrade->Show						(false);

	m_pConditionState					= xr_new<CUIProgressBar>();
	m_pConditionState->SetAutoDelete	(true);
	AttachChild							(m_pConditionState);
	CUIXmlInit::InitProgressBar			(uiXml, "cell_item_condition", 0, m_pConditionState);
	m_pConditionState->Show				(false);
	m_pConditionState->ShowBackground	(true);
	m_pConditionState->m_bUseGradient	= true;

	m_fill_bar							= xr_new<CUIProgressBar>();
	m_fill_bar->SetAutoDelete			(true);
	AttachChild							(m_fill_bar);
	CUIXmlInit::InitProgressBar			(uiXml, "cell_item_fill", 0, m_fill_bar);
	m_fill_bar->Show					(false);
	m_fill_bar->ShowBackground			(true);
	m_fill_bar->m_bUseGradient			= true;
}

void CUICellItem::Draw()
{	
	m_drawn_frame						= Device.dwFrame;
	inherited::Draw						();
	if (m_custom_draw) 
		m_custom_draw->OnDraw			(this);
}

void CUICellItem::Update()
{
	EnableHeading(m_pParentList->GetVerticalPlacement());
	if(Heading())
	{
		SetHeading			( -90.f * (PI/180.0f) );
		SetHeadingPivot		(Fvector2().set(0.0f,0.0f), Fvector2().set(GetWidth() * (1.f - m_TextureMargin.top - m_TextureMargin.bottom), 0.f), true);
	}else
		ResetHeadingPivot	();

	inherited::Update();
	
	if ( CursorOverWindow() )
	{
		Frect clientArea;
		m_pParentList->GetClientArea(clientArea);
		Fvector2 cp			= GetUICursor().GetCursorPosition();
		if(clientArea.in(cp))
			GetMessageTarget()->SendMessage(this, DRAG_DROP_ITEM_FOCUSED_UPDATE, NULL);
	}
	
	PIItem item					= (PIItem)m_pData;
	m_has_upgrade				= (item) ? item->has_any_upgrades() : false;
	if (m_has_upgrade)
	{
		Ivector2 itm_grid_size	= GetGridSize();
		if (m_pParentList->GetVerticalPlacement())
			std::swap			(itm_grid_size.x, itm_grid_size.y);
		Ivector2 cell_size		= m_pParentList->CellSize();
		Ivector2 cell_space		= m_pParentList->CellsSpacing();
		float x					= itm_grid_size.x * (cell_size.x + cell_space.x) - m_upgrade->GetWidth() - m_upgrade_pos.x;
		float y					= cell_space.y + m_upgrade_pos.y;
		m_upgrade->SetWndPos	(Fvector2().set(x, y));
	}
	m_upgrade->Show(m_has_upgrade);
}

bool CUICellItem::OnMouseAction(float x, float y, EUIMessages mouse_action)
{
	if ( mouse_action == WINDOW_LBUTTON_DOWN )
	{
		GetMessageTarget()->SendMessage( this, DRAG_DROP_ITEM_LBUTTON_CLICK, NULL );
		GetMessageTarget()->SendMessage( this, DRAG_DROP_ITEM_SELECTED, NULL );
		m_mouse_selected_item = this;
		return false;
	}
	else if ( mouse_action == WINDOW_MOUSE_MOVE )
	{
		if ( pInput->iGetAsyncBtnState(0) && m_mouse_selected_item && m_mouse_selected_item == this )
		{
			GetMessageTarget()->SendMessage( this, DRAG_DROP_ITEM_DRAG, NULL );
			return true;
		}
	}
	else if ( mouse_action == WINDOW_LBUTTON_DB_CLICK )
	{
		GetMessageTarget()->SendMessage( this, DRAG_DROP_ITEM_DB_CLICK, NULL );
		return true;
	}
	else if ( mouse_action == WINDOW_RBUTTON_DOWN )
	{
		GetMessageTarget()->SendMessage( this, DRAG_DROP_ITEM_RBUTTON_CLICK, NULL );
		return true;
	}
	
	m_mouse_selected_item = NULL;
	return false;
};

bool CUICellItem::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
	if (WINDOW_KEY_PRESSED == keyboard_action)
	{
		if (GetAccelerator() == dik)
		{
			GetMessageTarget()->SendMessage(this, DRAG_DROP_ITEM_DB_CLICK, NULL);
			return		true;
		}
	}
	return inherited::OnKeyboardAction(dik, keyboard_action);
}

CUIDragItem* CUICellItem::CreateDragItem()
{
	CUIDragItem* tmp;
	tmp = xr_new<CUIDragItem>(this);
	Frect r;
	GetAbsoluteRect(r);

	r.left += GetWidth() * (Heading() ? m_TextureMargin.bottom : m_TextureMargin.left);
	r.top += GetHeight() * (Heading() ? m_TextureMargin.left : m_TextureMargin.top);
	r.right -= GetWidth() * (Heading() ? m_TextureMargin.top : m_TextureMargin.right);
	r.bottom -= GetHeight() * (Heading() ? m_TextureMargin.right : m_TextureMargin.bottom);

	if( m_UIStaticItem.GetFixedLTWhileHeading() )
	{
		float t1,t2;
		t1				= r.width();
		t2				= r.height()*UI().get_current_kx();

		Fvector2 cp = GetUICursor().GetCursorPosition();

		r.x1			= (cp.x-t2/2.0f);
		r.y1			= (cp.y-t1/2.0f);
		r.x2			= r.x1 + t2;
		r.y2			= r.y1 + t1;
	}
	tmp->Init(GetShader(), r, GetUIStaticItem().GetTextureRect());
	return tmp;
}

void CUICellItem::SetOwnerList(CUIDragDropListEx* p)	
{
	m_pParentList = p;
}

void CUICellItem::UpdateConditionProgressBar()
{
	m_pConditionState->Show				(false);
	m_fill_bar->Show					(false);
	if (!m_pParentList || !m_pParentList->GetConditionProgBarVisibility())
		return;

	PIItem item							= static_cast<PIItem>(m_pData);
	if (!item)
		return;

	float fill							= item->getFillBar();
	float condition						= item->GetCondition();
	bool show_fill_bar					= (fill != -1.f);
	bool show_condition_bar				= !fEqual(condition, 1.f);
	if (!show_fill_bar && !show_condition_bar)
		return;

	Ivector2 itm_grid_size				= GetGridSize();
	if (m_pParentList->GetVerticalPlacement())
		_STD swap						(itm_grid_size.x, itm_grid_size.y);
	Ivector2 cell_size					= m_pParentList->CellSize();
	Ivector2 cell_space					= m_pParentList->CellsSpacing();

	if (show_fill_bar)
	{
		float indent					= m_pConditionState->GetHeight() + 2.f;
		m_fill_bar->SetX				(1.f);
		m_fill_bar->SetY				(indent);

		float height					= itm_grid_size.y * (cell_size.y + cell_space.y) - 2.f * indent;
		m_fill_bar->SetHeight			(height);
		m_fill_bar->m_UIProgressItem.SetHeight(height);
		m_fill_bar->m_UIBackgroundItem.SetHeight(height);

		m_fill_bar->SetProgressPos		(fill);
		m_fill_bar->Show				(true);
	}

	if (show_condition_bar)
	{
		float indent					= m_fill_bar->GetWidth() + 2.f;
		m_pConditionState->SetX			(indent);
		m_pConditionState->SetY			(itm_grid_size.y * (cell_size.y + cell_space.y) - m_pConditionState->GetHeight() - 1.f);

		float width						= itm_grid_size.x * (cell_size.x + cell_space.x) - 2.f * indent;
		m_pConditionState->SetWidth		(width);
		m_pConditionState->m_UIProgressItem.SetWidth(width);
		m_pConditionState->m_UIBackgroundItem.SetWidth(width);

		m_pConditionState->SetProgressPos(condition);
		m_pConditionState->Show			(true);
	}
}

bool CUICellItem::EqualTo(CUICellItem* itm)
{
	return (m_grid_size.x==itm->GetGridSize().x) && (m_grid_size.y==itm->GetGridSize().y);
}

u32 CUICellItem::ChildsCount()
{
	return m_childs.size();
}

void CUICellItem::PushChild(CUICellItem* c)
{
	R_ASSERT(c->ChildsCount()==0);
	VERIFY				(this!=c);
	m_childs.push_back	(c);
	UpdateItemText		();
}

CUICellItem* CUICellItem::PopChild(CUICellItem* needed)
{
	CUICellItem* itm	= m_childs.back();
	m_childs.pop_back	();
	
	if (!needed)
		needed			= this;
	if (itm != needed)
		static_cast<PIItem>(itm->m_pData)->swapIcon(static_cast<PIItem>(needed->m_pData));

	UpdateItemText		();
	R_ASSERT			(itm->ChildsCount()==0);
	itm->SetOwnerList	(NULL);
	return				itm;
}

bool CUICellItem::HasChild(CUICellItem* item)
{
	return (m_childs.end() != std::find(m_childs.begin(), m_childs.end(), item) );
}

void CUICellItem::UpdateItemText()
{
	if (ChildsCount())
	{
		string32							str;
		xr_sprintf							(str, "x%d", ChildsCount() + 1);
		m_text->TextItemControl()->SetText	(str);
		m_text->AdjustWidthToText			();
		m_text->AdjustHeightToText			();
		m_text->Show						(true);
	}
	else
		m_text->Show						(false);
}

void CUICellItem::Mark( bool status )
{
	m_cur_mark = status;
}

void CUICellItem::SetCustomDraw(ICustomDrawCellItem* c)
{
	if (m_custom_draw)
		xr_delete(m_custom_draw);
	m_custom_draw = c;
}

bool CUICellItem::destroy(bool force)
{
	if (!m_pData || force)
	{
		void* _real_ptr = dynamic_cast<void*>(this);
		this->~CUICellItem();
		Memory.mem_free(_real_ptr);
		return true;
	}
	return false;
}

// -------------------------------------------------------------------------------------------------

CUIDragItem::CUIDragItem(CUICellItem* parent)
{
	m_custom_draw					= NULL;
	m_back_list						= NULL;
	m_pParent						= parent;
	AttachChild						(&m_static);
	Device.seqRender.Add			(this, REG_PRIORITY_LOW-5000);
	Device.seqFrame.Add				(this, REG_PRIORITY_LOW-5000);
	VERIFY							(m_pParent->GetMessageTarget());
}

CUIDragItem::~CUIDragItem()
{
	Device.seqRender.Remove			(this);
	Device.seqFrame.Remove			(this);
	delete_data						(m_custom_draw);
}

void CUIDragItem::SetCustomDraw(ICustomDrawDragItem* c)
{
	if (m_custom_draw)
		xr_delete(m_custom_draw);
	m_custom_draw = c;
}

void CUIDragItem::Init(const ui_shader& sh, const Frect& rect, const Frect& text_rect)
{
	SetWndRect						(rect);
	m_static.SetShader				(sh);
	m_static.SetTextureRect			(text_rect);
	m_static.SetWndPos				(Fvector2().set(0.0f,0.0f));
	m_static.SetWndSize				(GetWndSize());
	m_static.TextureOn				();
	m_static.SetTextureColor		(color_rgba(255,255,255,170));
	m_static.SetStretchTexture		(true);
	m_pos_offset.sub				(rect.lt, GetUICursor().GetCursorPosition());
}

bool CUIDragItem::OnMouseAction(float x, float y, EUIMessages mouse_action)
{
	if(mouse_action == WINDOW_LBUTTON_UP)
	{
		m_pParent->GetMessageTarget()->SendMessage(m_pParent,DRAG_DROP_ITEM_DROP,NULL);
		return true;
	}
	return false;
}

void CUIDragItem::OnRender()
{
	Draw			();
}

void CUIDragItem::OnFrame()
{
	Update			();
}

void CUIDragItem::Draw()
{
	Fvector2 tmp;
	tmp.sub					(GetWndPos(), GetUICursor().GetCursorPosition());
	tmp.sub					(m_pos_offset);
	tmp.mul					(-1.0f);
	MoveWndDelta			(tmp);
	inherited::Draw			();
	if(m_custom_draw) 
		m_custom_draw->OnDraw(this);
}

void CUIDragItem::SetBackList(CUIDragDropListEx* l)
{
	if(m_back_list)
		m_back_list->OnDragEvent(this, false);

	m_back_list					= l;

	if(m_back_list)
		l->OnDragEvent			(this, true);
}

Fvector2 CUIDragItem::GetPosition()
{
	return Fvector2().add(m_pos_offset, GetUICursor().GetCursorPosition());
}
