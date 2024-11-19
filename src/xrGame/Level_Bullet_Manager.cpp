// Level_Bullet_Manager.cpp:	для обеспечения полета пули по траектории
//								все пули и осколки передаются сюда
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Level.h"
#include "Level_Bullet_Manager.h"
#include "game_cl_base.h"
#include "Actor.h"
#include "gamepersistent.h"
#include "mt_config.h"
#include "game_cl_base_weapon_usage_statistic.h"
#include "game_cl_mp.h"
#include "reward_event_generator.h"

#include "../Include/xrRender/UIRender.h"
#include "../Include/xrRender/Kinematics.h"

#ifdef DEBUG
#	include "debug_renderer.h"
#endif

#define HIT_POWER_EPSILON 0.05f
#define WALLMARK_SIZE 0.04f

float const CBulletManager::parent_ignore_distance		= 3.f;

#ifdef DEBUG
	float air_resistance_epsilon					= .1f;
#else // #ifdef DEBUG
	static float const air_resistance_epsilon		= .1f;
#endif // #ifdef DEBUG
float g_bullet_time_factor							= 1.f;

SBullet::SBullet()
{
}

SBullet::~SBullet()
{
}

void SBullet::Init(const Fvector& position,
				   const Fvector& direction,
				   float barrel_length,
				   u16	sender_id,
				   u16 sendersweapon_id,
				   ALife::EHitType e_hit_type,
				   float maximum_distance,
				   const CCartridge& cartridge,
				   bool SendHit,
				   float power,
				   float impulse)
{
	flags._storage			= 0;
	pos 					= position;
	dir.normalize			(direction);
	speed 					= barrel_length * cartridge.param_s.bullet_speed_per_barrel_len;
	VERIFY					(speed > 0.f);
	
	hit_param.impulse		= impulse;
	hit_param.main_damage	= power;

	VERIFY					(direction.magnitude() > 0.f);

	fly_dist				= 0.f;
	fly_time				= 0.f;
	tracer_start_position	= position;

	parent_id				= sender_id;
	flags.allow_sendhit		= SendHit;
	weapon_id				= sendersweapon_id;
	hit_type				= e_hit_type;
	
	mass					= cartridge.param_s.fBulletMass;
	buck_shot				= cartridge.param_s.buckShot;
	resist					= cartridge.param_s.fBulletResist;
	penetration				= cartridge.param_s.penetration;
	hollow_point			= cartridge.param_s.bullet_hollow_point;
	air_resistance			= cartridge.param_s.fAirResist;
	k_ap					= cartridge.param_s.bullet_k_ap;
	m_u8ColorID				= cartridge.param_s.u8ColorID;
	
	updates					= 0;
	wallmark_size			= (cartridge.param_s.fWMS == -1.f) ? Level().BulletManager().m_fBulletWallMarkSizeScale * sqrt(resist) : cartridge.param_s.fWMS;

	bullet_material_idx		= cartridge.bullet_material_idx;
	VERIFY					( u16(-1) != bullet_material_idx );

	flags.allow_tracer = !!cartridge.m_flags.test(CCartridge::cfTracer);

	flags.allow_ricochet	= !!cartridge.m_flags.test(CCartridge::cfRicochet);
	flags.explosive			= !!cartridge.m_flags.test(CCartridge::cfExplosive);
	flags.magnetic_beam		= !!cartridge.m_flags.test(CCartridge::cfMagneticBeam);

	flags.piercing_was = 0;
//	flags.skipped_frame		= 0;

	init_frame_num			= Device.dwFrame;

	targetID				= u16(-1);
	density					= 1.f;

	static u32 i = 0;
	id = i++;
}

CBulletManager::CBulletManager()
#if 0//def PROFILE_CRITICAL_SECTIONS
	: m_Lock(MUTEX_PROFILE_ID(CBulletManager))
#	ifdef DEBUG
		,m_thread_id(GetCurrentThreadId())
