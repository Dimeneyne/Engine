#include "stdafx.h"
#include "player_hud.h"
#include "HudItem.h"
#include "ui_base.h"
#include "actor.h"
#include "physic_item.h"
#include "static_cast_checked.hpp"
#include "actoreffector.h"
#include "../xrEngine/IGame_Persistent.h"
#include "WeaponMagazined.h"
#include "weapon_hud.h"

#define TENDTO_SPEED         1.0f     // Модификатор силы инерции (больше - чувствительней)
#define TENDTO_SPEED_AIM     1.0f     // (Для прицеливания)
#define TENDTO_SPEED_RET     5.0f     // Модификатор силы отката инерции (больше - быстрее)
#define TENDTO_SPEED_RET_AIM 5.0f     // (Для прицеливания)
#define INERT_MIN_ANGLE      0.0f     // Минимальная сила наклона, необходимая для старта инерции
#define INERT_MIN_ANGLE_AIM  3.5f     // (Для прицеливания)

// Пределы смещения при инерции (лево / право / верх / низ)
#define ORIGIN_OFFSET        0.04f,  0.04f,  0.04f, 0.02f
#define ORIGIN_OFFSET_AIM    0.015f, 0.015f, 0.01f, 0.005f

player_hud* g_player_hud = NULL;

float CalcMotionSpeed(const shared_str& anim_name)
{
	return 1.0f;
}

void update_bones(IKinematics* model)
{
	IKinematicsAnimated* ka				= model->dcast_PKinematicsAnimated();
	if (ka)
	{
		ka->UpdateTracks				();
		model->CalculateBones_Invalidate();
		model->CalculateBones			(TRUE);
	}
}

player_hud_motion* player_hud_motion_container::find_motion(const shared_str& name)
{
	for (auto& a : m_anims)
		if (a.m_alias_name == name)
			return &a;
	return nullptr;
}

void player_hud_motion_container::load(IKinematicsAnimated* model, LPCSTR sect)
{
	CInifile::Sect& _sect		= pSettings->r_section(sect);
	CInifile::SectCIt _b		= _sect.Data.begin();
	CInifile::SectCIt _e		= _sect.Data.end();
	player_hud_motion* pm		= NULL;
	
	string512					buff;
	MotionID					motion_ID;

	for(;_b!=_e;++_b)
	{
		if(strstr(_b->first.c_str(), "anm_")==_b->first.c_str())
		{
			const shared_str& anm	= _b->second;
			m_anims.resize			(m_anims.size()+1);
			pm						= &m_anims.back();
			//base and alias name
			pm->m_alias_name		= _b->first;
			
			if(_GetItemCount(anm.c_str())==1)
			{
				pm->m_base_name			= anm;
				pm->m_additional_name	= anm;
				pm->m_anim_speed = 1.f;
			}else
			{
				R_ASSERT2(_GetItemCount(anm.c_str())<=3, anm.c_str());
				string512				str_item;
				_GetItem(anm.c_str(),0,str_item);
				pm->m_base_name			= str_item;

				_GetItem(anm.c_str(),1,str_item);
				pm->m_additional_name = (strlen(str_item) > 0) ? pm->m_additional_name = str_item : pm->m_base_name;
				
				_GetItem(anm.c_str(), 2, str_item);
				pm->m_anim_speed = strlen(str_item) > 0	 ? atof(str_item) : 1.f;
			}

			//and load all motions for it

			for(u32 i=0; i<=8; ++i)
			{
				if(i==0)
					xr_strcpy				(buff,pm->m_base_name.c_str());		
				else
					xr_sprintf				(buff,"%s%d",pm->m_base_name.c_str(),i);		

				motion_ID				= model->ID_Cycle_Safe(buff);
				if(motion_ID.valid())
				{
					pm->m_animations.resize			(pm->m_animations.size()+1);
					pm->m_animations.back().mid		= motion_ID;
					pm->m_animations.back().name	= buff;
#ifdef DEBUG
//					Msg(" alias=[%s] base=[%s] name=[%s]",pm->m_alias_name.c_str(), pm->m_base_name.c_str(), buff);
#endif // #ifdef DEBUG
				}
			}
			R_ASSERT2(pm->m_animations.size(),make_string("motion not found [%s]", pm->m_base_name.c_str()).c_str());
		}
	}
}

void attachable_hud_item::set_bone_visible(const shared_str& bone_name, BOOL bVisibility, BOOL bSilent)
{
	u16  bone_id;
	BOOL bVisibleNow;
	bone_id			= m_model->LL_BoneID			(bone_name);
	if(bone_id==BI_NONE)
	{
		if (bSilent)
			return;
		R_ASSERT2(0, make_string("model [%s] has no bone [%s]", pSettings->r_string(m_object_section, "visual"), bone_name.c_str()).c_str());
	}
	bVisibleNow		= m_model->LL_GetBoneVisible	(bone_id);
	if(bVisibleNow!=bVisibility)
		m_model->LL_SetBoneVisible	(bone_id,bVisibility, TRUE);
}

