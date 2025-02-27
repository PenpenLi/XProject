﻿#include "MonsterMgr.h"
#include "Common/DataStruct/XTime.h"

LUNAR_IMPLEMENT_CLASS(MonsterMgr)
{
	LUNAR_DECLARE_METHOD(MonsterMgr, CreateMonster),
	//LUNAR_DECLARE_METHOD(MonsterMgr, RemoveMonster), 自动收集不需要主动删除
	LUNAR_DECLARE_METHOD(MonsterMgr, GetMonster),
	{0, 0}
};


MonsterMgr::MonsterMgr()
{
}

MonsterMgr::~MonsterMgr()
{
	MonsterIter iter = m_oMonsterIDMap.begin();
	for (iter; iter != m_oMonsterIDMap.end(); iter++)
	{
		SAFE_DELETE(iter->second);
	}
}

Monster* MonsterMgr::CreateMonster(int nID, int nConfID, const char* psName, int nAIID, int8_t nCamp)
{
	Monster* poMonster = GetMonsterByID(nID);
	if (poMonster != NULL)
	{
		XLog(LEVEL_ERROR, "CreateMonster error for monster id:%d exist\n", nID);
		return poMonster;
	}
	poMonster = XNEW(Monster);
	poMonster->Init(nID, nConfID, psName);
	m_oMonsterIDMap[nID] = poMonster;
	return poMonster;
}

void MonsterMgr::RemoveMonster(int nID)
{
	MonsterIter iter = m_oMonsterIDMap.find(nID);
	if (iter == m_oMonsterIDMap.end())
	{
		return;
	}
	Monster* poMonster = iter->second;
	if (poMonster->GetScene() != NULL)
	{
		XLog(LEVEL_ERROR, "Remove monster must leave scene first\n");
		return;
	}
	m_oMonsterIDMap.erase(iter);
	SAFE_DELETE(poMonster);
}

Monster* MonsterMgr::GetMonsterByID(int nID)
{
	MonsterIter iter = m_oMonsterIDMap.find(nID);
	if (iter != m_oMonsterIDMap.end())
	{
		return iter->second;
	}
	return NULL;
}

void MonsterMgr::Update(int64_t nNowMS)
{
	static int64_t nLastMSTime = 0;
	if (nNowMS - nLastMSTime < 30)
	{
		return;
	}
	nLastMSTime = nNowMS;

	int nMonsterCount = 0;
	MonsterIter iter = m_oMonsterIDMap.begin();
	MonsterIter iter_end = m_oMonsterIDMap.end();
	for (; iter != iter_end;)
	{
		Monster* poMonster = iter->second;
		if (poMonster->IsTime2Collect(nNowMS))
		{
			iter = m_oMonsterIDMap.erase(iter);
			LuaWrapper::Instance()->FastCallLuaRef<void, CNOTUSE>("OnObjCollected", 0, "ii", poMonster->GetID(), poMonster->GetType());
			SAFE_DELETE(poMonster);
			continue;
		}
		if (poMonster->GetScene() != NULL)
		{
			poMonster->Update(nNowMS);
		}
		nMonsterCount++;
		iter++;
	}	

	static int64_t nLastDumpTime = 0;
	if (nNowMS-nLastDumpTime >= 60000)
	{
		nLastDumpTime = nNowMS;
		XLog(LEVEL_INFO, "CPP current monster count=%d\n", nMonsterCount);
	}
}


///////////////// export to lua /////////////////
void RegClassMonster()
{
	REG_CLASS(Monster, false, NULL); 
	REG_CLASS(MonsterMgr, false, NULL); 
}

int MonsterMgr::CreateMonster(lua_State* pState)
{
	int nObjID = (int)luaL_checkinteger(pState, 1);
	int nConfID = (int)luaL_checkinteger(pState, 2);
	const char* psName = luaL_checkstring(pState, 3);

	Monster* poMonster = CreateMonster(nObjID, nConfID, psName, 0, 0);
	if (poMonster != NULL)
	{
		Lunar<Monster>::push(pState, poMonster);
		return 1;
	}
	return 0;
}

int MonsterMgr::RemoveMonster(lua_State* pState)
{
	int nObjID = (int)luaL_checkinteger(pState, 1);
	RemoveMonster(nObjID);
	return 0;
}

int MonsterMgr::GetMonster(lua_State* pState)
{
	int nObjID = (int)luaL_checkinteger(pState, 1);
	Monster* poMonster = GetMonsterByID(nObjID);
	if (poMonster != NULL)
	{
		Lunar<Monster>::push(pState, poMonster);
		return 1;
	}
	return 0;
}
