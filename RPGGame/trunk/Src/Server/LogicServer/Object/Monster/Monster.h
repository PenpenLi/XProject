﻿#ifndef __MONSTER_H__
#define __MONSTER_H__

#include "Server/LogicServer/Object/Actor.h"

class Monster : public Actor
{
public:
	LUNAR_DECLARE_CLASS(Monster);

public:
	Monster();
	virtual ~Monster();
	void Init(int nObjID, int nConfID, const char* psName);

public:
	//virtual AStarPathFind* GetAStar() { return &m_oAStar; }

private:
	//AStarPathFind m_oAStar;	
	DISALLOW_COPY_AND_ASSIGN(Monster);


/////////////////lua export////////////
public:
	int MoveTo(lua_State* pState);
};

#endif