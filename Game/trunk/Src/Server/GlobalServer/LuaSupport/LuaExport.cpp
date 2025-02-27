﻿#include "Include/DBDriver/DBDriver.hpp"
#include "Include/Lpeg/Lpeg.hpp"
#include "Include/Luacjson/Luacjson.hpp"
#include "Include/Pbc/Pbc.hpp"
#include "Include/Script/Script.hpp"

#include "Common/DataStruct/XUUID.h"
#include "Common/LuaCommon/LuaCmd.h"
#include "Common/LuaCommon/LuaRpc.h"
#include "Common/LuaCommon/LuaPB.h"
#include "Common/TimerMgr/TimerMgr.h"
#include "Server/Base/NetworkExport.h"
#include "Server/Base/ServerContext.h"
#include "Server/GlobalServer/GlobalServer.h"

//////////////////////////Global funcitons/////////////////////////////
//服务ID
int GetServiceID(lua_State* pState)
{
	int nService = gpoContext->GetService()->GetServiceID();
	lua_pushinteger(pState, nService);
	return 1;
}

//生成唯一ID
int MakeObjID(lua_State* pState)
{
	int64_t nID = XUUID::GenID(gpoContext->GetService()->GetServiceID());
	lua_pushinteger(pState,nID);
	return 1;
}

luaL_Reg _global_lua_func[] =
{
	{ "GetServiceID", GetServiceID},
	{ "MakeGameObjID", MakeObjID},
	{ NULL, NULL },
};


void OpenLuaExport()
{
	LuaWrapper* poWrapper = LuaWrapper::Instance();

	RegLuaDebugger(NULL);
	RegTimerMgr("GlobalExport");
	poWrapper->RegFnList(_global_lua_func, "GlobalExport");


    RegLuaCmd("NetworkExport");
    RegLuaRpc("NetworkExport");
	RegLuaPBPack("NetworkExport");
	RegLuaNetwork("NetworkExport");

	RegClassSSDBDriver();
	RegClassMysqlDriver();

	luaopen_lpeg(poWrapper->GetLuaState());
	luaopen_protobuf_c(poWrapper->GetLuaState());
	luaopen_cjson(poWrapper->GetLuaState());
	luaopen_cjson_raw(poWrapper->GetLuaState());
}