#	endif // #ifdef DEBUG
#else // #ifdef PROFILE_CRITICAL_SECTIONS
#	ifdef DEBUG
		: m_thread_id(GetCurrentThreadId())
#	endif // #ifdef DEBUG
#endif // #ifdef PROFILE_CRITICAL_SECTIONS
{
	m_Bullets.clear			();
	m_Bullets.reserve		(100);
}

CBulletManager::~CBulletManager()
{
	m_Bullets.clear			();
	m_WhineSounds.clear		();
	m_Events.clear			();
}

void CBulletManager::Load()
{
	LPCSTR bullet_manager_sect			= "bullet_manager";

	m_fTracerWidth						= pSettings->r_float(bullet_manager_sect, "tracer_width");
	m_fTracerLengthMax					= pSettings->r_float(bullet_manager_sect, "tracer_length_max");
	m_fTracerLengthMin					= pSettings->r_float(bullet_manager_sect, "tracer_length_min");
	m_fGravityConst						= pSettings->r_float(bullet_manager_sect, "gravity_const");
	m_gravity							= { 0.f, -m_fGravityConst, 0.f };
	m_min_bullet_speed					= pSettings->r_float(bullet_manager_sect, "min_bullet_speed");
	m_max_bullet_fly_time				= pSettings->r_float(bullet_manager_sect, "max_bullet_fly_time");
	m_fCollisionEnergyMin				= pSettings->r_float(bullet_manager_sect, "collision_energy_min");
	m_fCollisionEnergyMax				= pSettings->r_float(bullet_manager_sect, "collision_energy_max");

	m_fHPMaxDist						= pSettings->r_float(bullet_manager_sect, "hit_probability_max_dist");

	m_fBulletAirResistanceScale			= pSettings->r_float(bullet_manager_sect, "air_resistance_scale");
	m_fBulletWallMarkSizeScale			= pSettings->r_float(bullet_manager_sect, "wallmark_size_scale");

	m_fZeroingAirResistCorrectionK1		= pSettings->r_float(bullet_manager_sect, "zeroing_air_resist_correction_k1");
	m_fZeroingAirResistCorrectionK2		= pSettings->r_float(bullet_manager_sect, "zeroing_air_resist_correction_k2");
	m_fZeroingAirResistCorrectionK3		= pSettings->r_float(bullet_manager_sect, "zeroing_air_resist_correction_k3");

	m_global_ap_scale					= pSettings->r_float(bullet_manager_sect, "global_ap_scale");
	m_fBulletHollowPointResistFactor	= pSettings->r_float(bullet_manager_sect, "hollow_point_resist_factor");
	m_fBulletAPLossOnPierce				= pSettings->r_float(bullet_manager_sect, "ap_loss_on_pierce");

	m_fBulletHitImpulseScale			= pSettings->r_float(bullet_manager_sect, "hit_impulse_scale");
	m_fBulletArmorDamageScale			= pSettings->r_float(bullet_manager_sect, "armor_damage_scale");
	m_fBulletPierceDamageScale			= pSettings->r_float(bullet_manager_sect, "pierce_damage_scale");
	m_fBulletArmorPierceDamageScale		= pSettings->r_float(bullet_manager_sect, "armor_pierce_damage_scale");

	m_fBulletPierceDamageFromResist.Load(bullet_manager_sect, "pierce_damage_from_resist");
	m_fBulletPierceDamageFromKAP.Load	(bullet_manager_sect, "pierce_damage_from_kap");
	m_fBulletPierceDamageFromSpeed.Load	(bullet_manager_sect, "pierce_damage_from_speed");

	m_fBulletPierceDamageFromSpeedScale.Load(bullet_manager_sect, "pierce_damage_from_speed_scale");
	m_fBulletPierceDamageFromStability.Load	(bullet_manager_sect, "pierce_damage_from_stability");
	m_fBulletPierceDamageFromPierce.Load(bullet_manager_sect, "pierce_damage_from_pierce");

	pSettings->w_float_ex				(g_bullet_time_factor, bullet_manager_sect, "bullet_velocity_time_factor");

	m_WhineSounds.clear					();
	LPCSTR whine_sounds					= pSettings->r_string(bullet_manager_sect, "whine_sounds");
	xr_string							tmp;
	for (int k = 0, cnt = _GetItemCount(whine_sounds); k < cnt; ++k)
	{
		m_WhineSounds.push_back			(ref_sound());
		m_WhineSounds.back().create		(_GetItem(whine_sounds,k,tmp), st_Effect, sg_SourceType);
	}

	m_ExplodeParticles.clear			();
	LPCSTR explode_particles			= pSettings->r_string(bullet_manager_sect, "explode_particles");
	for (int k = 0, cnt = _GetItemCount(explode_particles); k < cnt; ++k)
		m_ExplodeParticles.push_back	(_GetItem(explode_particles, k, tmp));

	perform_aboba();
}

