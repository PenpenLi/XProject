﻿#include "Server/LogicServer/Object/Actor.h"

#include "Common/DataStruct/XTime.h"
#include "Common/DataStruct/XMath.h"
#include "Common/DataStruct/TimeMonitor.h"

#include "Server/Base/CmdDef.h"
#include "Server/LogicServer/Component/Battle/BattleUtil.h"
#include "Server/LogicServer/ConfMgr/ConfMgr.h"
#include "Server/LogicServer/LogicServer.h"
#include "Server/LogicServer/SceneMgr/SceneMgr.h"

LUNAR_IMPLEMENT_CLASS(Actor)
{
	DECLEAR_OBJECT_METHOD(Actor),
	DECLEAR_ACTOR_METHOD(Actor),
	{ 0, 0 },
};

Actor::Actor()
{
	m_uServer = 0;
	m_nSession = 0;

	m_nRunSpeedX = 0;
	m_nRunSpeedY = 0;
	m_nRunStartX = 0;
	m_nRunStartY = 0;
	m_nRunStartMSTime = 0;
	m_nClientRunStartMSTime = 0;

	m_bRunCallback = false;

}

Actor::~Actor()
{
}

void Actor::OnEnterScene(Scene* poScene, int nAOIID, const Point& oPos, int16_t nLine)
{
	Object::OnEnterScene(poScene, nAOIID, oPos, nLine);
	StopRun();
}

void Actor::StartRun(int nSpeedX, int nSpeedY, int8_t nFace, bool bSelf/*=false*/)
{
	if (!m_oTargetPos.IsValid())
	{
		XLog(LEVEL_ERROR, "Actor::StartRun target pos error!\n");
		return;
	}
	if (m_nClientRunStartMSTime == 0)
	{
		m_nClientRunStartMSTime = XTime::UnixMSTime();
	}
	m_nRunStartMSTime = XTime::MSTime();
	m_nRunSpeedX = nSpeedX;
	m_nRunSpeedY = nSpeedY;
	m_nRunStartX = m_oPos.x;
	m_nRunStartY = m_oPos.y;
	Object::SetFace(nFace);
	BroadcastStartRun(bSelf);
	//XLog(LEVEL_INFO,"%s Start run pos:(%d, %d) speed:(%d,%d)\n", m_sName, m_oPos.x, m_oPos.y, nSpeedX, nSpeedY);
}

void Actor::StopRun(bool bBroadcast, bool bClientStop)
{
	m_oLastTargetPos = m_oTargetPos;
	m_oTargetPos.Reset();
	if (m_nRunStartMSTime > 0)
	{
		m_nRunStartMSTime = 0;
		m_nClientRunStartMSTime = 0;
		m_nRunSpeedX = 0;
		m_nRunSpeedY = 0;
		m_nRunStartX = 0;
		m_nRunStartY = 0;
		if (bBroadcast)
		{
			BroadcastStopRun(!bClientStop);
		}
		XLog(LEVEL_DEBUG, "%s Do stop run pos:(%d, %d) from_client:%d time:%d\n", m_sName, m_oPos.x, m_oPos.y, bClientStop, XTime::MSTime());
	}
}

bool Actor::UpdateRunState(int64_t nNowMS)
{
	bool bCanMove = false;
	if (m_nRunStartMSTime > 0 && GetScene() != NULL)
	{
		int nNewPosX = 0;
		int nNewPosY = 0;
		bCanMove = CalcPositionAtTime(nNowMS, nNewPosX, nNewPosY);
		Object::SetPos(Point(nNewPosX, nNewPosY));
		if (!bCanMove || (m_nRunSpeedX == 0 && m_nRunSpeedY == 0))
		{
			StopRun();
		}
		if (m_oTargetPos.IsValid())
		{
			Point oStartPos(m_nRunStartX, m_nRunStartY);
			if (m_oPos.Distance(oStartPos) >= m_oTargetPos.Distance(oStartPos)-10)
			{
				Point oTargetPos = m_oTargetPos;
				StopRun(); //这里会重置m_oTargetPos
				OnReacheTargetPos(oTargetPos);
			}
		}
		UpdateFollow(nNowMS);
		//XLog(LEVEL_DEBUG, "Pos(%d,%d) canmove:%d\n", nNewPosX, nNewPosY, bCanMove);
	}
	return bCanMove;
}

void Actor::OnReacheTargetPos(Point& oTargetPos)
{
	XLog(LEVEL_DEBUG, "%d %s reach target pos(%d,%d) tarpos(%d,%d)\n", time(NULL), m_sName, m_oPos.x, m_oPos.y, oTargetPos.x, oTargetPos.y);
	if (m_bRunCallback)
	{
		m_bRunCallback = false;
		LuaWrapper::Instance()->FastCallLuaRef<void, CNOTUSE>("OnObjReachPos", 0, "iiii", m_nObjID, m_nObjType, oTargetPos.x, oTargetPos.y);
	}
}

