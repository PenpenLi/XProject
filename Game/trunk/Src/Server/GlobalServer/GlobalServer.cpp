﻿#include "Server/GlobalServer/GlobalServer.h"
#include "Include/Network/Network.hpp"
#include "Common/DataStruct/XMath.h"
#include "Common/DataStruct/XTime.h"
#include "Common/TimerMgr/TimerMgr.h"
#include "Server/Base/CmdDef.h"
#include "Server/Base/NetAdapter.h"
#include "Server/Base/RouterMgr.h"
#include "Server/Base/ServerContext.h"

extern ServerContext* gpoContext;

GlobalServer::GlobalServer()
{
	m_uListenPort = NULL;
	m_poExterNet = NULL;
	m_poInnerNet = NULL;
	memset(m_sListenIP, 0, sizeof(m_sListenIP));
}

GlobalServer::~GlobalServer()
{
}

bool GlobalServer::Init(int8_t nServiceID, const char* psListenIP, uint16_t uListenPort)
{
	char sServiceName[32];
	sprintf(sServiceName, "GlobalServer:%d", nServiceID);
	m_oNetEventHandler.GetMailBox().SetName(sServiceName);

	if (!Service::Init(nServiceID, sServiceName))
	{
		return false;
	}
	if (psListenIP != NULL)
	{
		strcpy(m_sListenIP, psListenIP);
	}
	m_uListenPort = uListenPort;
	m_poExterNet = INet::CreateNet(NET_TYPE_EXTERNAL, nServiceID, 32, &m_oNetEventHandler);
	if (m_poExterNet == NULL)
	{
		return false;
	}

	m_poInnerNet = INet::CreateNet(NET_TYPE_INTERNAL, nServiceID, 8, &m_oNetEventHandler);
	if (m_poInnerNet == NULL)
	{
		return false;
	}
	return true;
}

bool GlobalServer::RegToRouter(int nRouterServiceID)
{
	ROUTER* poRouter = gpoContext->GetRouterMgr()->GetRouter(nRouterServiceID);
	assert(poRouter != NULL);
	Packet* poPacket = Packet::Create();
	if (poPacket == NULL) {
		return false;
	}
	INNER_HEADER oHeader(NSSysCmd::ssRegServiceReq, 0, GetServiceID(),gpoContext->GetServerID(), nRouterServiceID, 0);
	poPacket->AppendInnerHeader(oHeader, NULL, 0);
	if (!m_poInnerNet->SendPacket(poRouter->nSession, poPacket))
	{
		poPacket->Release();
		return false;
	}
	return true;
}

bool GlobalServer::Start()
{
	if (!m_poExterNet->Listen(m_sListenIP[0]?m_sListenIP:NULL, m_uListenPort))
	{
		return false;
	}

	for (;;)
	{
		ProcessNetEvent(10);
		int64_t nNowMS = XTime::MSTime();
		Service::Update(nNowMS);
		ProcessTimer(nNowMS);
	}
	return true;
}

void GlobalServer::ProcessNetEvent(int64_t nWaitMSTime)
{
	NSNetEvent::EVENT oEvent;
	if (!m_oNetEventHandler.RecvEvent(oEvent, (uint32_t)nWaitMSTime))
	{
		return;
	}
	switch (oEvent.uEventType)
	{
		case NSNetEvent::eEVT_ON_RECV:
		{
			NSNetEvent::EVENT_RECV& oRecv = oEvent.U.oRecv;
			if (oEvent.pNet == m_poExterNet)
			{
				OnExterNetMsg(oRecv.nSessionID, oRecv.poPacket);
			}
			else if (oEvent.pNet == m_poInnerNet)
			{
				OnInnerNetMsg(oRecv.nSessionID, oRecv.poPacket);
			}
			break;
		}
		case NSNetEvent::eEVT_ON_ACCEPT:
		{
			if (oEvent.pNet == m_poExterNet)
			{
				OnExterNetAccept(oEvent.U.oAccept.nSessionID);
			}
			break;
		}
		case NSNetEvent::eEVT_ON_CLOSE:
		{
			if (oEvent.pNet == m_poExterNet)
			{
				OnExterNetClose(oEvent.U.oClose.nSessionID);
			}
			else if (oEvent.pNet == m_poInnerNet)
			{
				OnInnerNetClose(oEvent.U.oClose.nSessionID);
			}
			break;
		}
		case NSNetEvent::eEVT_ON_LISTEN:
		{
			break;
		}
		case NSNetEvent::eEVT_ON_CONNECT:
		{
			if (oEvent.pNet == m_poInnerNet)
			{
				OnInnerNetConnect(oEvent.U.oConnect.nSessionID, oEvent.U.oConnect.uRemoteIP, oEvent.U.oConnect.uRemotePort);
			}
			break;
		}
		default:
		{
			XLog(LEVEL_ERROR, "Msg type error:%d\n", oEvent.uEventType);
			break;
		}
	}
}

void GlobalServer::ProcessTimer(int64_t nNowMSTime)
{
	static int64_t nLastMSTime = XTime::MSTime();
	if (nNowMSTime - nLastMSTime < 100)
	{
		return;
	}
	nLastMSTime = nNowMSTime;
	TimerMgr::Instance()->ExecuteTimer(nNowMSTime);
}

void GlobalServer::OnExterNetAccept(int nSessionID)
{
	//XLog(LEVEL_INFO, "On externet accept\n");
}

void GlobalServer::OnExterNetClose(int nSessionID)
{
	//XLog(LEVEL_INFO, "On externet close\n");
}

void GlobalServer::OnExterNetMsg(int nSessionID, Packet* poPacket)
{
	EXTER_HEADER oExterHeader;
	if (!poPacket->GetExterHeader(oExterHeader, true))
	{
		poPacket->Release();
		XLog(LEVEL_ERROR, "%s: OnExterNetMsg: packet get exter header fail\n", GetServiceName());
		return;
	}
	//m_poExterNet->SetSentClose(nSessionID);
	gpoContext->GetPacketHandler()->OnRecvExterPacket(nSessionID, poPacket, oExterHeader);
}


void GlobalServer::OnInnerNetConnect(int nSessionID, uint32_t uRemoteIP, uint16_t nRemotePort)
{
    ROUTER* poRouter = gpoContext->GetRouterMgr()->OnConnectRouterSuccess(nRemotePort, nSessionID);
    assert(poRouter != NULL);
    RegToRouter(poRouter->nService);
}

void GlobalServer::OnInnerNetClose(int nSessionID)
{
	XLog(LEVEL_INFO, "On innernet disconnect\n");
	gpoContext->GetRouterMgr()->OnRouterDisconnected(nSessionID);
}

void GlobalServer::OnInnerNetMsg(int nSessionID, Packet* poPacket)
{
	assert(poPacket != NULL);
	INNER_HEADER oHeader;
	int* pSessionArray = NULL;
	if (!poPacket->GetInnerHeader(oHeader, &pSessionArray, true))
	{
		XLog(LEVEL_INFO, "%s: Get inner header fail\n", GetServiceName());
		poPacket->Release();
		return;
	}
	if (oHeader.nTarService != GetServiceID())
	{
		XLog(LEVEL_INFO, "%s: Tar service error\n", GetServiceName());
		poPacket->Release();
		return;
	}
	gpoContext->GetPacketHandler()->OnRecvInnerPacket(nSessionID, poPacket, oHeader, pSessionArray);
}
