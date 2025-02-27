﻿#include "RoleMgr.h"
#include "Server/LogicServer/LogicServer.h"

LUNAR_IMPLEMENT_CLASS(RoleMgr)
{
	LUNAR_DECLARE_METHOD(RoleMgr, CreateRole),
	LUNAR_DECLARE_METHOD(RoleMgr, RemoveRole),
	LUNAR_DECLARE_METHOD(RoleMgr, GetRole),
	{0, 0}
};


RoleMgr::RoleMgr()
{
}

RoleMgr::~RoleMgr()
{
	RoleIter iter = m_oRoleIDMap.begin();
	for (iter; iter != m_oRoleIDMap.end(); iter++)
	{
		SAFE_DELETE(iter->second);
	}
	m_oRoleSSMap.clear();
}

Role* RoleMgr::CreateRole(int nID, int nConfID, const char* psName, uint16_t uServer, int nSession)
{
	Role* poRole = GetRoleByID(nID);
	if (poRole != NULL)
	{
		XLog(LEVEL_ERROR, "CreateRole error for role id:%d exist\n", nID);
		return poRole;
	}

	if (nSession > 0)
	{
		poRole = GetRoleBySS(uServer, nSession);
		if (poRole != NULL)
		{
			XLog(LEVEL_ERROR, "CreateRole error for role server:%d session:%d exist\n", uServer, nSession);
			return NULL;
		}
	}

	poRole = XNEW(Role);
	poRole->Init(nID, nConfID, psName);
	poRole->SetServer(uServer);
	poRole->SetSession(nSession);

	m_oRoleIDMap[nID] = poRole;
	m_oRoleSSMap[GenSSKey(uServer,nSession)] = poRole;
	return poRole;
}

void RoleMgr::RemoveRole(int nID)
{
	RoleIter iter = m_oRoleIDMap.find(nID);
	if (iter == m_oRoleIDMap.end())
	{
		return;
	}

	Role* poRole = iter->second;
	if (poRole->GetScene() != NULL)
	{
		XLog(LEVEL_ERROR, "Remove role must leave scene first\n");
		return;
	}

	m_oRoleIDMap.erase(iter);

	int nSession = poRole->GetSession();
	if (nSession > 0)
	{
		uint16_t uServer = poRole->GetServer();
		m_oRoleSSMap.erase(GenSSKey(uServer, nSession));
	}

	SAFE_DELETE(poRole);
}

Role* RoleMgr::GetRoleByID(int nID)
{
	RoleIter iter = m_oRoleIDMap.find(nID);
	if (iter != m_oRoleIDMap.end())
	{
		return iter->second;
	}
	return NULL;
}

Role* RoleMgr::GetRoleBySS(uint16_t uServer, int nSession)
{
	int64_t nSSKey = GenSSKey(uServer, nSession);
	RoleSSIter iter = m_oRoleSSMap.find(nSSKey);
	if (iter != m_oRoleSSMap.end())
	{
		return iter->second;
	}
	return NULL;
}

void RoleMgr::BindSession(int nID, int nSession)
{
	Role* poRole = GetRoleByID(nID);
	if (poRole == NULL)
		return;

	int nOldSession = poRole->GetSession();
	if (nOldSession == nSession)
		return;

	poRole->SetSession(nSession);

	if (nOldSession > 0)
	{
		int64_t nOldSSKey = GenSSKey(poRole->GetServer(), nOldSession);
		m_oRoleSSMap.erase(nOldSSKey);

		LogicServer* poLogic = (LogicServer*)(gpoContext->GetService());
		poLogic->OnClientClose(poRole->GetServer(), nOldSession>>SERVICE_SHIFT, nOldSession);
	}

	if (nSession > 0)
	{
		int64_t nNewSSKey = GenSSKey(poRole->GetServer(), nSession);
		m_oRoleSSMap[nNewSSKey] = poRole;
	}
}

void RoleMgr::Update(int64_t nNowMS)
{
	static int64_t nLastMSTime = 0;
	if (nNowMS - nLastMSTime < 30)
	{
		return;
	}
	nLastMSTime = nNowMS;

	int nRoleCount = 0;
	RoleIter iter = m_oRoleIDMap.begin();
	RoleIter iter_end = m_oRoleIDMap.end();
	for (; iter != iter_end; iter++)
	{
		Role* poRole = iter->second;
		if (poRole->GetScene() != NULL)
		{
			poRole->Update(nNowMS);
		}
		nRoleCount++;
	}	

	static int64_t nLastDumpTime = 0;
	if (nNowMS-nLastDumpTime >= 60000)
	{
		nLastDumpTime = nNowMS;
		XLog(LEVEL_INFO, "CPP current role count=%d\n", nRoleCount);
	}
}




//////////////////////lua export//////////////////
void RegClassRole()
{
	REG_CLASS(Actor, false, NULL); 
	REG_CLASS(Role, false, NULL); 
	REG_CLASS(RoleMgr, false, NULL); 
}

int RoleMgr::CreateRole(lua_State* pState)
{
	int nRoleID = (int)luaL_checkinteger(pState, 1);
	int nConfID = (int)luaL_checkinteger(pState, 2);
	const char* psName = luaL_checkstring(pState, 3);
	uint16_t uServer = (int16_t)luaL_checkinteger(pState, 4);
	int nSession = (int)luaL_checkinteger(pState, 5);
	Role* poRole = CreateRole(nRoleID, nConfID, psName, uServer, nSession);
	if (poRole != NULL)
	{
		Lunar<Role>::push(pState, poRole);
		return 1;
	}
	return 0;
}

int RoleMgr::RemoveRole(lua_State* pState)
{
	int nRoleID = (int)luaL_checkinteger(pState, 1);
	RemoveRole(nRoleID);
	return 0;
}

int RoleMgr::GetRole(lua_State* pState)
{
	int nRoleID = (int)luaL_checkinteger(pState, 1);
	Role* poRole = GetRoleByID(nRoleID);
	if (poRole != NULL)
	{
		Lunar<Role>::push(pState, poRole);
		return 1;
	}
	return 0;
}