void Actor::UpdateFollow(int64_t nNowMS)
{
	if (m_poScene == NULL)
	{
		return;
	}

	LogicServer* poLogic = (LogicServer*)(gpoContext->GetService());
	SceneMgr* poSceneMgr = poLogic->GetSceneMgr();

	Follow::FollowVec* poFollowVec = (poSceneMgr->GetFollow()).GetFollowList(m_nObjType, m_nObjID);
	if (poFollowVec == NULL || poFollowVec->size() <= 0)
	{
		return;
	}

	RoleMgr* poRoleMgr = poLogic->GetRoleMgr();
	MonsterMgr* poMonsterMgr = poLogic->GetMonsterMgr();

	const Point& oTarPos = GetPos();
	for (int i = 0; i < poFollowVec->size(); i++)
	{
		FOLLOW oFollow((*poFollowVec)[i]);

		Object* poFollowObj = NULL;
		if (oFollow.nObjType == eOT_Role)
		{
			poFollowObj = poRoleMgr->GetRoleByID(oFollow.nObjID);
		}
		else
		{
			poFollowObj = poMonsterMgr->GetMonsterByID(oFollow.nObjID);
		}

		if (poFollowObj == NULL || poFollowObj->GetScene() != m_poScene)
		{
			continue;
		}
		if (oTarPos.Distance(poFollowObj->GetPos()) >= (gnUnitWidth*gnTowerWidth)*0.5)
		{
			poFollowObj->SetPos(oTarPos);
		}
	}
}

void Actor::UpdateViewList(int64_t nNowMS)
{
	Scene* poScene = GetScene();
	if (poScene == NULL)
	{
		return;
	}
	poScene->MoveObj(GetAOIID(), m_oPos.x, m_oPos.y);
}

bool Actor::CalcPositionAtTime(int64_t nNowMS, int& nNewPosX, int& nNewPosY)
{
	int nNewX = m_nRunStartX;
	int nNewY = m_nRunStartY;
	int nTimeElapased = (int)(nNowMS - m_nRunStartMSTime);

	if (nTimeElapased > 0)
	{
		//常规移动计算
		nNewX += (int)((m_nRunSpeedX * nTimeElapased) * 0.001);
		nNewY += (int)((m_nRunSpeedY * nTimeElapased) * 0.001);
	}

	bool bRes = true;
	if (nNewX != m_oPos.x || nNewY != m_oPos.y)
	{
		bRes = BattleUtil::FixLineMovePoint(GetScene()->GetMapConf(), m_oPos.x, m_oPos.y, nNewX, nNewY, this);
	}
	nNewPosX = nNewX;
	nNewPosY = nNewY;
	return bRes;
}

void Actor::RunTo(const Point& oTarPos, int nMoveSpeed)
{
	if (m_poScene == NULL)
	{
		return;
	}
	if (nMoveSpeed <= 0)
	{
		return;
	}
	if (m_oPos == oTarPos)
	{
		m_bRunCallback = true;
		m_oTargetPos = oTarPos;
		OnReacheTargetPos(m_oTargetPos);
		return;
	}

	int nSpeedX = 0;
	int nSpeedY = 0;
	BattleUtil::CalcMoveSpeed1(nMoveSpeed, m_oPos, oTarPos, nSpeedX, nSpeedY);
	if (nSpeedX == 0 && nSpeedY == 0)
	{
		StopRun();
	}

	int nOldSpeedX = GetSpeedX();
	int nOldSpeedY = GetSpeedY();
	if (!IsRunning() || nSpeedX != nOldSpeedX || nSpeedY != nOldSpeedY)
	{
		m_bRunCallback = true;
		m_oTargetPos = oTarPos;
		StartRun(nSpeedX, nSpeedY, m_nFace, true);
	}
#ifdef _DEBUG
	XLog(LEVEL_DEBUG, "%d %s run to speed(%d,%d), pos(%d,%d), tarpos(%d,%d) time:%d\n"
		, time(NULL), m_sName, nSpeedX, nSpeedY, m_oPos.x, m_oPos.y, oTarPos.x, oTarPos.y, XTime::MSTime());
#endif // _DEBUG

}

