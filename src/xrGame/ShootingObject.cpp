//////////////////////////////////////////////////////////////////////
// ShootingObject.cpp:  ��������� ��� ��������� ���������� �������� 
//						(������ � ���������� �������) 	
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ShootingObject.h"

#include "ParticlesObject.h"
#include "WeaponAmmo.h"

#include "actor.h"
#include "spectator.h"
#include "game_cl_base.h"
#include "level.h"
#include "level_bullet_manager.h"
#include "game_cl_single.h"

constexpr float HIT_POWER_EPSILON = .05f;
constexpr float WALLMARK_SIZE = .04f;

CShootingObject::CShootingObject(void)
{
	fShotTimeCounter				= 0;
 	fOneShotTime					= 0;

	m_vCurrentShootDir.set			(0,0,0);
	m_vCurrentShootPos.set			(0,0,0);
	m_iCurrentParentID				= 0xFFFF;
	
	bWorking						= false;

	light_render					= 0;
}

void CShootingObject::Load(LPCSTR section)
{
	//����� ������������� �� �������
	m_rpm								= pSettings->r_float(section, "rpm");
	fOneShotTime						= (m_rpm) ? 60.f / m_rpm : 0.f;
	LoadFireParams						(section);
}

void CShootingObject::Light_Create		()
{
	//lights
	light_render				=	::Render->light_create();
	if (::Render->get_generation()==IRender_interface::GENERATION_R2)	light_render->set_shadow	(true);
	else																light_render->set_shadow	(false);
}

void CShootingObject::Light_Destroy		()
{
	light_render.destroy		();
}

void CShootingObject::LoadFireParams( LPCSTR section )
{
	//������� ��������� ������
	fireDispersionBase	= deg2rad( pSettings->r_float	(section,"fire_dispersion_base"	) );
}

void CShootingObject::Light_Start()
{
	if (m_silencer || !m_cartridge.light_enabled)
		return;

	if (!light_render)
		Light_Create();

	if (Device.dwFrame	!= light_frame)
	{
		light_frame					= Device.dwFrame;
		light_time					= m_cartridge.light_lifetime;
		
		light_build_color.set		(
			Random.randFs(m_cartridge.light_var_color, m_cartridge.light_base_color.r),
			Random.randFs(m_cartridge.light_var_color, m_cartridge.light_base_color.g),
			Random.randFs(m_cartridge.light_var_color, m_cartridge.light_base_color.b),
			1
		);
		light_build_range			= Random.randFs(m_cartridge.light_var_range, m_cartridge.light_base_range);
	}
}

void CShootingObject::Light_Render(const Fvector& P)
{
	float light_scale			= light_time / m_cartridge.light_lifetime;
	R_ASSERT(light_render);

	light_render->set_position	(P);
	light_render->set_color		(light_build_color.r*light_scale,light_build_color.g*light_scale,light_build_color.b*light_scale);
	light_render->set_range		(light_build_range*light_scale);

	if (!light_render->get_active())
		light_render->set_active(true);
}

//////////////////////////////////////////////////////////////////////////
// Particles
//////////////////////////////////////////////////////////////////////////

void CShootingObject::StartParticles (CParticlesObject*& pParticles, LPCSTR particles_name, 
									 const Fvector& pos, const  Fvector& vel, bool auto_remove_flag)
{
	if(!particles_name) return;

	if(pParticles != NULL) 
	{
		UpdateParticles(pParticles, pos, vel);
		return;
	}

	pParticles = CParticlesObject::Create(particles_name,(BOOL)auto_remove_flag);
	
	UpdateParticles(pParticles, pos, vel);
	CSpectator* tmp_spectr = smart_cast<CSpectator*>(Level().CurrentControlEntity());
	bool in_hud_mode = IsHudModeNow();
	if (in_hud_mode && tmp_spectr &&
		(tmp_spectr->GetActiveCam() != CSpectator::eacFirstEye))
	{
		in_hud_mode = false;
	}
	pParticles->Play(in_hud_mode);
}
void CShootingObject::StopParticles (CParticlesObject*&	pParticles)
{
	if(pParticles == NULL) return;

	pParticles->Stop		();
	CParticlesObject::Destroy(pParticles);
}

void CShootingObject::UpdateParticles (CParticlesObject*& pParticles, 
							   const Fvector& pos, const Fvector& vel)
{
	if(!pParticles)		return;

	Fmatrix particles_pos; 
	particles_pos.set	(get_ParticlesXFORM());
	particles_pos.c.set	(pos);
	
	pParticles->SetXFORM(particles_pos);

	if(!pParticles->IsAutoRemove() && !pParticles->IsLooped() 
		&& !pParticles->PSI_alive())
	{
		pParticles->Stop		();
		CParticlesObject::Destroy(pParticles);
	}
}

