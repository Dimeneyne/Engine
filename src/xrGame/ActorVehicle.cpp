#include "stdafx.h"
#pragma hdrstop

#include "actor.h"
#include "../xrEngine/CameraBase.h"

#include "ActorEffector.h"
#include "holder_custom.h"
#ifdef DEBUG
#include "PHDebug.h"
#endif
#include "alife_space.h"
#include "hit.h"
#include "PHDestroyable.h"
#include "Car.h"
#include "../Include/xrRender/Kinematics.h"
//#include "PHShellSplitter.h"

#include "actor_anim_defs.h"
#include "game_object_space.h"
#include "characterphysicssupport.h"
#include "inventory.h"

#include "pch_script.h"
#include "game_object_space.h"
#include "script_callback_ex.h"
#include "script_game_object.h"

void CActor::attach_Vehicle(CHolderCustom* vehicle)
{
#ifdef ENABLE_CAR
	if(!vehicle) return;
	if(m_holder) return;
	CCar* car = smart_cast<CCar*>(vehicle);
	if (!car)
		return;

	if(!vehicle->attach_Actor(this)){
		return;
	}

	IRenderVisual *pVis = Visual();
	IKinematicsAnimated* V		= smart_cast<IKinematicsAnimated*>(pVis); R_ASSERT(V);
	IKinematics* pK = smart_cast<IKinematics*>(pVis);
	// temp play animation
	
	u16 anim_type					= car->DriverAnimationType();
	SVehicleAnimCollection& anims	= m_vehicle_anims->m_vehicles_type_collections[anim_type];
	V->PlayCycle					(anims.idles[0],FALSE);

	ResetCallbacks					();
	u16 head_bone					= pK->LL_BoneID("bip01_head");
	pK->LL_GetBoneInstance			(u16(head_bone)).set_callback		(bctPhysics, VehicleHeadCallback,this);

	character_physics_support		()->movement()->DestroyCharacter();
	mstate_wishful					= 0;
	m_holder = vehicle;
	m_holderID = car->ID();

	SetWeaponHideState				(INV_STATE_CAR, true);

	CStepManager::on_animation_start(MotionID(), 0);

	//Alundaio
	this->callback(GameObject::eAttachVehicle)(car->lua_game_object());
	//-Alundaio
#endif
}

void CActor::detach_Vehicle()
{
	if(!m_holder) return;
	CCar* car=smart_cast<CCar*>(m_holder);
	if(!car)return;

	//CPHShellSplitterHolder*sh= car->PPhysicsShell()->SplitterHolder();
	//if(sh)
	//	sh->Deactivate();
	car->PPhysicsShell()->SplitterHolderDeactivate();

	if(!character_physics_support()->movement()->ActivateBoxDynamic(0))
	{
		//if(sh)sh->Activate();
		car->PPhysicsShell()->SplitterHolderActivate();
		return;
	}
	//if(sh)
	//	sh->Activate();
	car->PPhysicsShell()->SplitterHolderActivate();
	m_holder->detach_Actor();//
	
	//Alundaio
#ifdef ENABLE_CAR
	this->callback(GameObject::eDetachVehicle)(car->lua_game_object());
#endif
	//-Alundaio

	character_physics_support()->movement()->SetPosition(m_holder->ExitPosition());
	character_physics_support()->movement()->SetVelocity(m_holder->ExitVelocity());

	r_model_yaw=-m_holder->Camera()->yaw;
	r_torso.yaw=r_model_yaw;
	r_model_yaw_dest=r_model_yaw;
	m_holder=NULL;
	SetCallbacks		();
	IKinematicsAnimated* V= smart_cast<IKinematicsAnimated*>(Visual()); R_ASSERT(V);
	V->PlayCycle		(m_anims->m_normal.legs_idle);
	V->PlayCycle		(m_anims->m_normal.m_torso_idle);
	m_holderID=u16(-1);

//.	SetWeaponHideState(whs_CAR, FALSE);
	SetWeaponHideState(INV_STATE_CAR, false);
}

bool CActor::use_Vehicle(CHolderCustom* object)
{
	
//	CHolderCustom* vehicle=smart_cast<CHolderCustom*>(object);
	CHolderCustom* vehicle=object;
	Fvector center;
	Center(center);
	if(m_holder){
		if(!vehicle&& m_holder->Use(Device.camera.position, Device.camera.direction,center)) detach_Vehicle();
		else{ 
			if(m_holder==vehicle)
				if(m_holder->Use(Device.camera.position, Device.camera.direction,center))detach_Vehicle();
		}
		return true;
	}else{
		if(vehicle)
		{
			if( vehicle->Use(Device.camera.position, Device.camera.direction,center))
			{
				if (pCamBobbing)
				{
					Cameras().RemoveCamEffector(eCEBobbing);
					pCamBobbing = NULL;
				}

				attach_Vehicle(vehicle);
			}
			else
			{
				//Alundaio
#ifdef ENABLE_CAR
				CCar * car= smart_cast<CCar*>(vehicle);
				this->callback(GameObject::eUseVehicle)(car->lua_game_object() );
#endif
				//-Alundaio
			}

			return true;
		}
		return false;
	}
}

void CActor::on_requested_spawn(CObject *object)
{
	CCar * car= smart_cast<CCar*>(object);
	attach_Vehicle(car);
}