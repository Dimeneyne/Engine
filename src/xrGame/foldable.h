#pragma once
#include "module.h"

class CFoldable : public CModule
{
public:
										CFoldable								(CGameObject* obj) : CModule(obj) {}

private:
	bool								m_status								= false;
	
	void								on_status_change					C$	(bool new_status);

	float								aboba								O$	(EEventTypes type, void* data, int param);

public:
	void								setStatus								(bool status);
	
	bool								getStatus							C$	()		{ return m_status; }
};