﻿#include "Include/DBDriver/SSDBDriver.h"
#include "Include/Logger/Logger.h"

SSDBDriver::SSDBDriver(lua_State* pState)
{
	m_sIP[0] = '\0';
	m_sPwd[0] = '\0';
	m_uPort = 0;
	m_poSSDBClient = NULL;
}

SSDBDriver::~SSDBDriver()
{
	SAFE_DELETE(m_poSSDBClient);
	XLog(LEVEL_INFO, "SSDBDriver destruct!\n");
}

int SSDBDriver::Connect(lua_State* pState)
{
	const char* psIP = luaL_checkstring(pState, 1);
	strcpy(m_sIP, psIP);
	m_uPort = (uint16_t)luaL_checkinteger(pState, 2);
	SAFE_DELETE(m_poSSDBClient);
#ifdef __linux
	m_poSSDBClient = ssdb::Client::connect(m_sIP, m_uPort);
#else
	m_poSSDBClient = XNEW(SSDBClient);
	if (m_poSSDBClient != NULL)
	{
		m_poSSDBClient->connect(m_sIP, m_uPort);
		if (!m_poSSDBClient->isConnect())
		{
			SAFE_DELETE(m_poSSDBClient);
		}
	}
#endif
	if (m_poSSDBClient == NULL)
	{
		LuaWrapper::luaM_error(pState, "Connect SSDB %s:%d fail", m_sIP, m_uPort);
		return 0;
	}
	lua_pushboolean(pState, 1);
	return 1;
}

int SSDBDriver::Auth(lua_State* pState)
{
	const char* pwd = luaL_checkstring(pState, 1);
	if (!Auth(pwd))
	{
		return LuaWrapper::luaM_error(pState, "ssdb auth fail");
	}
	lua_pushboolean(pState, 1);
	return 1;
}

bool SSDBDriver::Auth(const std::string& pwd)
{
#ifdef __linux
	const std::vector<std::string> *resp;
	resp = m_poSSDBClient->request("auth", pwd);
	ssdb::Status oStatus(resp);
#else
	Status oStatus = m_poSSDBClient->auth(pwd);
#endif
	if (!oStatus.ok())
	{
		XLog(LEVEL_ERROR, "ssdb 认证失败\n");
		return false;
	}
	strcpy(m_sPwd, pwd.c_str());
	return true;
}

int SSDBDriver::HSet(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	size_t nSize = 0;
	const char* psKey = luaL_checklstring(pState, 2, &nSize);
	std::string oStrKey(psKey, nSize);
	if (oStrKey == "")
	{
		return LuaWrapper::luaM_error(pState, "HSet key empty");
	}
	nSize = 0;
	const char* psVal = luaL_checklstring(pState, 3, &nSize);
	std::string oStrVal(psVal, nSize);
	if (nSize >= 32 * 1024 * 1024)
	{
		XLog(LEVEL_ERROR, "数据超出单次上限32M size:%d !!!\n", nSize);
	}
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hset(psDB, oStrKey, oStrVal);
#else
	Status oStatus = m_poSSDBClient->hset(psDB, oStrKey, oStrVal);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hset(psDB, oStrKey, oStrVal);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	return 0;
}

int SSDBDriver::HGet(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	size_t nSize = 0;
	const char* psKey = luaL_checklstring(pState, 2, &nSize);
	std::string oStrKey(psKey, nSize);
	std::string oStrVal;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hget(psDB, oStrKey, &oStrVal);
#else
	Status oStatus = m_poSSDBClient->hget(psDB, oStrKey, &oStrVal);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HGet %s %s error: %s\n", psDB, oStrKey.c_str(), oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hget(psDB, oStrKey, &oStrVal);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushlstring(pState, oStrVal.c_str(), oStrVal.size());
	return 1;
}

int SSDBDriver::HSize(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	int64_t nSize = 0;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hsize(psDB, &nSize);
#else
	Status oStatus = m_poSSDBClient->hsize(psDB, &nSize);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HSize %s error: %s\n", psDB, oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hsize(psDB, &nSize);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushinteger(pState, nSize);
	return 1;
}

int SSDBDriver::HKeys(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	std::vector<std::string> oVecKeys;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hkeys(psDB, "", "", -1, &oVecKeys);
#else
	Status oStatus = m_poSSDBClient->hkeys(psDB, "", "", -1, &oVecKeys);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HKeys %s error: %s\n", psDB, oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hkeys(psDB, "", "", -1, &oVecKeys);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_newtable(pState);
	for (int i = 0; i < (int)oVecKeys.size(); i++)
	{
		std::string& oStrKey = oVecKeys[i];
		lua_pushlstring(pState, oStrKey.c_str(), oStrKey.size());
		lua_rawseti(pState, -2, i+1);
	}
	return 1;
}

int SSDBDriver::HScan(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	std::vector<std::string> oVecResult;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hscan(psDB, "", "", -1, &oVecResult);
#else
	Status oStatus = m_poSSDBClient->hscan(psDB, "", "", -1, &oVecResult);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HScan %s error: %s\n", psDB, oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hscan(psDB, "", "", -1, &oVecResult);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_newtable(pState);
	for(int i = 0; i < (int)oVecResult.size(); i++)
	{
		if(i % 2 == 0)
		{
			std::string& oStrKey = oVecResult[i];
			lua_pushlstring(pState, oStrKey.c_str(), oStrKey.size());
		}
		else
		{
			std::string& oStrVal = oVecResult[i];
			lua_pushlstring(pState, oStrVal.c_str(), oStrVal.size());
			lua_settable(pState, -3);
		}
	}
	return 1;
}

