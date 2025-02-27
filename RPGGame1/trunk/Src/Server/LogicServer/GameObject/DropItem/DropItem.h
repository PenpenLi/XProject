﻿#ifndef __DROPITEM_H__
#define __DROPITEM_H__

#include "Server/LogicServer/GameObject/Object.h"

class DropItem : public Object
{
public:
	LUNAR_DECLARE_CLASS(DropItem);

public:
	DropItem();
	virtual ~DropItem();
	void Init(int64_t nObjID, int nConfID, const char* psName, int nAliveTime, int nCamp);

public:
	virtual void Update(int64_t nNowMS);

private:
	int m_nDisappearTime;

private:
	DISALLOW_COPY_AND_ASSIGN(DropItem);


/////////////////Lua export////////////
public:
};

#endif