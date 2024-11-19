#pragma once

class CWound;
class NET_Packet;
class CEntityAlive;
class CLevel;

#include "hit_immunity.h"
#include "Hit.h"
#include "Level.h"
#include "ActorHelmet.h"
#include "CustomOutfit.h"
enum EBoostParams{
	eBoostPainkill= 0,
	eBoostRegeneration,
	eBoostRecuperation,
	eBoostEndurance,
	eBoostTenacity,
	eBoostAntirad,
	eBoostMaxCount
};

static const LPCSTR ef_boosters_section_names[] =
{
	"boost_painkill",
	"boost_regeneration",
	"boost_recuperation",
	"boost_endurance",
	"boost_tenacity",
	"boost_antirad"
};

struct SBooster{
	float fBoostTime;
	float fBoostValue;
	EBoostParams m_type;
	SBooster() :fBoostTime(-1.0f), fBoostValue(0.f), m_type(eBoostPainkill){};
	void Load(const shared_str& sect, EBoostParams type);
};

struct SMedicineInfluenceValues{
	float fHealth;
	float fPower;
	float fSatiety;
	float fRadiation;
	float fWoundsHeal;
	float fMaxPowerUp;
	float fAlcohol;
	float fTimeTotal;
	float fTimeCurrent;

	SMedicineInfluenceValues():fTimeCurrent(-1.0f){}
	bool InProcess (){return fTimeCurrent>0.0f;}
	void Load(const shared_str& sect, float depletion_rate = 1.f);
};

class CEntityConditionSimple
{
	float					m_fHealth;
	float					m_fHealthMax;
public:
							CEntityConditionSimple	();
	virtual					~CEntityConditionSimple	();

	IC		 float				GetHealth				() const				{return m_fHealth;}
	IC		 void				SetHealth				( const float value ) 	{ m_fHealth = value; }
	IC		 float 				GetMaxHealth			() const				{return m_fHealthMax;}
	IC const float&				health					() const				{return	m_fHealth;}
	IC		 float&				max_health				()						{return	m_fHealthMax;}
};

class CEntityCondition: public CEntityConditionSimple, public CHitImmunity
{
private:
	bool					m_use_limping_state;
	CEntityAlive			*m_object;

public:
							CEntityCondition		(CEntityAlive *object);
	virtual					~CEntityCondition		();

	virtual void			LoadCondition			(LPCSTR section);
	virtual void			LoadTwoHitsDeathParams	(LPCSTR section);
	virtual void			remove_links			(const CObject *object);

	virtual void			save					(NET_Packet &output_packet);
	virtual void			load					(IReader &input_packet);

	IC float				GetPower				() const			{return m_fPower;}	
	IC float				GetRadiation			() const			{return m_fRadiation;}
	IC float				GetPsyHealth			() const			{return m_fPsyHealth;}
	IC float				GetSatiety				() const			{return 1.0f;}	

	IC float 				GetEntityMorale			() const			{return m_fEntityMorale;}

	IC float 				GetHealthLost			() const			{return m_fHealthLost;}

	virtual bool 			IsLimping				() const;

	virtual void			ChangeSatiety			(const float value)		{};
	void 					ChangeHealth			(const float value);
	void 					ChangePower				(const float value);
	void 					ChangeRadiation			(const float value);
	void 					ChangePsyHealth			(const float value);
	virtual void 			ChangeAlcohol			(const float value){};

	IC void					MaxPower				()					{m_fPower = m_fPowerMax;};
	IC void					SetMaxPower				(const float val)	{m_fPowerMax = val; clamp(m_fPowerMax,0.1f,1.0f);};
	IC float				GetMaxPower				() const			{return m_fPowerMax;};

	void 					ChangeBleeding			(const float percent);

	void 					ChangeCircumspection	(const float value);
	void 					ChangeEntityMorale		(const float value);

	virtual CWound*			ConditionHit			(SHit* pHDS);
	//обновления состояния с течением времени
	virtual void			UpdateCondition			();
	void					UpdateWounds			();
	void					UpdateConditionTime		();
	IC void					SetConditionDeltaTime	(float DeltaTime) { m_fDeltaTime = DeltaTime; };

	
	//скорость потери крови из всех открытых ран 
	float					BleedingSpeed			();

	CObject*				GetWhoHitLastTime		() {return m_pWho;}
	u16						GetWhoHitLastTimeID		() {return m_iWhoID;}

	CWound*					AddWound				(float hit_power, ALife::EHitType hit_type, u16 element);

	IC void 				SetCanBeHarmedState		(bool CanBeHarmed) 			{m_bCanBeHarmed = CanBeHarmed;}
	IC bool					CanBeHarmed				() const					{return OnServer() && m_bCanBeHarmed;};
	virtual bool			ApplyInfluence			(const SMedicineInfluenceValues& V, const shared_str& sect);
	virtual bool			ApplyBooster			(const SBooster& B, const shared_str& sect);
	void					ClearWounds				();

