// Level_Bullet_Manager.h:  дл€ обеспечени€ полета пули по траектории
//							все пули и осколки передаютс€ сюда
//////////////////////////////////////////////////////////////////////

#pragma once

#include "weaponammo.h"
#include "tracer.h"

//коэфициенты и параметры патрона
struct SBullet_Hit 
{
	float	impulse					= 0.f;
	float	main_damage				= 0.f;
	float	pierce_damage			= 0.f;
	float	armor_pierce_damage		= 0.f;
};

//структура, описывающа€ пулю и ее свойства в полете
struct SBullet
{
	u32				init_frame_num		;			//номер кадра на котором была запущена пул€
	union			{
		struct			{
			u16			ricochet_was	: 1	;			//пул€ срикошетила
			u16			explosive		: 1	;			//special explosive mode for particles
			u16			allow_tracer	: 1	;
			u16			allow_ricochet	: 1	;			//разрешить рикошет
			u16			allow_sendhit	: 1	;			//statistics
			u16			magnetic_beam	: 1 ;			//магнитный луч (нет отклонени€ после пробивани€, не падает скорость после пробивани€)
			u16			piercing_was	: 1	;
		};
		u16				_storage			;
	}				flags				;
	u16				bullet_material_idx	;

	Fvector			pos					;			//текуща€ позици€
	Fvector			dir					;
	float			speed				;			//текуща€ скорость
	
	u16				parent_id			;			//ID персонажа который иницировал действие
	u16				weapon_id			;			//ID оружи€ из которого была выпущены пул€
	
	float			fly_dist			;			//дистанци€ которую пул€ пролетела
	float			fly_time			;			//¬рем€ которое пул€ летит
	Fvector			tracer_start_position;

	//коэфициенты и параметры патрона
	SBullet_Hit     hit_param;
	//-------------------------------------------------------------------
	float			air_resistance		;
	//-------------------------------------------------------------------
	u16				updates				;
	float			wallmark_size		;
	float			mass				;
	int				buck_shot			;
	float			resist				;
	float			penetration			;
	float			k_ap				;
	bool			hollow_point		;
	//-------------------------------------------------------------------
	u8				m_u8ColorID			;
	
	//тип наносимого хита
	ALife::EHitType hit_type			;
	//---------------------------------
	u32				m_dwID				;
	ref_sound		m_whine_snd			;
	ref_sound		m_mtl_snd			;
	//---------------------------------
	u16				targetID			;
	//---------------------------------
	float			density				;
	bool			operator	==		(u32 ID){return	ID == m_dwID;}
public:
					SBullet				();
					~SBullet			();

	bool			CanBeRenderedNow	() const { return (Device.dwFrame > init_frame_num);}

	void			Init				(const	Fvector& position,
										const	Fvector& direction,
										float	barrel_length,
										u16		sender_id,
										u16		sendersweapon_id,
										ALife::EHitType e_hit_type,
										float	maximum_distance,
										const	CCartridge& cartridge,
										bool	SendHit,
										float	power,
										float	impulse);

	u32 id = 0;
	float flush_time = 0.f;
};

class CLevel;

class CBulletManager
{
private:
	static float const parent_ignore_distance;

private:
	collide::rq_results		rq_storage;
	collide::rq_results		m_rq_results;

private:
	DEFINE_VECTOR						(ref_sound,SoundVec,SoundVecIt);
	DEFINE_VECTOR						(SBullet,BulletVec,BulletVecIt);
	friend	CLevel;

	enum EventType {
		EVENT_HIT	= u8(0),
		EVENT_REMOVE,

		EVENT_DUMMY = u8(-1),
	};
	struct	_event			{
		EventType			Type;
		BOOL				dynamic		;
		SBullet_Hit			hit_result	;
		SBullet				bullet		;
		Fvector				normal		;
		Fvector				point		;
		collide::rq_result	R			;
		u16					tgt_material;
	};
	static void CalculateNewVelocity(Fvector & dest_new_vel, Fvector const & old_velocity, float ar, float life_time);
protected:
	SoundVec				m_WhineSounds		;
	RStringVec				m_ExplodeParticles	;

	//список пуль наход€щихс€ в данный момент на уровне
//.	xrCriticalSection		m_Lock				;

	BulletVec				m_Bullets			;	// working set, locked
	BulletVec				m_BulletsRendered	;	// copy for rendering
	xr_vector<_event>		m_Events			;

#ifdef DEBUG
	u32						m_thread_id;

	typedef xr_vector<Fvector>	BulletPoints;
	BulletPoints			m_bullet_points;
#endif // #ifdef DEBUG

	//отрисовка трассеров от пуль
	CTracer					tracers;

	float					m_fHPMaxDist;

