﻿#ifndef __SERVERCONTEXT_H__
#define __SERVERCONTEXT_H__

#include "Common/Platform.h"
#include "Include/Script/LuaWrapper.h"
#include "Server/Base/PacketHandler.h"
#include "Common/LuaCommon/LuaSerialize.h"
#include "Common/LuaCommon/LuaTableSeri.h"
#include "Server/Base/RouterMgr.h"
#include "Server/Base/ServerConfig.h"
#include "Server/Base/Service.h"

class ServerContext
{
public:
	ServerContext();
	virtual ~ServerContext();

    uint16_t GetServerID() { return m_oSrvConf.uServerID; }

	Service* GetService() { return m_poService; }
	void SetService(Service* poService) { m_poService = poService; }

	RouterMgr* GetRouterMgr() { return m_poRouterMgr; }
	void SetRouterMgr(RouterMgr* poRouterMgr) { m_poRouterMgr = poRouterMgr; }

	PacketHandler* GetPacketHandler() { return m_poPacketHandler; }
	void SetPacketHandler(PacketHandler* poPacketHandler) { m_poPacketHandler = poPacketHandler; }

	int GetRandomLogic();
	ServerVector& GetGateList() { return m_oSrvConf.oGateList; }
	ServerVector& GetLogicList() { return m_oSrvConf.oLogicList; }

	bool LoadServerConfig();
	ServerConfig& GetServerConfig() { return m_oSrvConf; }

	LuaTableSeri* GetLuaTableSeri() { return m_poLuaTableSeri; }
	void SetLuaTableSeri(LuaTableSeri* seri) { m_poLuaTableSeri = seri; }
	void SetLuaSerialize(LuaSerialize* seri) { m_poLuaSerialize = seri; }
	LuaSerialize* GetLuaSerialize() { return m_poLuaSerialize; }

private:
    uint16_t m_uServerID;
	Service* m_poService;
	RouterMgr* m_poRouterMgr;
	PacketHandler* m_poPacketHandler;
	LuaTableSeri* m_poLuaTableSeri;
	LuaSerialize* m_poLuaSerialize;
	
	ServerConfig m_oSrvConf;
	DISALLOW_COPY_AND_ASSIGN(ServerContext);
};

extern ServerContext* gpoContext;

#endif