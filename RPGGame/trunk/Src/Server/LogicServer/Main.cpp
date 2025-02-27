﻿//#include <vld.h>

#include "Server/LogicServer/LogicServer.h"
#include "Include/Network/Network.hpp"
#include "Include/DBDriver/DBDriver.hpp"
#include "Common/DataStruct/XMath.h"
#include "Common/DataStruct/XTime.h"
#include "Common/DataStruct/TimeMonitor.h"
#include "Common/TimerMgr/TimerMgr.h"
#include "LuaSupport/LuaExport.h"
#include "Server/Base/ServerContext.h"
#include "Server/LogicServer/ConfMgr/ConfMgr.h"
#include "Server/LogicServer/Component/Battle/BattleUtil.h"
#include "Server/LogicServer/PacketProc/LogicPacketProc.h"

ServerContext* gpoContext;

//网络初始化
bool InitNetwork(int8_t nServiceID)
{
	LogicNode* poNode = NULL;
	ServerConfig& oSrvConf = gpoContext->GetServerConfig();
	for (int i = 0; i < oSrvConf.oLogicList.size(); i++)
	{
		if (oSrvConf.oLogicList[i].uServer == oSrvConf.uServerID && oSrvConf.oLogicList[i].uID == nServiceID)
		{
			poNode = &oSrvConf.oLogicList[i];
			break;
		}
	}
	if (poNode == NULL)
	{
		XLog(LEVEL_ERROR, "LogicServer conf:%d not found\n", nServiceID);
		return false;
	}

	gpoContext->GetRouterMgr()->InitRouters();
	return true;
}

//LUA死循环检测
static XThread goMonitorThread;
static void MonitorThreadFunc(void* pParam)
{
	uint32_t uLastMainLoops = 0;
	uint32_t uNowMainLoops = 0;
	uint32_t nTimeCount = 0;
	while (gpoContext != NULL)
	{
		XTime::MSSleep(1000);
		if (gpoContext == NULL)
		{
			break;
		}
		if (++nTimeCount < 30)
		{
			continue;
		}
		nTimeCount = 0;
		uNowMainLoops = gpoContext->GetService()->GetMainLoopCount();
		if (uNowMainLoops == uLastMainLoops && !LuaWrapper::Instance()->IsBreaking())
		{
			XLog(LEVEL_ERROR, "May endless loop!!!\n");
			LuaWrapper::Instance()->SetEndlessLoop(1);
		}
		uLastMainLoops = uNowMainLoops;
	}
}

//启动脚本虚拟机
void StartScriptEngine()
{
	XLog(LEVEL_INFO, "Start script engine...\n");
	static bool bStarted = false;
	if (bStarted)
		return;
	bStarted = true;
	LuaWrapper* poLuaWrapper = LuaWrapper::Instance();

	bool bDebug = false;
#ifdef _DEBUG
	bDebug = true;
#endif
	lua_pushboolean(poLuaWrapper->GetLuaState(), bDebug);
	lua_setglobal(poLuaWrapper->GetLuaState(), "gbDebug");

	OpenLuaExport();
	bool bRes = poLuaWrapper->DoFile("LogicServer/Main");
	assert(bRes);
	if (!bRes)
	{
		exit(-1);
	}
	bRes = poLuaWrapper->CallLuaFunc(NULL, "Main");
	assert(bRes);
	if (!bRes)
	{
		exit(-1);
	}

	Logger::Instance()->SetSync(false);
	goMonitorThread.Create(MonitorThreadFunc, NULL);
}

void ExitFunc(void)
{
	XTime::MSSleep(1000);
}

void OnSigTerm(int)
{
	XLog(LEVEL_INFO, "receive sigterm signal!\n");
}

void OnSigInt(int)
{
	if (gpoContext != NULL)
	{
		gpoContext->GetService()->Terminate();
	}
}


int main(int nArg, char *pArgv[])
{
	assert(nArg >= 2);
	signal(SIGINT, OnSigInt);
	signal(SIGTERM, OnSigTerm);
	int8_t nServiceID = (int8_t)atoi(pArgv[1]);
#ifdef _WIN32
	::SetUnhandledExceptionFilter(Platform::MyUnhandledFilter);
#endif
	XMath::RandomSeed((uint32_t)XTime::MSTime());
	atexit(ExitFunc);
	Logger::Instance()->Init();
	Logger::Instance()->SetSync(true);
	MysqlDriver::MysqlLibaryInit();

	LuaWrapper* poLuaWrapper = LuaWrapper::Instance();
	poLuaWrapper->Init(Platform::FileExist("./adb.txt"));
	char szWorkDir[256] = {0};
	char szScriptPath[512] = {0};
	Platform::GetWorkDir(szWorkDir, sizeof(szWorkDir)-1);
	sprintf(szScriptPath, ";%s/Script/?.lua;%s/../Script/?.lua;", szWorkDir, szWorkDir);
    poLuaWrapper->AddSearchPath(szScriptPath);

	gpoContext = XNEW(ServerContext);
	bool bRes = gpoContext->LoadServerConfig();
	assert(bRes);
	if (!bRes)
	{
		XLog(LEVEL_ERROR, "load server conf fail!\n");
		exit(-1);
	}
	ConfMgr::Instance()->LoadConf(gpoContext->GetServerConfig().sDataPath);

	NetAPI::StartupNetwork();
	if (!Platform::FileExist("./debug.txt"))
	{
		char szLogName[128] = "";
		sprintf(szLogName, "logicserver%d", nServiceID);
		Logger::Instance()->SetLogFile(gpoContext->GetServerConfig().sLogPath, szLogName);
	}

	RouterMgr* poRouterMgr = XNEW(RouterMgr);
	gpoContext->SetRouterMgr(poRouterMgr);

	PacketHandler* poPacketHandler = XNEW(PacketHandler);
	gpoContext->SetPacketHandler(poPacketHandler);

	NSPacketProc::RegisterPacketProc();

	LogicServer* poService = XNEW(LogicServer);
	bRes = poService->Init(nServiceID);
	assert(bRes);
	if (!bRes)
	{
		XLog(LEVEL_ERROR, "init service fail!\n");
		exit(-1);
	}
	gpoContext->SetService(poService);

	LuaSerialize* poSerialize = XNEW(LuaSerialize);
	gpoContext->SetLuaSerialize(poSerialize);

	bRes = InitNetwork(nServiceID);
	assert(bRes);
	if (!bRes)
	{
		XLog(LEVEL_ERROR, "init network fail!\n");
		exit(-1);
	}

	XLog(LEVEL_INFO, "LogicServer start successful\n");
	assert(bRes);
	bRes = poService->Start();
	if (!bRes)
	{
		XLog(LEVEL_ERROR, "start server fail!\n");
		exit(-1);
	}

	//wchar_t wcBuffer[256] = {L""};
	//wsprintfW(wcBuffer, L"logic%d.leak", gpoContext->GetService()->GetServiceID());
	//VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE|VLD_OPT_REPORT_TO_DEBUGGER, wcBuffer);

	SAFE_DELETE(gpoContext);
	TimerMgr::Instance()->Release();
	LuaWrapper::Instance()->Release();
	ConfMgr::Instance()->Release();
	Logger::Instance()->Terminate();
	NetAdapter::Release();
	goMonitorThread.Join();
	MysqlDriver::MysqlLibaryEnd();
	return 0;
}