void CBulletManager::PlayExplodePS( const Fmatrix& xf )
{
	if ( m_ExplodeParticles.empty() )
		return;

	shared_str const& ps_name	= m_ExplodeParticles[Random.randI(0, m_ExplodeParticles.size())];
	CParticlesObject* const	ps	= CParticlesObject::Create(*ps_name,TRUE);
	ps->UpdateParent			(xf,zero_vel);
	GamePersistent().ps_needtoplay.push_back(ps);
}

void CBulletManager::PlayWhineSound(SBullet* bullet, CObject* object, const Fvector& pos)
{
	if (m_WhineSounds.empty())						return;
	if (bullet->m_whine_snd._feedback() != NULL)	return;
	if(bullet->hit_type!=ALife::eHitTypeFireWound ) return;

	bullet->m_whine_snd								= m_WhineSounds[Random.randI(0, m_WhineSounds.size())];
	bullet->m_whine_snd.play_at_pos					(object,pos);
}

void CBulletManager::Clear		()
{
	m_Bullets.clear			();
	m_Events.clear			();
}

void CBulletManager::AddBullet(const Fvector& position,
							   const Fvector& direction,
							   float barrel_length,
							   u16	sender_id,
							   u16 sendersweapon_id,
							   ALife::EHitType e_hit_type,
							   float maximum_distance,
							   const CCartridge& cartridge,
							   bool SendHit,
							   float power,
							   float impulse)
{
#ifdef DEBUG
	VERIFY						( m_thread_id == GetCurrentThreadId() );
#endif
	VERIFY						(u16(-1)!=cartridge.bullet_material_idx);
	m_Bullets.push_back			(SBullet());
	SBullet& bullet				= m_Bullets.back();
	bullet.Init					(position, direction, barrel_length, sender_id, sendersweapon_id, e_hit_type, maximum_distance, cartridge, SendHit, power, impulse);
}

void CBulletManager::UpdateWorkload()
{
//	VERIFY						( m_thread_id == GetCurrentThreadId() );

	rq_storage.r_clear			();

	float time_delta					= (float)Device.dwTimeDelta;
	if (!time_delta)
		return;
	time_delta							*= g_bullet_time_factor * .001f;

	collide::rq_result			dummy;

	// this is because of ugly nature of removing bullets
	// when index in vector passed through the tgt_material field
	// and we can remove them only in case when we iterate bullets
	// in the reversed order
	BulletVec::reverse_iterator	i = m_Bullets.rbegin();
	BulletVec::reverse_iterator	e = m_Bullets.rend();
	for (u16 j=u16(e - i); i != e; ++i, --j) {
		if ( process_bullet( rq_storage, *i, time_delta) )
			continue;

		VERIFY					(j > 0);
		RegisterEvent			(EVENT_REMOVE, FALSE, &*i, Fvector().set(0, 0, 0), dummy, j - 1);
	}
}

