﻿#ifndef __SCENE_H__
#define __SCENE_H__

#include "Include/Script/Script.hpp"

#include "Server/LogicServer/SceneMgr/AOI.h"
#include "Common/DataStruct/XUUID.h"
#include "Common/DataStruct/Point.h"

class Object;
class Actor;
class Role;

class SceneMgr;
struct MapConf;

class Scene
{
public:
	LUNAR_DECLARE_CLASS(Scene);

	//<aoiid, object*>
	typedef std::unordered_map<int, Object*> ObjMap;
	typedef ObjMap::iterator ObjIter;

	friend class SceneMgr;

public:
	//nSceneType 1城镇; 2副本
	Scene(SceneMgr* poSceneMgr, int64_t nSceneMixID, MapConf* poMapConf, bool bCanCollected=true, int8_t nSceneType=2);
	~Scene();

	bool InitAOI(int nMapWidth, int nMapHeight, int nLineObjNum) { return m_oAOI.Init(this, nMapWidth, nMapHeight, nLineObjNum); }

	void Update(int64_t nNowMS);
	bool IsTime2Collect(int64_t nNowMS);

	Object* GetGameObj(int nAOIID);
	ObjMap& GetObjMap() { return m_oObjMap; }
	int GetRoleCount() { return m_nRoleCount;  }

	MapConf* GetMapConf() { return m_poMapConf; }
	SceneMgr* GetSceneMgr() { return m_poSceneMgr;  }
	int64_t GetSceneMixID() { return m_nSceneMixID; }
	int8_t GetSceneType() { return m_nSceneType; }

public:
	int EnterScene(Object* poObj, int nPosX, int nPosY, int8_t nAOIMode, int nAOIArea[], int8_t nAOIType=AOI_TYPE_RECT, int16_t nLine=-1, int8_t nFace=0);
	void LeaveScene(int nAOIID) { m_oAOI.RemoveObj(nAOIID, true); }
	void MoveObj(int nAOIID, int nTarX, int nTarY) { m_oAOI.MoveObj(nAOIID, nTarX, nTarY); }
	void SetLine(int nAOIID, int16_t nLine) { m_oAOI.ChangeLine(nAOIID, nLine); }
	void OnObjEnterScene(AOIOBJ* pObj);		//进入场景但是未同步视野
	void AfterObjEnterScene(AOIOBJ* pObj);	//同步了视野后
	void OnObjLeaveScene(AOIOBJ* pObj);
	void OnObjEnterObj(Array<AOIOBJ*>& oObserverCache, AOIOBJ* pObserved);
	void OnObjEnterObj(AOIOBJ* pObserver, Array<AOIOBJ*>& oObservedCache);
	void OnObjLeaveObj(Array<AOIOBJ*>& oObserverCache, AOIOBJ* pObserved);
	void OnObjLeaveObj(AOIOBJ* pObserver, Array<AOIOBJ*>& oObservedCache);
	Array<AOIOBJ*>& GetAreaObservers(int nAOIID, int nGameObjType);
	Array<AOIOBJ*>& GetAreaObserveds(int nAOIID, int nGameObjType);
	void KickAllRole();

private:
	AOI m_oAOI;						//AOI
	Array<AOIOBJ*> m_oObjCache;		//AOI对象缓存

	SceneMgr* m_poSceneMgr;
	int64_t m_nSceneMixID;			//自增ID|配置ID
	bool m_bCanCollected;			//是否可以被收集
	int8_t m_nSceneType;			//场景类型
	int m_nCreateTime;				//创建时间
	
	ObjMap m_oObjMap;				//游戏对象列表
	uint16_t m_nRoleCount;			//角色数量


	int64_t m_nLastUpdateTime;		//最后更新时间
	int64_t m_nLastRoleLeaveTime;	//最后角色离开时间

	MapConf* m_poMapConf;			//地图配置
	DISALLOW_COPY_AND_ASSIGN(Scene);


/////////////////export to lua///////////////////////
public:
	int GetMixID(lua_State* pState);
	int GetConfID(lua_State* pState);
	int EnterDup(lua_State* pState);
	int LeaveDup(lua_State* pState);
	int SetAutoCollected(lua_State* pState);
	int GetRoleCount(lua_State* pState);
	int GetSceneType(lua_State* pState);
	int GetCreateTime(lua_State* pState);
	int IsAutoCollected(lua_State* pState);
	int GetRoleLastLeaveTime(lua_State* pState);

	int GetObj(lua_State* pState);
	//int MoveObj(lua_State* pState);
	int GetObjList(lua_State* pState);

	int AddObserver(lua_State* pState);
	int AddObserved(lua_State* pState);
	int RemoveObserver(lua_State* pState);
	int RemoveObserved(lua_State* pState);

	int GetAreaObservers(lua_State* pState);
	int GetAreaObserveds(lua_State* pState);

	int KickAllRole(lua_State* pState);
	int DumpSceneInfo(lua_State* pState);
};


#endif