void attachable_hud_item::update(bool bForce)
{
	if (!bForce && m_upd_firedeps_frame == Device.dwFrame) return;
	m_parent->calc_transform		(m_attach_place_idx, m_item_attach, m_transform);
	m_parent_hud_item->UpdateSlotsTransform();
	m_upd_firedeps_frame			= Device.dwFrame;
}

void attachable_hud_item::setup_firedeps(firedeps& fd)
{
	update												(false);
	Fmatrix ftrans										= static_cast<Fmatrix>(m_transform);
	if (m_measures.m_prop_flags.test(hud_item_measures::e_fire_point))
	{
		Fmatrix& fire_mat								= m_model->LL_GetTransform(m_measures.m_fire_bone);
		fire_mat.transform_tiny							(fd.vLastFP, m_measures.m_fire_point_offset);
		ftrans.transform_tiny							(fd.vLastFP);
		
		Fvector fire_dir								= vForward;
		ftrans.transform_dir							(fire_dir);

		fd.m_FireParticlesXForm.identity				();
		fd.m_FireParticlesXForm.k.set					(fire_dir);
		Fvector::generate_orthonormal_basis_normalized	(fd.m_FireParticlesXForm.k,
														fd.m_FireParticlesXForm.j, 
														fd.m_FireParticlesXForm.i);

		VERIFY											(_valid(fd.vLastFP));
		VERIFY											(_valid(fd.m_FireParticlesXForm));
	}

	if (m_measures.m_prop_flags.test(hud_item_measures::e_fire_point2))
	{
		Fmatrix& fire_mat			= m_model->LL_GetTransform(m_measures.m_fire_bone2);
		fire_mat.transform_tiny		(fd.vLastFP2,m_measures.m_fire_point2_offset);
		ftrans.transform_tiny		(fd.vLastFP2);
		VERIFY						(_valid(fd.vLastFP2));
	}
}

bool  attachable_hud_item::need_renderable()
{
	return m_parent_hud_item->need_renderable();
}

void attachable_hud_item::render()
{
	::Render->set_Transform		(m_transform);
	::Render->add_Visual		(m_model->dcast_RenderVisual());
	debug_draw_firedeps			();
	m_parent_hud_item->render_hud_mode();
}

bool attachable_hud_item::render_item_ui_query()
{
	return m_parent_hud_item->render_item_3d_ui_query();
}

void attachable_hud_item::render_item_ui()
{
	m_parent_hud_item->render_item_3d_ui();
}

