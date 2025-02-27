﻿#include "Server/LogicServer/Object/Role/Role.h"	
#include "Common/DataStruct/XMath.h"
#include "Common/DataStruct/XTime.h"
#include "Server/Base/CmdDef.h"
#include "Server/Base/NetAdapter.h"
#include "Server/LogicServer/Component/Battle/BattleUtil.h"
#include "Server/LogicServer/LogicServer.h"
#include "Server/LogicServer/SceneMgr/Scene.h"

LUNAR_IMPLEMENT_CLASS(Role)
{
	DECLEAR_OBJECT_METHOD(Role),
	DECLEAR_ACTOR_METHOD(Role),
	{0, 0}
};

Role::Role()
{
	m_nObjType = eOT_Role;
}

Role::~Role()
{
}

void Role::Init(int nID, int nConfID, const char* psName)
{
	m_nObjID = nID;
	m_nConfID = nConfID;
	strcpy(m_sName, psName);
}

void Role::RoleStartRunHandler(Packet* poPacket)
{
	if (GetScene() == NULL)
	{
		XLog(LEVEL_INFO, "RoleStartRunHandler: %s role not in scene\n", m_sName);
		return;
	}
	if (GetFollowTarget() > 0)
	{
		XLog(LEVEL_INFO, "RoleStartRunHandler: %s is following can not run manual.\n", m_sName);
		Actor::SyncPosition();
		return;
	}

	int nAOIID = 0;
	uint16_t uPosX = 0;
	uint16_t uPosY = 0;

	uint16_t uTarPosX = 0;
	uint16_t uTarPosY = 0;

	int16_t nSpeedX = 0;
	int16_t nSpeedY = 0;

	int64_t nClientMSTime = 0;
	double dClientMSTime = 0;

	uint8_t uFace = 0;

	goPKReader.SetPacket(poPacket);
	goPKReader >> nAOIID >> uPosX >> uPosY >> nSpeedX >> nSpeedY >> dClientMSTime >> uFace >> uTarPosX >> uTarPosY;
	nClientMSTime = (int64_t)dClientMSTime;
	XLog(LEVEL_DEBUG,  "%s Client start run srv:(%d,%d) clt(%d,%d) speed(%d,%d) tar(%d,%d) time:%lld\n"
		, m_sName, m_oPos.x, m_oPos.y, uPosX, uPosY, nSpeedX, nSpeedY, uTarPosX, uTarPosY, nClientMSTime-m_nClientRunStartMSTime);

	//客户端提供的时间值必须大于起始时间值
	if (nClientMSTime < m_nClientRunStartMSTime)
	{
		XLog(LEVEL_INFO,  "%s sync pos for start run client time invalid\n", m_sName);
		Actor::SyncPosition();
		return;
	}

	MapConf* poMapConf = m_poScene->GetMapConf();
	if (uPosX >= poMapConf->nPixelWidth || uPosY >= poMapConf->nPixelHeight || poMapConf->IsBlockUnit(uPosX/gnUnitWidth, uPosY/gnUnitHeight))
	{
		XLog(LEVEL_INFO, "%s sync pos for start run pos invalid pos:(%u,%u),block:%d\n", m_sName, uPosX, uPosY, poMapConf->IsBlockUnit(uPosX/gnUnitWidth, uPosY/gnUnitHeight));
		Actor::SyncPosition();
		return;
	}

	////正在移动则先更新移动后的新位置
	//if (m_nRunStartMSTime > 0 && m_nClientRunStartMSTime > 0) //当角色在AI寻路状态时 m_nClientRunStartMSTimer==0
	//{
	//	Actor::UpdateRunState(m_nRunStartMSTime + (nClientMSTime - m_nClientRunStartMSTime));
	//}

	////客户端与服务器坐标误差在一定范围内，则以客户端坐标为准
	//if (!BattleUtil::IsAcceptablePositionFaultBit(m_oPos.x, m_oPos.y, uPosX, uPosY))
	//{
	//	XLog(LEVEL_INFO, "%s sync pos for start run faultbit srv:(%d,%d) clt:(%d,%d) target:(%d,%d)\n", m_sName, m_oPos.x, m_oPos.y, uPosX, uPosY, uTarPosX, uTarPosY);
	//	uPosX = (uint16_t)m_oPos.x;
	//	uPosY = (uint16_t)m_oPos.y;
	//	Actor::SyncPosition();
	//	Actor::StopRun(true, true); //坐标不对就停下来 by panda 2018.6.29 mengzhu
	//	return;
	//}
	Actor::SetPos(Point(uPosX, uPosY), __FILE__, __LINE__);
	
	m_bRunCallback = false;
	m_nClientRunStartMSTime = nClientMSTime;
	m_oTargetPos = Point(uTarPosX, uTarPosY);
	Actor::StartRun(nSpeedX, nSpeedY, (int8_t)uFace);
}

