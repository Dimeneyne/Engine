#include "stdafx.h"
#include "WeaponStatMgun.h"
#include "level.h"
#include "entity_alive.h"
#include "hudsound.h"
#include "actor.h"
#include "actorEffector.h"
#include "EffectorShot.h"
#include "Weapon.h"

const Fvector&	CWeaponStatMgun::get_CurrentFirePoint()
{
	return m_fire_pos;
}

const Fmatrix&	CWeaponStatMgun::get_ParticlesXFORM	()						
{
	return m_fire_bone_xform;
}

void CWeaponStatMgun::FireStart()
{
	if (m_firing_disabled)
		return;
	
	m_dAngle.set(0.0f,0.0f);
	inheritedShooting::FireStart();
}

void CWeaponStatMgun::FireEnd()	
{
	m_dAngle.set(0.0f,0.0f);
	inheritedShooting::FireEnd();
	StopFlameParticles	();
	RemoveShotEffector ();
}

void CWeaponStatMgun::UpdateFire()
{
	fShotTimeCounter -= time_delta();

	inheritedShooting::UpdateFlameParticles();
	inheritedShooting::UpdateLight();
	
	if (m_overheat_enabled)
	{
		m_overheat_value -= m_overheat_decr_quant;
		if (m_overheat_value < 100.f)
		{
			if (p_overheat)
			{
				if (p_overheat->IsPlaying())
					p_overheat->Stop(FALSE);
				CParticlesObject::Destroy(p_overheat);
			}
			if (m_firing_disabled)
				m_firing_disabled = false;
		}
		else {
			if (p_overheat)
			{
				Fmatrix	pos;
				pos.set(get_ParticlesXFORM());
				pos.c.set(get_CurrentFirePoint());
				p_overheat->SetXFORM(pos);
			}
		}
	}

	if(!IsWorking()){
		clamp(fShotTimeCounter,0.0f, flt_max);
		clamp(m_overheat_value, 0.0f, m_overheat_threshold);
		return;
	}

	if (m_overheat_enabled)
	{
		m_overheat_value += m_overheat_time_quant;
		clamp(m_overheat_value, 0.0f, m_overheat_threshold);

		if (m_overheat_value >= 100.f)
		{
			if (!p_overheat)
			{
				p_overheat = CParticlesObject::Create(m_overheat_particles.c_str(),FALSE);
				Fmatrix	pos;
				pos.set(get_ParticlesXFORM());
				pos.c.set(get_CurrentFirePoint());
				p_overheat->SetXFORM(pos);
				p_overheat->Play(false);
			}

			if (m_overheat_value >= m_overheat_threshold)
			{
				m_firing_disabled = true;
				FireEnd();
				return;
			}
		}
	}
	
	if(fShotTimeCounter<=0)
	{
		OnShot			();
		fShotTimeCounter		+= fOneShotTime;
	}else
	{
		angle_lerp		(m_dAngle.x,0.f,5.f,time_delta());
		angle_lerp		(m_dAngle.y,0.f,5.f,time_delta());
	}
}


void CWeaponStatMgun::OnShot()
{
	VERIFY(Owner());

	FireBullet				(	m_fire_pos, m_fire_dir, fireDispersionBase, *m_Ammo, 
								Owner()->ID(),ID(), SendHitAllowed(Owner()));

	StartShotParticles		();
	Light_Start				();
	StartFlameParticles		();
	StartSmokeParticles		(m_fire_pos, zero_vel);
	OnShellDrop				(m_fire_pos, zero_vel);

	bool b_hud_mode =		(Level().CurrentEntity() == smart_cast<CObject*>(Owner()));
	m_sounds.PlaySound		("sndShot", m_fire_pos, Owner(), b_hud_mode);

	AddShotEffector			();
	m_dAngle.set			(	::Random.randF(-fireDispersionBase,fireDispersionBase),
								::Random.randF(-fireDispersionBase,fireDispersionBase));
}

void CWeaponStatMgun::AddShotEffector				()
{
	if(OwnerActor())
	{
		CCameraShotEffector* S	= smart_cast<CCameraShotEffector*>(OwnerActor()->Cameras().GetCamEffector(eCEShot));

		if (!S)	S			= (CCameraShotEffector*)OwnerActor()->Cameras().AddCamEffector(xr_new<CCameraShotEffector>());
		R_ASSERT			(S);
		S->Initialize		();
		S->Shot2			(0.01f);
	}
}

void  CWeaponStatMgun::RemoveShotEffector	()
{
	if(OwnerActor())
		OwnerActor()->Cameras().RemoveCamEffector	(eCEShot);
}