void hud_item_measures::load(LPCSTR hud_section, IKinematics* K)
{
	R_ASSERT2(pSettings->line_exist(hud_section, "fire_point") == pSettings->line_exist(hud_section, "fire_bone"), hud_section);
	R_ASSERT2(pSettings->line_exist(hud_section, "fire_point2") == pSettings->line_exist(hud_section, "fire_bone2"), hud_section);
	R_ASSERT2(pSettings->line_exist(hud_section, "shell_point") == pSettings->line_exist(hud_section, "shell_bone"), hud_section);

	shared_str bone_name;
	m_prop_flags.set(e_fire_point, pSettings->line_exist(hud_section, "fire_bone"));
	if (m_prop_flags.test(e_fire_point))
	{
		bone_name = pSettings->r_string(hud_section, "fire_bone");
		m_fire_bone = K->LL_BoneID(bone_name);
		m_fire_point_offset = pSettings->r_fvector3(hud_section, "fire_point");
	}
	else
		m_fire_point_offset.set(0, 0, 0);

	m_prop_flags.set(e_fire_point2, pSettings->line_exist(hud_section, "fire_bone2"));
	if (m_prop_flags.test(e_fire_point2))
	{
		bone_name = pSettings->r_string(hud_section, "fire_bone2");
		m_fire_bone2 = K->LL_BoneID(bone_name);
		m_fire_point2_offset = pSettings->r_fvector3(hud_section, "fire_point2");
	}
	else
		m_fire_point2_offset.set(0, 0, 0);

	// Настройки стрейфа (боковая ходьба)
	Fvector vDefStrafeValue;

	//--> Смещение в стрейфе
	m_strafe_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, hud_section, "strafe_hud_offset_pos", vDefStrafeValue);
	m_strafe_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, hud_section, "strafe_hud_offset_rot", vDefStrafeValue);

	//--> Поворот в стрейфе
	m_strafe_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, hud_section, "strafe_aim_hud_offset_pos", vDefStrafeValue);
	m_strafe_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, hud_section, "strafe_aim_hud_offset_rot", vDefStrafeValue);

	//--> Параметры стрейфа
	bool bStrafeEnabled = READ_IF_EXISTS(pSettings, r_BOOL, hud_section, "strafe_enabled", false);
	bool bStrafeEnabled_aim = READ_IF_EXISTS(pSettings, r_BOOL, hud_section, "strafe_aim_enabled", false);
	float fFullStrafeTime = READ_IF_EXISTS(pSettings, r_float, hud_section, "strafe_transition_time", .5f);
	float fFullStrafeTime_aim = READ_IF_EXISTS(pSettings, r_float, hud_section, "strafe_aim_transition_time", .5f);
	float fStrafeCamLFactor = READ_IF_EXISTS(pSettings, r_float, hud_section, "strafe_cam_limit_factor", 0.5f);
	float fStrafeCamLFactor_aim = READ_IF_EXISTS(pSettings, r_float, hud_section, "strafe_cam_limit_aim_factor", 1.0f);
	float fStrafeMinAngle = READ_IF_EXISTS(pSettings, r_float, hud_section, "strafe_cam_min_angle", 0.0f);
	float fStrafeMinAngle_aim = READ_IF_EXISTS(pSettings, r_float, hud_section, "strafe_cam_aim_min_angle", 7.0f);

	//--> (Data 1)
	m_strafe_offset[2][0].set((bStrafeEnabled ? 1.0f : 0.0f), fFullStrafeTime, NULL); // normal
	m_strafe_offset[2][1].set((bStrafeEnabled_aim ? 1.0f : 0.0f), fFullStrafeTime_aim, NULL); // aim-GL

	//--> (Data 2)
	m_strafe_offset[3][0].set(fStrafeCamLFactor, fStrafeMinAngle, NULL); // normal
	m_strafe_offset[3][1].set(fStrafeCamLFactor_aim, fStrafeMinAngle_aim, NULL); // aim-GL

	// Загрузка параметров смещения / инерции
	m_inertion_params.m_tendto_speed = READ_IF_EXISTS(pSettings, r_float, hud_section, "inertion_tendto_speed", TENDTO_SPEED);
	m_inertion_params.m_tendto_speed_aim = READ_IF_EXISTS(pSettings, r_float, hud_section, "inertion_tendto_aim_speed", TENDTO_SPEED_AIM);
	m_inertion_params.m_tendto_ret_speed = READ_IF_EXISTS(pSettings, r_float, hud_section, "inertion_tendto_ret_speed", TENDTO_SPEED_RET);
	m_inertion_params.m_tendto_ret_speed_aim = READ_IF_EXISTS(pSettings, r_float, hud_section, "inertion_tendto_ret_aim_speed", TENDTO_SPEED_RET_AIM);

	m_inertion_params.m_min_angle = READ_IF_EXISTS(pSettings, r_float, hud_section, "inertion_min_angle", INERT_MIN_ANGLE);
	m_inertion_params.m_min_angle_aim = READ_IF_EXISTS(pSettings, r_float, hud_section, "inertion_min_angle_aim", INERT_MIN_ANGLE_AIM);

	m_inertion_params.m_offset_LRUD = READ_IF_EXISTS(pSettings, r_fvector4, hud_section, "inertion_offset_LRUD", Fvector4().set(ORIGIN_OFFSET));
	m_inertion_params.m_offset_LRUD_aim = READ_IF_EXISTS(pSettings, r_fvector4, hud_section, "inertion_offset_LRUD_aim", Fvector4().set(ORIGIN_OFFSET_AIM));
}

attachable_hud_item::~attachable_hud_item()
{
	IRenderVisual* v			= m_model->dcast_RenderVisual();
	::Render->model_Delete		(v);
	m_model						= NULL;
}

void attachable_hud_item::load(LPCSTR hud_section, LPCSTR object_section)
{
	m_hud_section				= hud_section;
	m_object_section			= object_section;

	// Visual
	LPCSTR visual_name			= READ_IF_EXISTS(pSettings, r_string, hud_section, "item_visual", pSettings->r_string(object_section, "visual"));
	m_model						= smart_cast<IKinematics*>(::Render->model_Create(visual_name));

	m_attach_place_idx			= pSettings->r_u16(hud_section, "attach_place_idx");
	m_auto_attach				= pSettings->r_bool_ex(hud_section, "auto_hands_attach", false);
	m_measures.load				(hud_section, m_model);
	
	m_item_attach.setHPBv				(static_cast<Dvector>(pSettings->r_fvector3d2r(hud_section, "item_orientation")));
	m_item_attach.translate_over		(static_cast<Dvector>(pSettings->r_fvector3(hud_section, "item_position")));
	
	if (!m_auto_attach)
		m_hands_attach.setOffset(
			static_cast<Dvector>(pSettings->r_fvector3(hud_section, "hands_position")),
			static_cast<Dvector>(pSettings->r_fvector3d2r(hud_section, "hands_orientation"))
		);
}

