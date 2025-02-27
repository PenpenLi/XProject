﻿#include "Server/LogicServer/SceneMgr/Scene.h"

#include "Common/DataStruct/XTime.h"
#include "Common/DataStruct/XMath.h"
#include "Common/CDebug.h"
#include "Server/Base/CmdDef.h"
#include "Server/Base/NetAdapter.h"
#include "Server/Base/ServerContext.h"
#include "Server/LogicServer/LogicServer.h"
#include "Server/LogicServer/Object/Role/Role.h"
#include "Server/LogicServer/SceneMgr/SceneMgr.h"


//场景回收时间
const int nSCENE_COLLECT_MSTIME = 10 * 60 * 1000;
//AOI 废弃对象回收时间
const int nAOIDROP_COLLECT_MSTIME = 3*60*1000;

LUNAR_IMPLEMENT_CLASS(Scene)
{
	LUNAR_DECLARE_METHOD(Scene, GetMixID),
	LUNAR_DECLARE_METHOD(Scene, GetConfID),
	LUNAR_DECLARE_METHOD(Scene, EnterDup),
	LUNAR_DECLARE_METHOD(Scene, LeaveDup),
	LUNAR_DECLARE_METHOD(Scene, SetAutoCollected),
	LUNAR_DECLARE_METHOD(Scene, GetObj),
	//LUNAR_DECLARE_METHOD(Scene, MoveObj),
	LUNAR_DECLARE_METHOD(Scene, GetObjList),
	LUNAR_DECLARE_METHOD(Scene, AddObserver),
	LUNAR_DECLARE_METHOD(Scene, AddObserved),
	LUNAR_DECLARE_METHOD(Scene, RemoveObserver),
	LUNAR_DECLARE_METHOD(Scene, RemoveObserved),
	LUNAR_DECLARE_METHOD(Scene, GetAreaObservers),
	LUNAR_DECLARE_METHOD(Scene, GetAreaObserveds),
	LUNAR_DECLARE_METHOD(Scene, KickAllRole),
	LUNAR_DECLARE_METHOD(Scene, GetRoleCount),
	LUNAR_DECLARE_METHOD(Scene, DumpSceneInfo),
	LUNAR_DECLARE_METHOD(Scene, GetCreateTime),
	LUNAR_DECLARE_METHOD(Scene, GetSceneType),
	LUNAR_DECLARE_METHOD(Scene, IsAutoCollected),
	LUNAR_DECLARE_METHOD(Scene, GetRoleLastLeaveTime),
	{0, 0}
};

Scene::Scene(SceneMgr* poSceneMgr, int64_t nSceneMixID, MapConf* poMapConf, bool bCanCollected, int8_t nSceneType)
{
	m_poSceneMgr = poSceneMgr;
	m_nSceneMixID = nSceneMixID;
	m_bCanCollected = bCanCollected;
	m_nSceneType = nSceneType;

	int64_t nNowMS = XTime::MSTime();
	m_nLastUpdateTime = nNowMS;
	m_nLastRoleLeaveTime = nNowMS;

	m_poMapConf = poMapConf;
	m_nCreateTime = (int)time(0);
	m_nRoleCount = 0;
}

Scene::~Scene()
{
	AOI::AOIObjIter iter = m_oAOI.GetObjIterBegin();
	AOI::AOIObjIter iter_end = m_oAOI.GetObjIterEnd();
	for (; iter != iter_end; iter++)
	{
		if (iter->second->nAOIMode & AOI_MODE_DROP)
		{
			continue;
		}
		Object* poGameObj = iter->second->poGameObj;
		LeaveScene(poGameObj->GetAOIID());
	}
	m_oObjMap.clear();
	XLog(LEVEL_INFO, "Scene:%lld destructed!\n", m_nSceneMixID);
}