void Actor::SyncPosition(const char* pWhere)
{
	gpoPacketCache->Reset();
	goPKWriter << m_nAOIID << (uint16_t)m_oPos.x << (uint16_t)m_oPos.y << (int8_t)m_nFace;
	Packet* poPacket = gpoPacketCache->DeepCopy(__FILE__, __LINE__);

	NetAdapter::SERVICE_NAVI oNavi;
	oNavi.uSrcServer = gpoContext->GetServerID();
	oNavi.nSrcService = gpoContext->GetService()->GetServiceID();
	oNavi.uTarServer = GetServer();
	oNavi.nTarService = GetSession() >> SERVICE_SHIFT;
	oNavi.nTarSession = GetSession();
	NetAdapter::SendExter(NSCltSrvCmd::sSyncActorPosRet, poPacket, oNavi);
}

void Actor::BroadcastPos(bool bSelf)
{
	int nSelfServer = 0;
	int nSelfSession = 0;
	if (bSelf)
	{
		nSelfServer = m_uServer;
		nSelfSession = m_nSession;
	}
	CacheActorNavi(nSelfServer, nSelfSession);

	if (goNaviCache.Size() <= 0)
	{
		return;
	}

	gpoPacketCache->Reset();
	goPKWriter << m_nAOIID << (uint16_t)m_oPos.x << (uint16_t)m_oPos.y << (int8_t)m_nFace;
	Packet* poPacket = gpoPacketCache->DeepCopy(__FILE__, __LINE__);
	NetAdapter::BroadcastExter(NSCltSrvCmd::sSyncActorPosRet, poPacket, goNaviCache);
}

void Actor::BroadcastStartRun(bool bSelf /*=false*/)
{
	int nSelfServer = 0;
	int nSelfSession = 0;
	if (bSelf)
	{
		nSelfServer = m_uServer;
		nSelfSession = m_nSession;
	}
	CacheActorNavi(nSelfServer, nSelfSession);

	if (goNaviCache.Size() <= 0)
	{
		return;
	}
	gpoPacketCache->Reset();
	uint16_t uTarPosX = (uint16_t)m_oTargetPos.x;
	uint16_t uTarPosY = (uint16_t)m_oTargetPos.y;
	goPKWriter << m_nAOIID << (uint16_t)m_oPos.x << (uint16_t)m_oPos.y << (int16_t)m_nRunSpeedX << (int16_t)m_nRunSpeedY << (uint8_t)m_nFace << uTarPosX << uTarPosY;
	Packet* poPacket = gpoPacketCache->DeepCopy(__FILE__, __LINE__);
	NetAdapter::BroadcastExter(NSCltSrvCmd::sActorStartRunRet, poPacket, goNaviCache);
}

void Actor::BroadcastStopRun(bool bSelf)
{
	int nSelfServer = 0;
	int nSelfSession = 0;
	if (bSelf)
	{
		nSelfServer = m_uServer;
		nSelfSession = m_nSession;
	}
	CacheActorNavi(nSelfServer, nSelfSession);
	if (goNaviCache.Size() <= 0)
	{
		return;
	}

	gpoPacketCache->Reset();
	goPKWriter << m_nAOIID << (uint16_t)m_oPos.x << (uint16_t)m_oPos.y << (int8_t)m_nFace;
	Packet* poPacket = gpoPacketCache->DeepCopy(__FILE__, __LINE__);
	NetAdapter::BroadcastExter(NSCltSrvCmd::sActorStopRunRet, poPacket, goNaviCache);
}


///////////////lua export//////////////////
#define GET_FIGHT_PARAM(pState, nType) \
{\
lua_rawgeti(pState, -1, nType); \
m_oFightParam[nType] = (int)lua_tointeger(pState, -1); \
lua_pop(pState, 1); \
}

#define PUSH_FIGHT_PARAM(pState, nType) \
{\
lua_pushinteger(pState, m_oFightParam[nType]); \
lua_rawseti(pState, -2, nType);\
}

int Actor::GetRunSpeed(lua_State* pState)
{
	lua_pushinteger(pState, m_nRunSpeedX);
	lua_pushinteger(pState, m_nRunSpeedY);
	return 2;
}

int Actor::BindSession(lua_State* pState)
{
	int nSession = (int)lua_tointeger(pState, -1);
	LogicServer * poLogic = (LogicServer*)gpoContext->GetService();
	poLogic->GetRoleMgr()->BindSession(m_nObjID, nSession);
	return 0;
}

int Actor::StopRun(lua_State* pState)
{
	StopRun();
	return 0;
}

int Actor::RunTo(lua_State* pState)
{
	int nPosX = (int)luaL_checkinteger(pState, 1);
	int nPosY = (int)luaL_checkinteger(pState, 2);
	int nSpeed = (int)luaL_checkinteger(pState, 3);
	RunTo(Point(nPosX, nPosY), nSpeed);
	return 0;
}

int Actor::GetTarPos(lua_State* pState)
{
	lua_pushinteger(pState, m_oTargetPos.x);
	lua_pushinteger(pState, m_oTargetPos.y);
	return 2;
}