u32 attachable_hud_item::anim_play(const shared_str& anim_name, BOOL bMixIn, const CMotionDef*& md, u8& rnd_idx, bool ignore_anm_type)
{
	R_ASSERT				(strstr(anim_name.c_str(),"anm_")== anim_name.c_str());

	player_hud_motion* anm	= m_hand_motions.find_motion(anim_name);
	R_ASSERT2				(anm, make_string("model [%s] has no motion alias defined [%s]", m_hud_section.c_str(), anim_name.c_str()).c_str());
	R_ASSERT2				(anm->m_animations.size(), make_string("model [%s] has no motion defined in motion_alias [%s]", pSettings->r_string(m_object_section, "visual"), anim_name.c_str()).c_str());
	
	float speed = anm->m_anim_speed;

	rnd_idx					= (u8)Random.randI(anm->m_animations.size()) ;
	const motion_descr& M	= anm->m_animations[ rnd_idx ];

	u32 ret					= g_player_hud->anim_play(m_attach_place_idx, M.mid, bMixIn, md, speed);
	
	if(m_model->dcast_PKinematicsAnimated())
	{
		IKinematicsAnimated* ka			= m_model->dcast_PKinematicsAnimated();

		shared_str item_anm_name;
		if(anm->m_base_name!=anm->m_additional_name)
			item_anm_name = anm->m_additional_name;
		else
			item_anm_name = M.name;

		LPCSTR anm_type					= (ignore_anm_type) ? "" : m_parent_hud_item->anmType();
		MotionID M2						= ka->ID_Cycle_Safe(shared_str().printf("%s%s", item_anm_name.c_str(), anm_type));
		if (!M2.valid())
		{
			M2							= ka->ID_Cycle_Safe(item_anm_name);
			if (!M2.valid())
			{
				M2						= ka->ID_Cycle_Safe(shared_str().printf("idle%s", anm_type));
				if (!M2.valid())
					M2					= ka->ID_Cycle_Safe("idle");
			}
		}
		R_ASSERT3(M2.valid(), "model has no motion [idle] ", pSettings->r_string(m_object_section, "visual"));

		u16 root_id						= m_model->LL_GetBoneRoot();
		CBoneInstance& root_binst		= m_model->LL_GetBoneInstance(root_id);
		root_binst.set_callback_overwrite(TRUE);
		root_binst.mTransform.identity	();

		u16 pc							= ka->partitions().count();
		for(u16 pid=0; pid<pc; ++pid)
		{
			CBlend* B					= ka->PlayCycle(pid, M2, FALSE);
			R_ASSERT					(B);
			B->speed					*= speed;
		}

		m_model->CalculateBones_Invalidate	();
	}

	R_ASSERT2		(m_parent_hud_item, "parent hud item is NULL");
	CPhysicItem&	parent_object = m_parent_hud_item->object();
	//R_ASSERT2		(parent_object, "object has no parent actor");
	//CObject*		parent_object = static_cast_checked<CObject*>(&m_parent_hud_item->object());

	if (parent_object.H_Parent() == Level().CurrentControlEntity())
	{
		CActor* current_actor	= static_cast_checked<CActor*>(Level().CurrentControlEntity());
		VERIFY					(current_actor);
		CEffectorCam* ec		= current_actor->Cameras().GetCamEffector(eCEWeaponAction);

		if (ec)
			current_actor->Cameras().RemoveCamEffector(eCEWeaponAction);
	
		string_path			ce_path;
		string_path			anm_name;
		strconcat			(sizeof(anm_name),anm_name,"camera_effects\\weapon\\", M.name.c_str(),".anm");
		if (FS.exist( ce_path, "$game_anims$", anm_name))
		{
			CAnimatorCamEffector* e		= xr_new<CAnimatorCamEffector>();
			e->SetType					(eCEWeaponAction);
			e->SetHudAffect				(false);
			e->SetCyclic				(false);
			e->Start					(anm_name);
			current_actor->Cameras().AddCamEffector(e);
		}
	}
	return ret;
}