void Scene::Update(int64_t nNowMS)
{
	//清理AOI
	if (nNowMS - m_oAOI.m_nLastClearMSTime >= nAOIDROP_COLLECT_MSTIME)
	{
		m_oAOI.ClearDropObj(nNowMS);
	}
}

Array<AOIOBJ*>& Scene::GetAreaObservers(int nID, int nGameObjType)
{
	m_oObjCache.Clear();
	m_oAOI.GetAreaObservers(nID, m_oObjCache, nGameObjType);
	return m_oObjCache;
}

Array<AOIOBJ*>& Scene::GetAreaObserveds(int nID, int nGameObjType)
{
	m_oObjCache.Clear();
	m_oAOI.GetAreaObserveds(nID, m_oObjCache, nGameObjType);
	return m_oObjCache;
}

Object* Scene::GetGameObj(int nAOIID)
{
	AOIOBJ* poObj = m_oAOI.GetObj(nAOIID);
	return (poObj == NULL ? NULL : poObj->poGameObj);
}

bool Scene::IsTime2Collect(int64_t nNowMS)
{
	if (m_nRoleCount <= 0 && m_bCanCollected && nNowMS - m_nLastRoleLeaveTime >= nSCENE_COLLECT_MSTIME)
	{
		return true;
	}
	return false;
}


int Scene::EnterScene(Object* poObj, int nPosX, int nPosY, int8_t nAOIMode,  int nAOIArea[], int8_t nAOIType, int16_t nLine, int8_t nFace)
{
	int nObjID = poObj->GetID();
	int nObjType = poObj->GetType();

	if (m_oObjMap.find(nObjID) != m_oObjMap.end())
	{
		XLog(LEVEL_ERROR, "AddObj id:%ld type:%d already in scene:%lld\n", nObjID, nObjType, m_nSceneMixID);
		NSCDebug::TraceBack();
		return -1;
	}
	if (m_oObjMap.size() >= 10000)
	{
		XLog(LEVEL_ERROR, "AddObj too many secene obj:%lld obj:%d\n", m_nSceneMixID, m_oObjMap.size());
		NSCDebug::TraceBack();
		return -1;
	}

	poObj->SetFace(nFace);
	int nAOIID = m_oAOI.AddObj(nPosX, nPosY, nAOIMode, nAOIArea, poObj, nAOIType, nLine);
	if (nAOIID <= 0)
	{
		poObj->SetFace(0);
		XLog(LEVEL_ERROR, "AOI add obj error id:%lld type:%d\n", nObjID, nObjType);
		NSCDebug::TraceBack();
		return -1;
	}
	return nAOIID;
}

void Scene::KickAllRole()
{
	Array<Object*> oRoleArray;
	ObjIter iter = m_oObjMap.begin();
	ObjIter iter_end = m_oObjMap.end();

	for (; iter != iter_end; iter++)
	{
		if (iter->second->GetType() == eOT_Role)
			oRoleArray.PushBack(iter->second);
	}

	for (int i = 0; i < oRoleArray.Size(); i++)
	{
		Object* poObj = oRoleArray[i];
		LeaveScene(poObj->GetAOIID());
	}
}

void Scene::OnObjEnterScene(AOIOBJ* pObj)
{
	Object* poGameObj = pObj->poGameObj;
	m_oObjMap[poGameObj->GetID()] = poGameObj;

	poGameObj->OnEnterScene(this, pObj->nAOIID, Point(pObj->nPos[0], pObj->nPos[1]), pObj->nLine);
	if (poGameObj->GetType() == eOT_Role)
		m_nRoleCount++;

	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	Lunar<Object>::push(pState, pObj->poGameObj);
	pEngine->CallLuaRef("OnObjEnterScene", 2, 0);
}


void Scene::AfterObjEnterScene(AOIOBJ* pObj)
{
	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	Lunar<Object>::push(pState, pObj->poGameObj);
	pEngine->CallLuaRef("AfterObjEnterScene", 2, 0);
	
	//调用AfterObjEnterScene中可能又退出了场景
	if (pObj->poGameObj != NULL) {
		pObj->poGameObj->AfterEnterScene();
	}
}