bool CBulletManager::process_bullet(collide::rq_results & storage, SBullet& bullet, float time_delta)
{
	bullet.tracer_start_position		= bullet.pos;
	return								update_bullet(storage, bullet, time_delta);
}

bool CBulletManager::update_bullet(collide::rq_results& storage, SBullet& bullet, float time_delta)
{
	Fbox CR$ level_box					= Level().ObjectSpace.GetBoundingVolume();
	if (bullet.pos.x < level_box.x1 || bullet.pos.x > level_box.x2 ||
		bullet.pos.y < level_box.y1 || //bullet.pos.y > level_box.y2 ||
		bullet.pos.z < level_box.z1 || bullet.pos.z > level_box.z2 ||
		bullet.speed < m_min_bullet_speed ||
		bullet.fly_time > m_max_bullet_fly_time)
		return							false;

	bool dbg_log						= Core.ParamFlags.test(Core.dbgbulletfull);
	if (dbg_log && bullet.fly_time >= bullet.flush_time)
	{
		Msg("--xd update_bullet id [%d] speed [%.3f] dir [%.2f,%.2f,%.2f] dist [%.3f] time [%.3f] resist [%.3f]",
			bullet.id, bullet.speed, bullet.dir.x, bullet.dir.y, bullet.dir.z, bullet.fly_dist, bullet.fly_time, bullet.resist);
		bullet.flush_time			+= .1f;
	}

	bullet_test_callback_data			data;
	data.pBullet						= &bullet;
	bullet.flags.ricochet_was			= 0;

	data.dt								= time_delta;
	float resistance					= bullet.air_resistance;
	if (bullet.density > 1.f)
	{
		resistance						*= bullet.density * 500000.f / bullet.speed;
		float time_to_stop				= 1.f / resistance;
		if (time_to_stop < data.dt)
			data.dt						= time_to_stop;
	}
	else
		resistance						*= bullet.speed;
	R_ASSERT							(!fIsZero(resistance));

	data.start_velocity					= Fvector(bullet.dir).mul(bullet.speed);
	data.end_velocity					= Fvector(data.start_velocity).mul(1.f - resistance * data.dt).mad(m_gravity, data.dt);
	data.avg_velocity					= Fvector(data.start_velocity).add(data.end_velocity).mul(.5f);

	Fvector new_position				= Fvector(bullet.pos).mad(data.avg_velocity, data.dt);
	Fvector	start_to_target				= Fvector().sub(new_position, bullet.pos);
	float const distance				= start_to_target.magnitude();
	start_to_target.div					(distance);

	collide::ray_defs RD				(bullet.pos, start_to_target, distance, CDB::OPT_FULL_TEST, collide::rqtBoth);
	if (Level().ObjectSpace.RayQuery(storage, RD, CBulletManager::firetrace_callback, &data, CBulletManager::test_callback, NULL))
	{
		++bullet.updates;
		if (dbg_log)
			Msg("--xd after firetrace_callback id [%d] updates [%d] speed [%.3f] fly_dist [%.3f] fly_time [%.3f] resist [.%3f]",
				bullet.id, bullet.updates, bullet.speed, bullet.fly_dist, bullet.fly_time, bullet.resist);
		R_ASSERT						(bullet.updates < 1000);

		float d_pos						= .01f;
		bullet.pos.mad					(bullet.dir, d_pos);
		bullet.fly_dist					+= d_pos;
		float d_time					= (bullet.speed >= m_min_bullet_speed) ? d_pos / bullet.speed : 0.f;
		bullet.fly_time					+= d_time;
		
		return							update_bullet(storage, bullet, time_delta - data.collide_time - d_time);
	}

	bullet.fly_dist						+= distance;
	bullet.fly_time						+= time_delta;
	bullet.pos							= new_position;
	bullet.speed						= data.end_velocity.magnitude();
	bullet.dir							= data.end_velocity.normalize_safe();
	bullet.updates						= 0;

	return								!fis_zero(bullet.speed);
}