player_hud::player_hud()
{
	//Movement Layers
	for (int i = 0; i < move_anms_end; i++)
		for (int j = 0; j < state_anms_end; j++)
			for (int k = 0; k < pose_anms_end; k++)
			{
				char temp[20];
				string512 tmp;
				strconcat(sizeof(temp), temp, "movement_layer_", std::to_string(i).c_str());
				R_ASSERT2(pSettings->line_exist("hud_movement_layers", temp), make_string("Missing definition for [hud_movement_layers] %s", temp));
				LPCSTR layer_def = pSettings->r_string("hud_movement_layers", temp);
				R_ASSERT2(_GetItemCount(layer_def) > 0, make_string("Wrong definition for [hud_movement_layers] %s", temp));

				auto anm = &m_movement_layers[i][j][k];
				_GetItem(layer_def, 0, tmp);
				anm->Load(tmp);

				_GetItem(layer_def, 1, tmp);
				anm->anm->Speed() = (atof(tmp) ? atof(tmp) : 1.f);
				anm->anm->Speed() *= pSettings->r_float("hud_movement_layers", shared_str().printf("pose_speed_k_%d", k).c_str());

				_GetItem(layer_def, 2, tmp);
				anm->m_power = (atof(tmp) ? atof(tmp) : 1.f);
				anm->m_power *= pSettings->r_float("hud_movement_layers", shared_str().printf("weapon_state_power_k_%d", j).c_str());
				float pose_power_k = pSettings->r_float("hud_movement_layers", shared_str().printf("pose_power_k_%d", k).c_str());
				anm->m_power *= (i) ? pose_power_k : (1.f / pose_power_k);
			}
}

player_hud::~player_hud()
{
	for (auto& model : m_model)
	{
		IRenderVisual* v				= model->dcast_RenderVisual();
		::Render->model_Delete			(v);
		model							= nullptr;
	}

	delete_data							(m_pool);
}

void player_hud::load(const shared_str& player_hud_sect)
{
	if (player_hud_sect == m_sect_name)
		return;

	for (auto& model : m_model)
	{
		if (model)
		{
			IRenderVisual* v				= model->dcast_RenderVisual();
			::Render->model_Delete			(v);
		}
	}

	m_sect_name							= player_hud_sect;
	auto model_name						= pSettings->r_string(player_hud_sect, "visual");
	for (auto& model : m_model)
		model							= smart_cast<IKinematicsAnimated*>(::Render->model_Create(model_name));

	// hides the unused arm meshes
	u16 l_arm							= m_model[0]->dcast_PKinematics()->LL_BoneID("l_clavicle");
	u16 r_arm							= m_model[1]->dcast_PKinematics()->LL_BoneID("r_clavicle");

	m_model[0]->dcast_PKinematics()->LL_SetBoneVisible(l_arm, FALSE, TRUE);
	m_model[1]->dcast_PKinematics()->LL_SetBoneVisible(r_arm, FALSE, TRUE);

	auto CR$ _sect						= pSettings->r_section(player_hud_sect);
	for(auto& line : _sect.Data)
	{
		if (strstr(line.first.c_str(), "ancor_") == line.first.c_str())
		{
			auto CR$ _bone				= line.second;
			m_ancors.push_back			(m_model[0]->dcast_PKinematics()->LL_BoneID(_bone));
		}
	}

	if (!m_attached_items[0] && !m_attached_items[1])
	{
		for (auto& model : m_model)
			model->PlayCycle			("hand_idle_doun");
	}
	else
	{
		if (m_attached_items[1])
			m_attached_items[1]->m_parent_hud_item->on_a_hud_attach();
		if (m_attached_items[0])
			m_attached_items[0]->m_parent_hud_item->on_a_hud_attach();
	}

	for (auto& model : m_model)
	{
		model->dcast_PKinematics()->CalculateBones_Invalidate();
		model->dcast_PKinematics()->CalculateBones(TRUE);
	}
}

bool player_hud::render_item_ui_query()
{
	bool res = false;
	if(m_attached_items[0])
		res |= m_attached_items[0]->render_item_ui_query();

	if(m_attached_items[1])
		res |= m_attached_items[1]->render_item_ui_query();

	return res;
}

void player_hud::render_item_ui()
{
	if(m_attached_items[0])
		m_attached_items[0]->render_item_ui();

	if(m_attached_items[1])
		m_attached_items[1]->render_item_ui();
}

void player_hud::render_hud()
{
	bool b_r0							= (m_attached_items[0] && m_attached_items[0]->need_renderable());
	bool b_r1							= (m_attached_items[1] && m_attached_items[1]->need_renderable());
	if (!b_r0 && !b_r1)
		return;

	for (int i = 0; i < 2; ++i)
	{
		::Render->set_Transform			(m_transform[i]);
		::Render->add_Visual			(m_model[i]->dcast_RenderVisual());
	}

	if (m_attached_items[0])
		m_attached_items[0]->render		();

	if (m_attached_items[1])
		m_attached_items[1]->render		();
}

#include "../xrEngine/motion.h"

u32 player_hud::motion_length(const shared_str& anim_name, const shared_str& hud_name, const shared_str& section, const CMotionDef*& md)
{
	float speed						= CalcMotionSpeed(anim_name);
	attachable_hud_item* pi			= create_hud_item(*hud_name, *section);
	player_hud_motion*	pm			= pi->m_hand_motions.find_motion(anim_name);
	if(!pm)
		return						100; // ms TEMPORARY
	R_ASSERT2						(pm, 
		make_string	("hudItem model [%s] has no motion with alias [%s]", hud_name.c_str(), anim_name.c_str() ).c_str() 
		);
	return motion_length			(pm->m_animations[0].mid, md, speed);
}