void Scene::OnObjLeaveScene(AOIOBJ* pObj)
{
	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	Object* poGameObj = pObj->poGameObj;
	m_oObjMap.erase(poGameObj->GetID());

	Lunar<Object>::push(pState, pObj->poGameObj);
	pEngine->CallLuaRef("OnObjLeaveScene", 2, 0);

	poGameObj->OnLeaveScene();
	if (poGameObj->GetType() == eOT_Role)
	{
		m_nRoleCount--;
		m_nLastRoleLeaveTime = XTime::MSTime();
	}
}

void Scene::OnObjEnterObj(Array<AOIOBJ*>& oObserverCache, AOIOBJ* pObserved)
{
	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	lua_newtable(pState);
	for (int i = 0; i < oObserverCache.Size(); i++)
	{
		Lunar<Object>::push(pState, oObserverCache[i]->poGameObj);
		lua_rawseti(pState, -2, i+1);
	}

	lua_newtable(pState);
	Lunar<Object>::push(pState, pObserved->poGameObj);
	lua_rawseti(pState, -2, 1);
	pEngine->CallLuaRef("OnObjEnterObj", 3, 0);
}

void Scene::OnObjEnterObj(AOIOBJ* pObserver, Array<AOIOBJ*>& oObservedCache)
{
	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	lua_newtable(pState);
	Lunar<Object>::push(pState, pObserver->poGameObj);
	lua_rawseti(pState, -2, 1);

	lua_newtable(pState);
	for (int i = 0; i < oObservedCache.Size(); i++)
	{
		Lunar<Object>::push(pState, oObservedCache[i]->poGameObj);
		lua_rawseti(pState, -2, i+1);
	}
	pEngine->CallLuaRef("OnObjEnterObj", 3, 0);
}

void Scene::OnObjLeaveObj(Array<AOIOBJ*>& oObserverCache, AOIOBJ* pObserved)
{
	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	lua_newtable(pState);
	for (int i = 0; i < oObserverCache.Size(); i++)
	{
		Lunar<Object>::push(pState, oObserverCache[i]->poGameObj);
		lua_rawseti(pState, -2, i+1);
	}

	lua_newtable(pState);
	Lunar<Object>::push(pState, pObserved->poGameObj);
	lua_rawseti(pState, -2, 1);
	pEngine->CallLuaRef("OnObjLeaveObj", 3, 0);
}

void Scene::OnObjLeaveObj(AOIOBJ* pObserver, Array<AOIOBJ*>& oObservedCache)
{
	LuaWrapper* pEngine = LuaWrapper::Instance();
	lua_State* pState = pEngine->GetLuaState();
	lua_pushinteger(pState, m_nSceneMixID);

	lua_newtable(pState);
	Lunar<Object>::push(pState, pObserver->poGameObj);
	lua_rawseti(pState, -2, 1);

	lua_newtable(pState);
	for (int i = 0; i < oObservedCache.Size(); i++)
	{
		Lunar<Object>::push(pState, oObservedCache[i]->poGameObj);
		lua_rawseti(pState, -2, i+1);
	}
	pEngine->CallLuaRef("OnObjLeaveObj", 3, 0);
}




///////////////////lua export///////////////
int Scene::GetMixID(lua_State* pState)
{
	lua_pushinteger(pState, m_nSceneMixID);
	return 1;
}

int Scene::GetConfID(lua_State* pState)
{
	lua_pushinteger(pState, m_nSceneMixID&0xFFFF);
	return 1;
}