BOOL CBulletManager::firetrace_callback	(collide::rq_result& result, LPVOID params)
{
	bullet_test_callback_data& data		= *(bullet_test_callback_data*)params;
	data.collide_time					= result.range / data.avg_velocity.magnitude();
	if (fIsZero(result.range))
		return							TRUE;

	SBullet& bullet						= *data.pBullet;
	Fvector collide_position			= Fvector(bullet.pos).mad(data.avg_velocity.normalize(), result.range);
	bullet.fly_dist						+= bullet.pos.distance_to(collide_position);
	bullet.fly_time						+= data.collide_time;
	bullet.pos							= collide_position;
	bullet.dir.lerp						(data.start_velocity, data.end_velocity, data.collide_time / data.dt);
	bullet.speed						= bullet.dir.magnitude();
	bullet.dir.normalize_safe			();

	CBulletManager& bullet_manager		= Level().BulletManager();
	//статический объект
	if (!result.O)
	{
		CDB::TRI const& triangle		= *(Level().ObjectSpace.GetStaticTris() + result.element);
		bullet_manager.RegisterEvent	(EVENT_HIT, FALSE, &bullet, collide_position, result, triangle.material);
		return							(FALSE);
	}

	//динамический объект
	VERIFY								(!(result.O->ID() == bullet.parent_id && bullet.fly_dist < parent_ignore_distance));
	IKinematics* const kinematics		= smart_cast<IKinematics*>(result.O->Visual());
	if (!kinematics)
		return							(FALSE);

	CBoneData const& bone_data			= kinematics->LL_GetData( (u16)result.element );
	bullet_manager.RegisterEvent		(EVENT_HIT, TRUE, &bullet, collide_position, result, bone_data.game_mtl_idx);
	return								(FALSE);
}

#ifdef DEBUG
	BOOL g_bDrawBulletHit = FALSE;
#endif

float SqrDistancePointToSegment(const Fvector& pt, const Fvector& orig, const Fvector& dir)
{
	Fvector diff;	diff.sub(pt,orig);
	float fT		= diff.dotproduct(dir);

	if ( fT <= 0.0f ){
		fT = 0.0f;
	}else{
		float fSqrLen= dir.square_magnitude();
		if ( fT >= fSqrLen ){
			fT = 1.0f;
			diff.sub(dir);
		}else{
			fT /= fSqrLen;
			diff.sub(Fvector().mul(dir,fT));
		}
	}

	return diff.square_magnitude();
}