	typedef					xr_map<EBoostParams, SBooster> BOOSTER_MAP;

protected:
	void					UpdateHealth			();
	void					UpdatePower				();
	virtual void			UpdateRadiation			();
	void					UpdatePsyHealth			();

	void					UpdateEntityMorale		();

	//изменение силы хита в зависимости от надетого костюма
	//(только для InventoryOwner)
	CCustomOutfit*			GetOutfit				();
	CHelmet*				GetHelmet				();

	void					HitProtectionEffect		(SHit* pHDS);
	//изменение потери сил в зависимости от надетого костюма
	float					HitPowerEffect			(float power_loss);
	
	//для подсчета состояния открытых ран,
	//запоминается кость куда был нанесен хит
	//и скорость потери крови из раны
	DEFINE_VECTOR(CWound*, WOUND_VECTOR, WOUND_VECTOR_IT);
	WOUND_VECTOR			m_WoundVector;
	//очистка массива ран
	

	//все величины от 0 до 1			
	float m_fPower;					//сила
	float m_fRadiation;				//доза радиактивного облучения
	float m_fPsyHealth;				//здоровье
	float m_fEntityMorale;			//мораль

	//максимальные величины
	//	float m_fSatietyMax;
	float m_fPowerMax;
	float m_fRadiationMax;
	float m_fPsyHealthMax;

	float m_fEntityMoraleMax;

	//величины изменения параметров на каждом обновлении
	float m_fDeltaHealth;
	float m_fDeltaPower;
	float m_fDeltaRadiation;
	float m_fDeltaPsyHealth;

	float m_fDeltaCircumspection;
	float m_fDeltaEntityMorale;

	struct SConditionChangeV
	{
		float			m_fV_Radiation;
		float			m_fV_PsyHealth;
		float			m_fV_Circumspection;
		float			m_fV_EntityMorale;
		float			m_fV_RadiationHealth;
		float			m_fV_Bleeding;
		float			m_fV_WoundIncarnation;
		float			m_fV_HealthRestore;
		void			load(LPCSTR sect, LPCSTR prefix);
	};
	
	SConditionChangeV m_change_v;

	float				m_fMinWoundSize;
	bool				m_bIsBleeding;		//есть кровотечение


	//части хита, затрачиваемые на уменьшение здоровья и силы
	float				m_fHealthHitPart;
	float				m_fPowerHitPart;

	//потеря здоровья от последнего хита
	float				m_fHealthLost;

	float				m_fKillHitTreshold;
	float				m_fLastChanceHealth;
	float				m_fInvulnerableTime;
	float				m_fInvulnerableTimeDelta;
	//для отслеживания времени 
	u64					m_iLastTimeCalled;
	float				m_fDeltaTime;
	//кто нанес последний хит
	CObject*			m_pWho;
	u16					m_iWhoID;

	//для передачи параметров из DamageManager
	float				m_fArmorDamageBoneScale;
	float				m_fPierceDamageBoneScale;

	float				m_limping_threshold;

	bool				m_bTimeValid;
	bool				m_bCanBeHarmed;
	BOOSTER_MAP			m_booster_influences;

public:
	virtual void					reinit					();
	
	IC const	float				fdelta_time				() const 	{return		(m_fDeltaTime);				}
	IC const	WOUND_VECTOR&		wounds					() const	{return		(m_WoundVector);			}
	IC float&						radiation				()			{return		(m_fRadiation);				}
	IC float&						armor_damage_bone_scale	()			{return		(m_fArmorDamageBoneScale);	}
	IC float&						pierce_damage_bone_scale()			{return		(m_fPierceDamageBoneScale);	}
	IC SConditionChangeV&			change_v				()			{return		(m_change_v);				}

private:
	static HitImmunity::HitTypeSVec		HitTypeHeadPart;
	static HitImmunity::HitTypeSVec		HitTypeScale;

	static float						m_fMeleeOnPierceDamageMultiplier;
	static float						m_fMeleeOnPierceArmorDamageFactor;

	static CPowerDependency				ExplDamageResistance;
	static CPowerDependency				ArmorDamageResistance;

	static CPowerDependency				AnomalyDamageThreshold;
	static CPowerDependency				AnomalyDamageResistance;
	static CPowerDependency				ProtectionDamageResistance;

	template <typename T>
	float								GearProtectionEffect					(T gear, const ALife::EHitType& hit_type, float damage, bool head);
	template <typename T>
	float								GearExplEffect							(T gear, float damage, float protection, CInventoryOwner* io, bool head);


public:
	static CPowerDependency				StrikeDamageThreshold;
	static CPowerDependency				StrikeDamageResistance;
	static void							loadStaticData							();
};