int Scene::GetObj(lua_State* pState)
{
	int nAOIID= (int)luaL_checkinteger(pState, 1);
	Object* poGameObj = GetGameObj(nAOIID);
	if (poGameObj == NULL)
	{
		return 0;
	}

	switch (poGameObj->GetType())
	{
		case eOT_Role:
		case eOT_Robot:
		case eOT_Monster:
		{
			Actor* poActor = (Actor*)poGameObj;
			Lunar<Actor>::push(pState, poActor);
		}
		break;
		default:
		{
			Lunar<Object>::push(pState, poGameObj);
		}
		break;
	}
	return 1;
}

int Scene::EnterDup(lua_State* pState)
{
	int nDupMixID = (int)luaL_checkinteger(pState, 1);
	LogicServer *poService = (LogicServer*)(gpoContext->GetService());
	if (poService->GetSceneMgr()->GetScene(nDupMixID) == NULL)
	{
		return LuaWrapper::luaM_error(pState, "Scene::EnterDup scene:%d not exist!!!\n");
	}

	luaL_checktype(pState, 2, LUA_TUSERDATA);
	Lunar<Object>::userdataType *ud = static_cast<Lunar<Object>::userdataType*>(lua_touserdata(pState, 2));
	Object* poObject = ud->pT;
	
	int nPosX = (int)luaL_checkinteger(pState, 3);
	int nPosY = (int)luaL_checkinteger(pState, 4);
	int8_t nAOIMode = (int8_t)luaL_checkinteger(pState, 5);

	int tAOIArea[2];
	tAOIArea[0] = (int)luaL_checkinteger(pState, 6);
	tAOIArea[1] = (int)luaL_checkinteger(pState, 7);
	assert(tAOIArea[0] >= 0 && tAOIArea[1] >= 0);

	int nLine = (int)luaL_checkinteger(pState, 8); //0公共线,-1自动
	int8_t nFace = (int8_t)luaL_checkinteger(pState, 9); //面向

	int nAOIID = EnterScene(poObject, nPosX, nPosY, nAOIMode, tAOIArea, AOI_TYPE_RECT, nLine, nFace);
	if (nAOIID <= 0)
	{
		return LuaWrapper::luaM_error(pState, "AddObj to scene fail!");
	}
	lua_pushinteger(pState, nAOIID);
	return 1;
};

int Scene::LeaveDup(lua_State* pState)
{
	int nAOIID = (int)luaL_checkinteger(pState, 1);
	LeaveScene(nAOIID);
	return 0;
}

int Scene::SetAutoCollected(lua_State* pState)
{
	bool bAuto = lua_toboolean(pState, 1) != 0;
	m_bCanCollected = bAuto;
	return 0;
}

//int Scene::MoveObj(lua_State* pState)
//{
//	int nAOIID = (int)luaL_checkinteger(pState, 1);
//	int nPosX = (int)luaL_checkinteger(pState, 2);
//	int nPosY = (int)luaL_checkinteger(pState, 3);
//	MoveObj(nAOIID, nPosX, nPosY);
//	return 0;
//}

int Scene::RemoveObserver(lua_State* pState)
{
	int nAOIID = (int)luaL_checkinteger(pState, 1);
	bool bLeaveScene = lua_toboolean(pState, 2) != 0;
	m_oAOI.RemoveObserver(nAOIID, bLeaveScene);
	return 0;
}

int Scene::RemoveObserved(lua_State* pState)
{
	int nAOIID = (int)luaL_checkinteger(pState, 1);
	m_oAOI.RemoveObserved(nAOIID);
	return 0;
}

int Scene::AddObserver(lua_State* pState)
{
	int nAOIID = (int)luaL_checkinteger(pState, 1);
	if (!m_oAOI.AddObserver(nAOIID))
	{
		luaL_where(pState, 1);
		XLog(LEVEL_ERROR, "%s\n", lua_tostring(pState, -1));
	}
	lua_pushinteger(pState, nAOIID);
	return 1;
}