void CBulletManager::Render	()
{
#ifdef DEBUG
	if (g_bDrawBulletHit && !m_bullet_points.empty()) {
		VERIFY							(!(m_bullet_points.size() % 2));
		CDebugRenderer& renderer		= Level().debug_renderer();
		Fmatrix	sphere					= Fmatrix().scale(.05f, .05f, .05f);
		BulletPoints::const_iterator	i = m_bullet_points.begin();
		BulletPoints::const_iterator	e = m_bullet_points.end();
		for ( ; i != e; i+=2) {
			sphere.c					= *i;
			renderer.draw_ellipse		(sphere, D3DCOLOR_XRGB(255, 0, 0));

			renderer.draw_line			(Fidentity, *i, *(i + 1), D3DCOLOR_XRGB(0, 255, 0));

			sphere.c					= *(i + 1);
			renderer.draw_ellipse		(sphere, D3DCOLOR_XRGB(255, 0, 0));
		}

		if (m_bullet_points.size() > 32768)
			m_bullet_points.clear_not_free	();
	}
	else
		m_bullet_points.clear_not_free	();

	//0-рикошет
	//1-застрявание пули в материале
	//2-пробивание материала
	if (g_bDrawBulletHit) {
		extern FvectorVec g_hit[];
		FvectorIt it;
		u32 C[3] = {0xffff0000,0xff00ff00,0xff0000ff};
		//RCache.set_xform_world(Fidentity);
		DRender->CacheSetXformWorld(Fidentity);
		for(int i=0; i<3; ++i)
			for(it=g_hit[i].begin();it!=g_hit[i].end();++it){
				Level().debug_renderer().draw_aabb(*it,0.01f,0.01f,0.01f,C[i]);
			}
	}
#endif

	if(m_BulletsRendered.empty()) return;

	//u32	vOffset			=	0	;
	u32 bullet_num		=	m_BulletsRendered.size();

	UIRender->StartPrimitive((u32)bullet_num*12, IUIRender::ptTriList, IUIRender::pttLIT);

	for(BulletVecIt it = m_BulletsRendered.begin(); it!=m_BulletsRendered.end(); it++){
		SBullet* bullet					= &(*it);
		if(!bullet->flags.allow_tracer)	
			continue;
		if (!psActorFlags.test(AF_USE_TRACERS))
			continue;

		if (!bullet->CanBeRenderedNow())
			continue;

		Fvector const tracer			= Fvector().sub(bullet->pos, bullet->tracer_start_position);
		float length					= tracer.magnitude();
		Fvector const tracer_direction	= length >= EPS_L ? Fvector(tracer).mul(1.f/length) : Fvector().set(0.f, 0.f, 1.f);

		if (length < m_fTracerLengthMin) 
			continue;

		if (length > m_fTracerLengthMax)
			length			= m_fTracerLengthMax;

		float width			= m_fTracerWidth;
		float dist2segSqr	= SqrDistancePointToSegment(Device.camera.position, bullet->pos, tracer);
		//---------------------------------------------
		float MaxDistSqr = 1.0f;
		float MinDistSqr = 0.09f;
		if (dist2segSqr < MaxDistSqr)
		{
			if (dist2segSqr < MinDistSqr) dist2segSqr = MinDistSqr;

			width *= _sqrt(dist2segSqr/MaxDistSqr);
		}
		if (Device.camera.position.distance_to_sqr(bullet->pos)<(length*length))
		{
			length = Device.camera.position.distance_to(bullet->pos) - 0.3f;
		}

		Fvector center;
		center.mad				(bullet->pos, tracer_direction,  -length*.5f);
		bool bActor				= false;
		if(Level().CurrentViewEntity())
		{
			bActor				= ( bullet->parent_id == Level().CurrentViewEntity()->ID() );
		}
		tracers.Render			(bullet->pos, center, tracer_direction, length, width, bullet->m_u8ColorID, bullet->speed, bActor);
	}
	
	UIRender->CacheSetCullMode		(IUIRender::cmNONE);
	UIRender->CacheSetXformWorld	(Fidentity);
	UIRender->SetShader				(*tracers.sh_Tracer);
	UIRender->FlushPrimitive		();
	UIRender->CacheSetCullMode		(IUIRender::cmCCW);
}

void CBulletManager::CommitRenderSet		()	// @ the end of frame
{
	m_BulletsRendered	= m_Bullets			;
	if (g_mt_config.test(mtBullets))		{
		Device.seqParallel.push_back		(fastdelegate::FastDelegate0<>(this,&CBulletManager::UpdateWorkload));
	} else {
		UpdateWorkload						();
	}
}
void CBulletManager::CommitEvents			()	// @ the start of frame
{
	if (m_Events.size() > 1000)
		Msg			("! too many bullets during single frame: %d", m_Events.size());

	for (u32 _it=0; _it<m_Events.size(); _it++)	{
		_event&		E	= m_Events[_it];
		switch (E.Type)
		{
		case EVENT_HIT:
			{
				if (E.dynamic)	DynamicObjectHit	(E);
				else			StaticObjectHit		(E);
			}break;
		case EVENT_REMOVE:
			{
				m_Bullets[E.tgt_material] = m_Bullets.back();
				m_Bullets.pop_back();
			}break;
		}		
	}
	m_Events.clear_and_reserve	()	;
}