u32 player_hud::motion_length(const MotionID& M, const CMotionDef*& md, float speed)
{
	md					= m_model[0]->LL_GetMotionDef(M);
	VERIFY				(md);
	if (md->flags & esmStopAtEnd) 
	{
		CMotion*			motion		= m_model[0]->LL_GetRootMotion(M);
		return				iFloor( 0.5f + 1000.f*motion->GetLength() / (md->Dequantize(md->speed) * speed) );
	}
	return					0;
}

void player_hud::update(Fmatrix CR$ cam_trans)
{
	for (int i = 0; i < 2; ++i)
	{
		m_transform[i]					= static_cast<Dmatrix>(cam_trans);
		update_bones					(m_model[i]->dcast_PKinematics());
	}

	for (int i = 0; i < 2; i++)
		if (m_attached_items[i])
			update_bones				(m_attached_items[i]->m_model);

	for (auto& i : m_movement_layers)
		for (auto& j : i)
			for (auto& k : j)
			{
				auto anm				= &k;

				if (!anm || !anm->anm || (!anm->active && anm->blend_amount[0] == 0.f && anm->blend_amount[1] == 0.f))
					continue;

				if (anm->active)
				{
					if (m_attached_items[0])
					{
						anm->blend_amount[0] += Device.fTimeDelta / .4f;
						if (!m_attached_items[1])
							anm->blend_amount[1] += Device.fTimeDelta / .4f;
					}

					if (m_attached_items[1])
					{
						anm->blend_amount[1] += Device.fTimeDelta / .4f;
						if (!m_attached_items[0])
							anm->blend_amount[0] += Device.fTimeDelta / .4f;
					}
				}
				else
				{
					anm->blend_amount[0] -= Device.fTimeDelta / .4f;
					anm->blend_amount[1] -= Device.fTimeDelta / .4f;
				}

				clamp					(anm->blend_amount[0], 0.f, 1.f);
				clamp					(anm->blend_amount[1], 0.f, 1.f);

				if (anm->blend_amount[0] == 0.f && anm->blend_amount[1] == 0.f)
				{
					anm->Stop			(true);
					continue;
				}

				anm->anm->Update		(Device.fTimeDelta);

				if ((anm->blend_amount[0] == anm->blend_amount[1]) || (anm->blend_amount[0] > 0.f))
					for (auto& t : m_transform)
						t.mulB_43		(static_cast<Dmatrix>(anm->XFORM(0)));
			}

	for (int i = 0; i < 2; i++)
	{
		if (auto pi = m_attached_items[i])
		{
			pi->m_parent_hud_item->UpdateHudAdditional(m_transform[i]);
			m_transform[i].mulB_43(pi->m_hands_attach);
		}
	}

	if (!m_attached_items[1])
		m_transform[1]					= m_transform[0];

	for (auto& anm : m_script_layers)
	{
		if (!anm || !anm->anm || (!anm->active && anm->blend_amount == 0.f))
			continue;

		if (anm->active)
			anm->blend_amount			+= Device.fTimeDelta / .4f;
		else
			anm->blend_amount			-= Device.fTimeDelta / .4f;

		clamp							(anm->blend_amount, 0.f, 1.f);

		if (anm->blend_amount > 0.f)
		{
			if (anm->anm->bLoop || anm->anm->anim_param().t_current < anm->anm->anim_param().max_t)
				anm->anm->Update		(Device.fTimeDelta);
			else
				anm->Stop				(false);
		}
		else
		{
			anm->Stop					(true);
			continue;
		}

		auto blend						= static_cast<Dmatrix>(anm->XFORM());
		if (anm->m_part == 0 || anm->m_part == 2)
			m_transform[0].mulB_43		(blend);
		if (anm->m_part == 1 || anm->m_part == 2)
			m_transform[1].mulB_43		(blend);
	}

	if (m_attached_items[0])
		m_attached_items[0]->update		(true);

	if (m_attached_items[1])
		m_attached_items[1]->update		(true);
}