void Role::RoleStopRunHandler(Packet* poPacket)
{
	if (GetScene() == NULL)
	{
		XLog(LEVEL_INFO, "RoleStopRunHandler: %s role not in scene\n", m_sName);
		return;
	}
	if (GetFollowTarget() > 0)
	{
		XLog(LEVEL_INFO, "RoleStopRunHandler: %s is following can not stop manual.\n", m_sName);
		Actor::SyncPosition();
		return;
	}

	int nAOIID = 0;
	uint16_t uPosX = 0;
	uint16_t uPosY = 0;
	int64_t nClientMSTime = 0;
	double dClientMSTime = 0;

	goPKReader.SetPacket(poPacket);
	goPKReader >> nAOIID >> uPosX >> uPosY >> dClientMSTime;
	nClientMSTime = (int64_t)dClientMSTime;

	XLog(LEVEL_DEBUG, "%s Client stop run srv:(%d,%d), clt:(%d,%d) time:%lld\n", m_sName, m_oPos.x, m_oPos.y, uPosX, uPosY, nClientMSTime-m_nClientRunStartMSTime);

	MapConf* poMapConf = m_poScene->GetMapConf();
	if (m_nRunStartMSTime == 0)
	{
		if (uPosX >= poMapConf->nPixelWidth || uPosY >= poMapConf->nPixelHeight || poMapConf->IsBlockUnit(uPosX / gnUnitWidth, uPosY / gnUnitHeight))
		{
			XLog(LEVEL_INFO, "%s sync pos for stop run faultbit srv:(%d,%d) clt:(%d,%d) target:(%d,%d) -01\n", m_sName, m_oPos.x, m_oPos.y, uPosX, uPosY, m_oLastTargetPos.x, m_oLastTargetPos.y);
			Actor::SyncPosition();
		}
		else
		{
			Actor::SetPos(Point(uPosX, uPosY), __FILE__, __LINE__);
		}
		////客户端与服务器坐标误差在一定范围内，则以客户端坐标为准
		//if (!BattleUtil::IsAcceptablePositionFaultBit(m_oPos.x, m_oPos.y, uPosX, uPosY))
		//{
		//	XLog(LEVEL_INFO, "%s sync pos for stop run faultbit srv:(%d,%d) clt:(%d,%d) target:(%d,%d) -01\n", m_sName, m_oPos.x, m_oPos.y, uPosX, uPosY, m_oLastTargetPos.x, m_oLastTargetPos.y);
		//	Actor::SyncPosition();
		//}
		//else
		//{
		//	Actor::SetPos(Point(uPosX, uPosY), __FILE__, __LINE__);
		//}
		return;
	}

	//客户端提交的时间比起跑时提交的时间小，则视为非法数据，强制使用服务器计算的值
	if (nClientMSTime < m_nClientRunStartMSTime || uPosX >= poMapConf->nPixelWidth || uPosY >= poMapConf->nPixelHeight || poMapConf->IsBlockUnit(uPosX/gnUnitWidth, uPosY/gnUnitHeight))
	{
		int64_t nNowMS = XTime::MSTime();
		Actor::UpdateRunState(nNowMS);
		Actor::SyncPosition();
		Actor::StopRun(true, true);
		XLog(LEVEL_INFO, "%s sync pos for stop run pos:(%d,%d) or time:(%d) error -02\n", m_sName, uPosX, uPosY, nClientMSTime-m_nClientRunStartMSTime);
		return;
	}
	////正在移动则先更新移动后的新位置
	//if (m_nRunStartMSTime > 0 && m_nClientRunStartMSTime > 0) //当角色在AI寻路状态时 m_nClientRunStartMSTimer==0
	//{
	//	Actor::UpdateRunState(m_nRunStartMSTime + (nClientMSTime - m_nClientRunStartMSTime));
	//}

	//客户端与服务器坐标误差在一定范围内，则以客户端坐标为准
	//if (!BattleUtil::IsAcceptablePositionFaultBit(m_oPos.x, m_oPos.y, uPosX, uPosY))
	//{
	//	XLog(LEVEL_INFO, "%s sync pos for stop run faultbit srv:(%d,%d) clt:(%d,%d) target:(%d,%d) -03\n", m_sName, m_oPos.x, m_oPos.y, uPosX, uPosY, m_oLastTargetPos.x, m_oLastTargetPos.y);
	//	Actor::SyncPosition();
	//}
	//else
	//{
	//	Actor::SetPos(Point(uPosX, uPosY), __FILE__, __LINE__);
	//}
	Actor::SetPos(Point(uPosX, uPosY), __FILE__, __LINE__);
	Actor::StopRun(true, true);
}

///////////////////lua export///////////////////