	//константа G
	float					m_fGravityConst;
	//cколько процентов энергии потер€ет пул€ при столкновении с материалом (при падении под пр€мым углом)
	float					m_fCollisionEnergyMin;
	//сколькол процентов энергии устанетс€ у пули при любом столкновении
	float					m_fCollisionEnergyMax;

	//параметры отрисовки трассеров
	float					m_fTracerWidth;
	float 					m_fTracerLengthMax;
	float 					m_fTracerLengthMin;

protected:
	void					PlayWhineSound		(SBullet* bullet, CObject* object, const Fvector& pos);
	void					PlayExplodePS		(const Fmatrix& xf);
	//функци€ обработки хитов объектов
	static BOOL 			test_callback		(const collide::ray_defs& rd, CObject* object, LPVOID params);
	static BOOL				firetrace_callback	(collide::rq_result& result, LPVOID params);

	// Deffer event
	void					RegisterEvent		(EventType Type, BOOL _dynamic, SBullet* bullet, const Fvector& end_point, collide::rq_result& R, u16 target_material);
	
	//попадание по динамическому объекту
	void					DynamicObjectHit	(_event& E);
	
	//попадание по статическому объекту
	void					StaticObjectHit		(_event& E);

	//попадание по любому объекту, на выходе - импульс и сила переданные пулей объекту
	void					ObjectHit			(_event& E, SBullet* bullet, const Fvector& end_point, collide::rq_result& R, u16 target_material);

	//отметка на пораженном объекте
	void					FireShotmark		(SBullet* bullet, const Fvector& vDir, 
												const Fvector &vEnd,    collide::rq_result& R,  u16 target_material,
												const Fvector& vNormal, bool ShowMark = true);
	//просчет полета пули за некоторый промежуток времени
	//принимаетс€ что на этом участке пул€ движетс€ пр€молинейно
	//и равномерно, а после просчета также измен€етс€ текуща€
	//скорость и положение с учетом гравитации и ветра
	//возвращаем true если пул€ продолжает полет
	bool					update_bullet(
								collide::rq_results& rq_storage, 
								SBullet& bullet,
								float time_delta);
	bool					process_bullet(
								collide::rq_results& rq_storage,
								SBullet& bullet,
								float time_delta);
	void 		__stdcall	UpdateWorkload		();

public:
							CBulletManager		();
	virtual					~CBulletManager		();

	void 					Load				();
	void 					Clear				();
	void 					AddBullet			(const Fvector& position, const Fvector& direction, float barrel_length,
												u16	sender_id, u16 sendersweapon_id,
												ALife::EHitType e_hit_type, float maximum_distance, 
												const CCartridge& cartridge, bool SendHit,
												float power, float impulse);

	void					CommitEvents		();	// @ the start of frame
	void					CommitRenderSet		();	// @ the end of frame
	void 					Render				();

private:
	float								m_min_bullet_speed;
	float								m_max_bullet_fly_time;
	Fvector								m_gravity;
	float								m_global_ap_scale;

	float								calculate_hit_damage
	(
		float bullet_ap, float armor, float bone_density,
		SBullet_Hit& hit_res, SBullet* bullet,
		float k_speed_in = 0.f, float k_speed_out = 0.f, bool inwards = true
	) const;

	void perform_aboba();

public:
	float								m_fBulletAirResistanceScale;
	float								m_fBulletWallMarkSizeScale;

	float								m_fZeroingAirResistCorrectionK1;
	float								m_fZeroingAirResistCorrectionK2;
	float								m_fZeroingAirResistCorrectionK3;

	float								m_fBulletHollowPointResistFactor;
	float								m_fBulletAPLossOnPierce;

	float								m_fBulletHitImpulseScale;
	float								m_fBulletArmorDamageScale;
	float								m_fBulletPierceDamageScale;
	float								m_fBulletArmorPierceDamageScale;

	CPowerDependency					m_fBulletPierceDamageFromResist;
	CPowerDependency					m_fBulletPierceDamageFromKAP;
	CPowerDependency					m_fBulletPierceDamageFromSpeed;

	CPowerDependency					m_fBulletPierceDamageFromSpeedScale;
	CPowerDependency					m_fBulletPierceDamageFromStability;
	CPowerDependency					m_fBulletPierceDamageFromPierce;

	float								GravityConst						C$	()		{ return m_fGravityConst; }

	float								CalcZeroingCorrection				C$	(float k, float z);
	float								calculateAP							C$	(float penetration, float speed);
};

struct bullet_test_callback_data
{
	SBullet*		pBullet;
	float			collide_time;
	float			dt;
	Fvector			start_velocity;
	Fvector			end_velocity;
	Fvector			avg_velocity;
};