u32 player_hud::anim_play(u16 part, const MotionID& M, BOOL bMixIn, const CMotionDef*& md, float speed)
{
	u16 part_id							= u16_max;
	if (attached_item(0) && attached_item(1))
		part_id							= m_model[0]->partitions().part_id((part == 0) ? "right_hand" : "left_hand");
	
	auto play_impl = [M, bMixIn, speed](IKinematicsAnimated* model, int pid)
	{
		CBlend* B						= model->PlayCycle(pid, M, bMixIn);
		R_ASSERT						(B);
		B->speed						*= speed;
	};

	int pc								= m_model[0]->partitions().count();
	if (part_id == u16_max)
	{
		for (int pid = 0; pid < pc; ++pid)
		{
			if (pid == 0 || pid == 2)
				play_impl				(m_model[0], pid);
			if (pid == 0 || pid == 1)
				play_impl				(m_model[1], pid);
		}

		for (auto& model : m_model)
			model->dcast_PKinematics()->CalculateBones_Invalidate();
	}
	else if (part_id == 0 || part_id == 2)
	{
		for (int pid = 0; pid < pc; ++pid)
			if (pid != 1)
				play_impl				(m_model[0], pid);

		m_model[0]->dcast_PKinematics()->CalculateBones_Invalidate();
	}
	else if (part_id == 1)
	{
		for (int pid = 0; pid < pc; ++pid)
			if (pid != 2)
				play_impl				(m_model[1], pid);

		m_model[1]->dcast_PKinematics()->CalculateBones_Invalidate();
	}

	return								motion_length(M, md, speed);
}

attachable_hud_item* player_hud::create_hud_item(LPCSTR hud_section, LPCSTR object_section)
{
	for (auto itm : m_pool)
		if (itm->m_hud_section == hud_section && itm->m_object_section == object_section)
			return						itm;

	attachable_hud_item* res			= xr_new<attachable_hud_item>(this);
	res->load							(hud_section, object_section);
	res->m_hand_motions.load			(m_model[0], hud_section);
	m_pool.push_back					(res);

	return								res;
}

bool player_hud::allow_activation(CHudItem* item)
{
	if(m_attached_items[1])
		return m_attached_items[1]->m_parent_hud_item->CheckCompatibility(item);
	else
		return true;
}

void player_hud::attach_item(CHudItem* item)
{
	attachable_hud_item* pi				= create_hud_item(*item->HudSection(), *item->object().cNameSect());
	int item_idx						= pi->m_attach_place_idx;
	
	if (m_attached_items[item_idx] != pi)
	{
		if (m_attached_items[item_idx])
			m_attached_items[item_idx]->m_parent_hud_item->on_b_hud_detach();

		m_attached_items[item_idx]		= pi;
		pi->m_parent_hud_item			= item;

		if(item_idx==0 && m_attached_items[1])
			m_attached_items[1]->m_parent_hud_item->CheckCompatibility(item);

		item->on_a_hud_attach			();

		updateMovementLayerState		();
		item->UpdateHudBonesVisibility	();

		auto ao							= pi->m_parent_hud_item->object().getModule<MAddonOwner>();
		if (pi->m_auto_attach || ao)
		{
			pi->m_parent_hud_item->PlayHUDMotion("anm_idle", FALSE, 0, true);
			update_bones				(pi->m_model);
		}

		if (pi->m_auto_attach)
		{
			update_bones				(m_model[0]->dcast_PKinematics());
			u16 anchor_bone_id			= m_ancors[pi->m_attach_place_idx];
			auto& anchor_trans			= m_model[0]->dcast_PKinematics()->LL_GetTransform(anchor_bone_id);
			pi->m_hands_attach.mul		(static_cast<Dmatrix>(anchor_trans), pi->m_item_attach);
			pi->m_hands_attach.mulB_43	(static_cast<Dmatrix>(pi->m_model->LL_GetTransform_R(0)));
			pi->m_hands_attach.invert	();
		}

		if (ao)
			ao->calcSlotsBoneOffset		(pi);
	}
	else
		pi->m_parent_hud_item			= item;
}

void player_hud::detach_item_idx(u16 idx)
{
	if (!attached_item(idx))
		return;

	auto& hi							= m_attached_items[idx];
	hi->m_parent_hud_item->on_b_hud_detach();
	hi->m_parent_hud_item				= nullptr;
	m_pool.erase_data					(hi);
	hi									= nullptr;

	if (idx == 1)
	{
		if (m_attached_items[0])
			re_sync_anim				(2);
		else
			m_model[1]->PlayCycle		("hand_idle_doun");
	}
	else
	{
		if (m_attached_items[1])
		{
			//fix for a rare case where the right hand stays visible on screen after detaching the right hand's attached item
			player_hud_motion* pm		= m_attached_items[1]->m_hand_motions.find_motion("anm_idle");
			auto CR$ M					= pm->m_animations[0];
			m_model[0]->PlayCycle		(0, M.mid, false);
			m_model[0]->PlayCycle		(2, M.mid, false);
		}
		else
			m_model[0]->PlayCycle		("hand_idle_doun");
	}

	if (!m_attached_items[0] && !m_attached_items[1])
		for (auto& model : m_model)
			model->PlayCycle			("hand_idle_doun");
}