int SSDBDriver::HDel(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	size_t nSize = 0;
	const char* psKey = luaL_checklstring(pState, 2, &nSize);
	std::string oStrKey(psKey, nSize);
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hdel(psDB, oStrKey);
#else
	Status oStatus = m_poSSDBClient->hdel(psDB, oStrKey);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HDel %s %s error: %s\n", psDB, oStrKey.c_str(), oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hdel(psDB, oStrKey);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushboolean(pState, 1);
	return 1;
}

int SSDBDriver::HClear(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	int64_t nRet = 0;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hclear(psDB, &nRet);
#else
	Status oStatus = m_poSSDBClient->hclear(psDB, &nRet);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HClear %s error: %s\n", psDB, oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hclear(psDB, &nRet);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushboolean(pState, 1);
	return 1;
}

int SSDBDriver::HIncr(lua_State* pState)
{
	const char* psDB = luaL_checkstring(pState, 1);
	const char* psKey = luaL_checkstring(pState, 2);
	std::string oStrKey(psKey);
	int64_t nRet = 0;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->hincr(psDB, oStrKey, 1, &nRet);
#else
	Status oStatus = m_poSSDBClient->hincr(psDB, oStrKey, 1, &nRet);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "HIncr %s %s error: %s\n", psDB, oStrKey.c_str(), oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->hincr(psDB, oStrKey, 1, &nRet);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushinteger(pState, nRet);
	return 1;
}

int SSDBDriver::Setnx(lua_State* pState)
{
	size_t nSize = 0;
	const char* psKey = luaL_checklstring(pState, 1, &nSize);
	std::string oStrKey(psKey, nSize);
	if (oStrKey == "")
	{
		return LuaWrapper::luaM_error(pState, "Setnx key empty");
	}
	nSize = 0;
	const char* psVal = luaL_checklstring(pState, 2, &nSize);
	std::string oStrVal(psVal, nSize);

	std::string oRet;
#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->setnx(oStrKey, oStrVal, &oRet);
#else
	Status oStatus = m_poSSDBClient->setnx(oStrKey, oStrVal, &oRet);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->setnx(oStrKey, oStrVal, &oRet);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushstring(pState, oRet.c_str());
	return 1;
}

int SSDBDriver::Del(lua_State* pState)
{
	size_t nSize = 0;
	const char* psKey = luaL_checklstring(pState, 1, &nSize);
	std::string oStrKey(psKey, nSize);

#ifdef __linux
	ssdb::Status oStatus = m_poSSDBClient->del(oStrKey);
#else
	Status oStatus = m_poSSDBClient->del(oStrKey);
#endif
	if (!oStatus.ok() && !oStatus.not_found())
	{
		XLog(LEVEL_ERROR, "Del %s error: %s\n", oStrKey.c_str(), oStatus.code().c_str());
		if (CheckReconnect(oStatus))
		{
			oStatus = m_poSSDBClient->del(oStrKey);
		}
		if (!oStatus.ok() && !oStatus.not_found())
		{
			return LuaWrapper::luaM_error(pState, oStatus.code().c_str());
		}
	}
	lua_pushboolean(pState, 1);
	return 1;
}

#ifdef __linux
bool SSDBDriver::CheckReconnect(ssdb::Status& oStatus)
#else
bool SSDBDriver::CheckReconnect(Status& oStatus)
#endif
{
#ifdef __linux
	if (!oStatus.disconnected())
	{
		return false;
	}
#else
	if (m_poSSDBClient->isConnect())
	{
		return false;
	}
#endif

#ifdef __linux
	ssdb::Client* poSSDBClient = ssdb::Client::connect(m_sIP, m_uPort);
#else
	SSDBClient* poSSDBClient = XNEW(SSDBClient);
	if (poSSDBClient != NULL)
	{
		poSSDBClient->connect(m_sIP, m_uPort);
		if (!poSSDBClient->isConnect())
		{
			SAFE_DELETE(poSSDBClient);
		}
	}
#endif
	if (poSSDBClient == NULL)
	{
		XLog(LEVEL_ERROR, "Reconnect ssdb ip:%s port:%d fail!\n", m_sIP, m_uPort);
		return false;
	}
	SAFE_DELETE(m_poSSDBClient);
	m_poSSDBClient = poSSDBClient;
	XLog(LEVEL_ERROR, "Reconnect ssdb success ip:%s port:%d successful!\n", m_sIP, m_uPort);
	if (m_sPwd[0] != '\0')
	{
		Auth(m_sPwd);
	}
	return true;
}


// Lua export 
char SSDBDriver::className[] = "SSDBDriver";
Lunar<SSDBDriver>::RegType SSDBDriver::methods[] =
{
	LUNAR_DECLARE_METHOD(SSDBDriver, Connect),
	LUNAR_DECLARE_METHOD(SSDBDriver, Auth),
	LUNAR_DECLARE_METHOD(SSDBDriver, HGet),
	LUNAR_DECLARE_METHOD(SSDBDriver, HSet),
	LUNAR_DECLARE_METHOD(SSDBDriver, HSize),
	LUNAR_DECLARE_METHOD(SSDBDriver, HKeys),
	LUNAR_DECLARE_METHOD(SSDBDriver, HScan),
	LUNAR_DECLARE_METHOD(SSDBDriver, HDel),
	LUNAR_DECLARE_METHOD(SSDBDriver, HClear),
	LUNAR_DECLARE_METHOD(SSDBDriver, HIncr),
	LUNAR_DECLARE_METHOD(SSDBDriver, dispose),
	LUNAR_DECLARE_METHOD(SSDBDriver, Setnx),
	LUNAR_DECLARE_METHOD(SSDBDriver, Del),
	{0,0}
};


// Reg ssdb to mysql
void RegClassSSDBDriver()
{
	REG_CLASS(SSDBDriver, true, NULL); 
}