void CShootingObject::OnShellDrop(const Fvector& play_pos, const Fvector& parent_vel)
{
	if(!m_cartridge.shell_particles) return;
	if( Device.camera.position.distance_to_sqr(play_pos)>2*2 ) return;

	CParticlesObject* pShellParticles	= CParticlesObject::Create(m_cartridge.shell_particles.c_str(), TRUE);

	Fmatrix particles_pos; 
	particles_pos.set		(get_ParticlesXFORM());
	particles_pos.c.set		(play_pos);

	pShellParticles->UpdateParent		(particles_pos, parent_vel);
	CSpectator* tmp_spectr = smart_cast<CSpectator*>(Level().CurrentControlEntity());
	bool in_hud_mode = IsHudModeNow();
	if (in_hud_mode && tmp_spectr &&
		(tmp_spectr->GetActiveCam() != CSpectator::eacFirstEye))
	{
		in_hud_mode = false;
	}
	pShellParticles->Play(in_hud_mode);
}

//�������� ����
void CShootingObject::StartSmokeParticles(const Fvector& play_pos, const Fvector& parent_vel)
{
	CParticlesObject* pSmokeParticles = nullptr;
	shared_str CR$ smoke_particles = (m_silencer) ? m_cartridge.smoke_particles_silencer : m_cartridge.smoke_particles;
	StartParticles(pSmokeParticles, smoke_particles.c_str(), play_pos, parent_vel, true);
}

void CShootingObject::StartFlameParticles	()
{
	if (m_silencer)
		return;
	shared_str CR$ flame_particles = (m_flash_hider) ? m_cartridge.flame_particles_flash_hider : m_cartridge.flame_particles;
	if (!flame_particles.size())
		return;

	//���� �������� �����������
	if (m_flame_particles && m_flame_particles->IsLooped() && m_flame_particles->IsPlaying())
	{
		UpdateFlameParticles();
		return;
	}

	StopFlameParticles();
	m_flame_particles = CParticlesObject::Create(flame_particles.c_str(), FALSE);
	UpdateFlameParticles();

	CSpectator* tmp_spectr = smart_cast<CSpectator*>(Level().CurrentControlEntity());
	bool in_hud_mode = IsHudModeNow();
	if (in_hud_mode && tmp_spectr &&
		(tmp_spectr->GetActiveCam() != CSpectator::eacFirstEye))
	{
		in_hud_mode = false;
	}
	m_flame_particles->Play(in_hud_mode);
}

void CShootingObject::StopFlameParticles	()
{
	if (m_flame_particles)
	{
		m_flame_particles->SetAutoRemove(true);
		m_flame_particles->Stop();
		m_flame_particles = nullptr;
	}
}

void CShootingObject::UpdateFlameParticles	()
{
	if (!m_flame_particles)
		return;

	Fmatrix		pos; 
	pos.set		(get_ParticlesXFORM()	); 
	pos.c.set	(get_CurrentFirePoint()	);

	VERIFY(_valid(pos));

	m_flame_particles->SetXFORM			(pos);

	if (!m_flame_particles->IsLooped() && !m_flame_particles->IsPlaying() && !m_flame_particles->PSI_alive())
	{
		m_flame_particles->Stop();
		CParticlesObject::Destroy(m_flame_particles);
	}
}

//��������� �� ��������
void CShootingObject::UpdateLight()
{
	if (light_render && light_time>0)		
	{
		light_time -= Device.fTimeDelta;
		if (light_time<=0) StopLight();
	}
}

void CShootingObject::StopLight			()
{
	if(light_render){
		light_render->set_active(false);
	}
}

void CShootingObject::RenderLight()
{
	if ( light_render && light_time>0 ) 
	{
		Light_Render(get_CurrentFirePoint());
	}
}

bool CShootingObject::SendHitAllowed		(CObject* pUser)
{
	if (Game().IsServerControlHits())
		return OnServer();

	if (OnServer())
	{
		if (smart_cast<CActor*>(pUser))
		{
			if (Level().CurrentControlEntity() != pUser)
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		if (smart_cast<CActor*>(pUser))
		{
			if (Level().CurrentControlEntity() == pUser)
			{
				return true;
			}
		}
		return false;
	}
};

extern void random_dir(Fvector& tgt_dir, const Fvector& src_dir, float dispersion);

void CShootingObject::FireBullet(const Fvector& pos, 
								 const Fvector& shot_dir, 
								 float fire_disp,
								 const CCartridge& cartridge,
								 u16 parent_id,
								 u16 weapon_id,
								 bool send_hit)
{

	Fvector dir;
	random_dir(dir,shot_dir,fire_disp);

	m_vCurrentShootDir = dir;
	m_vCurrentShootPos = pos;
	m_iCurrentParentID = parent_id;

	Level().BulletManager().AddBullet(
		pos,
		dir,
		m_barrel_len * m_muzzle_koefs.bullet_speed,
		parent_id, 
		weapon_id,
		ALife::eHitTypeFireWound, 0.f,
		cartridge,
		send_hit,
		-1.f,
		-1.f
	);
}
void CShootingObject::FireStart	()
{
	bWorking=true;	
}
void CShootingObject::FireEnd	()				
{ 
	bWorking=false;	
}

void CShootingObject::StartShotParticles	()
{
	CParticlesObject* pSmokeParticles = nullptr;
	StartParticles(pSmokeParticles, m_cartridge.shot_particles.c_str(), m_vCurrentShootPos, m_vCurrentShootDir, true);
}