void player_hud::detach_item(CHudItem* item)
{
	if (!item->HudItemData())
		return;

	u16 item_idx						= item->HudItemData()->m_attach_place_idx;
	if (m_attached_items[item_idx] == item->HudItemData())
		detach_item_idx					(item_idx);
}

void player_hud::calc_transform(u16 attach_slot_idx, Dmatrix CR$ offset, Dmatrix& result)
{
	auto k								= m_model[attach_slot_idx]->dcast_PKinematics();
	auto anchor							= m_ancors[attach_slot_idx];
	auto ancor_matrix					= static_cast<Dmatrix>(k->LL_GetTransform(anchor));
	result.mul							(m_transform[attach_slot_idx], ancor_matrix);
	result.mulB_43						(offset);
}

bool player_hud::inertion_allowed()
{
	attachable_hud_item* hi = m_attached_items[0];
	if(hi)
	{
		bool res = ( hi->m_parent_hud_item->HudInertionEnabled() && hi->m_parent_hud_item->HudInertionAllowed() );
		return	res;
	}
	return true;
}

void player_hud::OnMovementChanged()
{
	if (!m_attached_items[0] && !m_attached_items[1])
		return;

	if (m_attached_items[0])
		m_attached_items[0]->m_parent_hud_item->onMovementChanged();
	if (m_attached_items[1])
		m_attached_items[1]->m_parent_hud_item->onMovementChanged();
	g_player_hud->updateMovementLayerState();
}

void player_hud::updateMovementLayerState()
{
	for (auto& i : m_movement_layers)
		for (auto& j : i)
			for (auto& k : j)
				k.Stop(false);

	eWeaponStateLayers wpn_state = eArmed;
	if (m_attached_items[0])
		if (auto wpn = m_attached_items[0]->m_parent_hud_item->object().cast_weapon())
			wpn_state = (wpn->ADS()) ? eADS : ((wpn->IsZoomed()) ? eAim : ((wpn->ArmedMode()) ? eArmed : eRelaxed));

	CEntity::SEntityState state;
	Actor()->g_State(state);
	ePoseLayers pose = (state.bCrouch) ? eCrouch : eStand;

	if (state.bSprint)
		m_movement_layers[eSprint][wpn_state][pose].Play();
	else if (Actor()->AnyMove())
	{
		if (isActorAccelerated(Actor()->MovingState(), Actor()->IsZoomADSMode()))
			m_movement_layers[eRun][wpn_state][pose].Play();
		else
			m_movement_layers[eWalk][wpn_state][pose].Play();
	}
	else
		m_movement_layers[eIdle][wpn_state][pose].Play();
}

script_layer* player_hud::playBlendAnm(shared_str CR$ name, u8 part, float speed, float power, bool bLooped, bool no_restart, bool full_blend)
{
	for (auto& anm : m_script_layers)
	{
		if (anm->m_name == name)
		{
			if (!no_restart)
			{
				anm->anm->Stop();
				anm->blend_amount = 0.f;
			}

			if (!anm->anm->IsPlaying())
				anm->anm->Play(bLooped);

			anm->anm->bLoop = bLooped;
			anm->m_part = part;
			anm->anm->Speed() = speed;
			anm->m_power = power;
			anm->active = true;

			if (full_blend)
				anm->blend_amount = 1.f;

			return anm.get();
		}
	}

	return m_script_layers.emplace_back(name, part, speed, power, bLooped, full_blend).get();
}

void player_hud::StopBlendAnm(shared_str CR$ name, bool bForce)
{
	for (auto& anm : m_script_layers)
	{
		if (anm->m_name == name)
		{
			anm->Stop(bForce);
			return;
		}
	}
}

//sync anim of other part to selected part (1 = sync to left hand anim; 2 = sync to right hand anim)
void player_hud::re_sync_anim(int part)
{
	u32 bc								= m_model[part == 1]->LL_PartBlendsCount(part);
	for (u32 bidx = 0; bidx < bc; ++bidx)
	{
		CBlend* BR						= m_model[part == 1]->LL_PartBlend(part, bidx);
		if (!BR)
			continue;

		MotionID M						= BR->motionID;

		int pc							= m_model[0]->partitions().count();
		for (int pid = 0; pid < pc; ++pid)
		{
			if (pid == 0)
			{
				CBlend* B				= m_model[0]->PlayCycle(0, M, TRUE);
				B->timeCurrent			= BR->timeCurrent;
				B->speed				= BR->speed;
				B						= m_model[1]->PlayCycle(0, M, TRUE);
				B->timeCurrent			= BR->timeCurrent;
				B->speed				= BR->speed;
			}
			else if (pid != part)
			{
				CBlend* B				= m_model[part != 1]->PlayCycle(pid, M, TRUE);
				B->timeCurrent			= BR->timeCurrent;
				B->speed				= BR->speed;
			}
		}
	}
}