void CBulletManager::RegisterEvent(EventType Type, BOOL _dynamic, SBullet* bullet, const Fvector& end_point, collide::rq_result& R, u16 tgt_material)
{
	if (_dynamic && (bullet->targetID == R.O->ID()))
		return;

	m_Events.emplace_back				();
	_event&	E							= m_Events.back();
	E.Type								= Type;
	E.bullet							= *bullet;
	
	switch(Type)
	{
	case EVENT_HIT:
	{
		E.dynamic						= _dynamic;
		E.point							= end_point;
		E.R								= R;
		E.tgt_material					= tgt_material;
		ObjectHit						(E, bullet, end_point, R, tgt_material);
		break;
	}
	case EVENT_REMOVE:
		E.tgt_material					= tgt_material;
		break;
	}	
}

float CBulletManager::CalcZeroingCorrection(float k, float z) const
{
	return								1.f / (1.f + m_fZeroingAirResistCorrectionK3 / (k - z) - m_fZeroingAirResistCorrectionK3 / k);
}

float CBulletManager::calculateAP(float penetration, float speed) const
{
	return								m_global_ap_scale * penetration * logf(1.f + .00000055f * _sqr(speed));
}

#include "items_library.h"
#include "BoneProtections.h"
#include "EntityCondition.h"
#include <fstream>
void CBulletManager::perform_aboba()
{
	std::ofstream output;
	output.open("C:\\tmp.txt");
	shared_str str;

	for (auto& sect : CItemsLibrary::getDivision("ammo", "cartridge", "void"))
	{
		auto cartridge = CCartridge(sect.c_str());
		if (cartridge.m_flags.test(CCartridge::cfTracer))
			continue;

		float bullet_speed = cartridge.param_s.bullet_speed_per_barrel_len * cartridge.param_s.barrel_len;
		float bullet_ap = calculateAP(cartridge.param_s.penetration, bullet_speed);

		SBullet bullet;
		bullet.Init(vZero, vForward, cartridge.param_s.barrel_len, 0, 0, ALife::eHitTypeFireWound, 0.f, cartridge, false, -1.f, -1.f);

		str.printf("======================================== %s [%.3f]", CInventoryItem::readName(sect), bullet_ap);
		output << str.c_str() << std::endl;

		for (int i = -1; i < 11; ++i)
		{
			float armor = (i >= 0) ? SBoneProtections::s_armor_levels[i] : 0;
			SBullet_Hit hit;
			calculate_hit_damage(bullet_ap, armor, 5.f, hit, &SBullet(bullet));

			float full_main_damage = hit.main_damage;
			hit.main_damage -= min(hit.main_damage, CEntityCondition::StrikeDamageThreshold.Calc(armor));
			if (fMore(hit.main_damage, 0.f))
				hit.main_damage /= (1.f + CEntityCondition::StrikeDamageResistance.Calc(armor));

			if (fIsZero(hit.pierce_damage))
				str.printf("ARMOR LEVEL %2d [%3.0f] MAIN DAMAGE [%.3f]-[%.3f] PIERCE DAMAGE [---] FULL_DAMAGE [%.3f] HITS TO KILL [%.3f]",
					i, armor, full_main_damage, hit.main_damage, hit.main_damage, 1.f / hit.main_damage);
			else
			{
				float full_damage = hit.main_damage + hit.pierce_damage;
				str.printf("ARMOR LEVEL %2d [%3.0f] MAIN DAMAGE [%.3f]-[%.3f] PIERCE DAMAGE [%.3f] FULL_DAMAGE [%.3f] HITS TO KILL [%.3f]",
					i, armor, full_main_damage, hit.main_damage, hit.pierce_damage, full_damage, 1.f / full_damage);
			}
			output << str.c_str() << std::endl;
		}
	}
}
