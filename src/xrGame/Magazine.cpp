#include "stdafx.h"
#include "Magazine.h"
#include "Level.h"
#include "player_hud.h"
#include "../Include/xrRender/Kinematics.h"
#include "WeaponAmmo.h"
#include "Weapon.h"
#include "addon_owner.h"
#include "addon.h"

MMagazine::MMagazine(CGameObject* obj, shared_str CR$ section) :
	CModule(obj),
	m_capacity(pSettings->r_u32(section, "capacity")),
	m_bullets_visible(!!pSettings->r_BOOL(section, "bullets_visible")),
	m_attach_anm(pSettings->r_string(section, "attach_anm")),
	m_detach_anm(pSettings->r_string(section, "detach_anm")),
	m_ammo_slot_type(pSettings->r_string(section, "ammo_slot_type"))
{
	m_vis								= m_bullets_visible;
	update_bullets_visibility			();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MMagazine::sOnChild(CGameObject* obj, bool take)
{
	CWeaponAmmo* heap					= obj->scast<CWeaponAmmo*>();
	if (heap && heap->m_mag_pos != u8_max)
	{
		int sign						= (take) ? 1 : -1;
		m_amount						+= sign * heap->GetAmmoCount();
		m_weight						+= sign * heap->Weight();
		register_heap					(heap, take);
	}
}

void MMagazine::sUpdateHudBonesVisibility()
{
	update_hud_bullets_visibility		();
}

float MMagazine::sSumItemData(EItemDataTypes type)
{
	switch (type)
	{
	case eWeight:
		return							m_weight;
	default:
		return							0.f;
	}
}

xoptional<float> MMagazine::sGetBar()
{
	float fill							= static_cast<float>(m_amount) / static_cast<float>(m_capacity);
	return								(fill < 1.f) ? fill : -1.f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

shared_str bullets_str					= "bullets";
void MMagazine::update_bullets_visibility()
{
	if (!m_bullets_visible)
		return;

	bool vis							= !Empty();
	if (vis != m_vis)
	{
		m_vis							= vis;
		O.UpdateBoneVisibility			(bullets_str, vis);
		I->SetInvIconType				(static_cast<u8>(vis));
		update_hud_bullets_visibility	();
	}
}

void MMagazine::update_hud_bullets_visibility()
{
	if (auto hi = O.scast<CHudItem*>()->HudItemData())
		hi->set_bone_visible			(bullets_str, (BOOL)!Empty(), TRUE);
}

void MMagazine::register_heap(CWeaponAmmo* heap, bool insert)
{
	if (insert)
	{
		if (heap->m_mag_pos == u8_max)
			heap->m_mag_pos				= m_heaps.size();
		if (m_heaps.size() <= heap->m_mag_pos)
			m_heaps.resize				(heap->m_mag_pos + 1, nullptr);
		m_heaps[heap->m_mag_pos]		= heap;
	}
	else
		m_heaps.erase_data				(heap);
	update_bullets_visibility			();
}

bool MMagazine::canTake(CWeaponAmmo CPC ammo) const
{
	if (Full())
		return							false;

	auto addon							= ammo->getModule<MAddon>();
	return								CAddonSlot::isCompatible(m_ammo_slot_type, addon->SlotType());
}

void MMagazine::loadCartridge(CCartridge CR$ cartridge, int count)
{
	m_amount							+= count;
	R_ASSERT							(m_amount <= m_capacity);
	m_weight							+= count * cartridge.weight;

	CWeaponAmmo* back_heap				= (m_heaps.empty()) ? nullptr : m_heaps.back();
	if (back_heap && back_heap->cNameSect() == cartridge.m_ammoSect && fEqual(back_heap->GetCondition(), cartridge.m_fCondition))
	{
		int d_count						= min(count, back_heap->m_boxSize - back_heap->GetAmmoCount());
		back_heap->ChangeAmmoCount		(d_count);
		count							-= d_count;
	}
	if (count)
	{
		auto vec						= O.giveItems(cartridge.m_ammoSect.c_str(), count, cartridge.m_fCondition, true);
		for (auto heap : vec)
		{
			auto obj					= Level().Objects.net_Find(heap->ID);
			register_heap				(obj->scast<CWeaponAmmo*>(), true);
		}
	}
}

void MMagazine::loadCartridge(CWeaponAmmo* ammo)
{
	CCartridge							cartridge;
	ammo->Get							(cartridge);
	loadCartridge						(cartridge);
}

CWeaponAmmo* MMagazine::get_ammo() const
{
	return								(Empty()) ? nullptr : m_heaps.back();
}

CWeaponAmmo const* MMagazine::getAmmo() const
{
	return								get_ammo();
}

xoptional<CCartridge> MMagazine::getCartridge(bool expend)
{
	CCartridge							res;
	auto ammo							= get_ammo();
	if (!ammo || !ammo->Get(res, expend))
		return							{};

	if (expend)
	{
		--m_amount;
		m_weight						-= res.weight;
	}

	return								res;
}

void MMagazine::setCondition(float val, bool recursive)
{
	if (recursive)
		for (auto h : m_heaps)
			h->SetCondition				(val, true);
}