int Scene::AddObserved(lua_State* pState)
{
	int nAOIID = (int)luaL_checkinteger(pState, 1);
	m_oAOI.AddObserved(nAOIID);
	lua_pushinteger(pState, nAOIID);
	return 1;
}

int Scene::GetAreaObservers(lua_State* pState)
{
	int nAOIID= (int)luaL_checkinteger(pState, 1);
	int nGameObjType = (int)luaL_checkinteger(pState, 2);
	GetAreaObservers(nAOIID, nGameObjType);

	lua_newtable(pState);
	int nTop = lua_gettop(pState);
	for (int i = 0; i < m_oObjCache.Size(); i++)
	{
		Lunar<Object>::push(pState, m_oObjCache[i]->poGameObj);
		lua_rawseti(pState, nTop, i + 1);
	}
	return 1; 
}

int Scene::GetAreaObserveds(lua_State* pState)
{
	int nAOIID = (int)luaL_checkinteger(pState, 1);
	int nGameObjType = (int)luaL_checkinteger(pState, 2);
	GetAreaObserveds(nAOIID, nGameObjType);

	lua_newtable(pState);
	int nTop = lua_gettop(pState);
	for (int i = 0; i < m_oObjCache.Size(); i++)
	{
		Lunar<Object>::push(pState, m_oObjCache[i]->poGameObj);
		lua_rawseti(pState, nTop, i + 1);
	}
	return 1;
}

int Scene::GetObjList(lua_State* pState)
{
	int nLine = (int)luaL_checkinteger(pState, 1);
	int nObjType = (int)luaL_checkinteger(pState, 2);

	AOI::AOIObjIter iter = m_oAOI.GetObjIterBegin();
	AOI::AOIObjIter iter_end = m_oAOI.GetObjIterEnd();

	lua_newtable(pState);
	for (int n = 1; iter != iter_end; iter++)
	{
		if (iter->second->nAOIMode == AOI_MODE_DROP)
			continue;
		Object* poObj = iter->second->poGameObj;
		if (poObj != NULL && (nObjType == 0 || nObjType == poObj->GetType()) && (nLine == -1 || iter->second->nLine == nLine))
		{
			Lunar<Object>::push(pState, poObj);
			lua_rawseti(pState, -2, n++);
		}
	}
	return 1;
}

int Scene::KickAllRole(lua_State* pState)
{
	KickAllRole();
	return 0;
}

int Scene::DumpSceneInfo(lua_State* pState)
{
	XLog(LEVEL_INFO, "dump scene Info-------\n");
	XLog(LEVEL_INFO, "scenetype:%d scenemixid:%lld sceneid:%d rolecount:%d collectflag:%d lastleavemins:%f\n"
		, m_nSceneType, m_nSceneMixID, m_nSceneMixID&0xFFFF, m_nRoleCount, m_bCanCollected, (XTime::MSTime()-m_nLastRoleLeaveTime)/60/1000.0);
	int16_t *pLineArray = m_oAOI.GetLineArray();
	for (int i = 0; i < MAX_LINE; i++)
	{
		if (pLineArray[i] > 0)
		{
			XLog(LEVEL_INFO, "line:%d objs:%d\n", i, pLineArray[i]);
		}
	}
	return 0;
}

int Scene::GetSceneType(lua_State* pState)
{
	lua_pushinteger(pState, m_nSceneType);
	return 1;
}

int Scene::GetCreateTime(lua_State* pState)
{
	lua_pushinteger(pState, m_nCreateTime);
	return 1;
}

int Scene::GetRoleCount(lua_State* pState)
{
	lua_pushinteger(pState, m_nRoleCount);
	return 1;
}

int Scene::IsAutoCollected(lua_State* pState)
{
	lua_pushboolean(pState, (int)m_bCanCollected);
	return 1;
}

int Scene::GetRoleLastLeaveTime(lua_State* pState)
{
	lua_pushinteger(pState, m_nLastRoleLeaveTime);
	return 1;
}
