--服务器管理类
local table, string, math, os, pairs, ipairs, assert = table, string, math, os, pairs, ipairs, assert

function CServerMgr:Ctor()
	self.m_nServerID = nil
	self.m_tServerMap = {} --{[serverid]={serverid=,displayid=,servername=,opentime=}
	self.m_tServiceMap = {} --{[serverid]={}}
	self.m_nGroupID = nil 	--跨服组ID

	self.m_oMgrMysql = nil
	self.m_nUpdateTimer = nil
end

function CServerMgr:OnRelease()
	goTimerMgr:Clear(self.m_nUpdateTimer)
	self.m_nUpdateTimer = nil
end

function CServerMgr:GetGroupID()
	return self.m_nGroupID
end

function CServerMgr:InitNew(nServerID)
	assert(nServerID, "服务ID错误:"..nServerID)
	self.m_nServerID = nServerID

	goTimerMgr:Clear(self.m_nUpdateTimer)
	self.m_nUpdateTimer = nil

	if not self.m_oMgrMysql then
		local oMgrMysql = MysqlDriver:new() 
		local tConf = gtMgrSQL
		local bRes = oMgrMysql:Connect(tConf.ip, tConf.port, tConf.db, tConf.usr, tConf.pwd, "utf8")
		assert(bRes, "连接数据库失败"..tostring(tConf))
		self.m_oMgrMysql = oMgrMysql
	end

	if (gnGroupID or 0) <= 0 then
		assert(self.m_nServerID ~= gnWorldServerID, "本地服服务器ID==世界服服务器ID了:"..self.m_nServerID)
		self.m_oMgrMysql:Query("select groupid from serverlist where serverid="..self.m_nServerID)
		assert(self.m_oMgrMysql:FetchRow(), "服务器:"..self.m_nServerID.."不存在")
		self.m_nGroupID = self.m_oMgrMysql:ToInt32("groupid")
	else
		self.m_nGroupID = gnGroupID
	end
	
	self.m_oMgrMysql:Query("select servergroup,serverid,servername,displayid,opentime,state,dataDistinction,merge from serverlist where groupid="..self.m_nGroupID)
	while self.m_oMgrMysql:FetchRow() do
		local sServerGroup, sServerName = self.m_oMgrMysql:ToString("servergroup", "servername")
		local nServerID, nDisplayID, nOpenTime, nState, nDivision, nMerge 
		= self.m_oMgrMysql:ToInt32("serverid", "displayid", "opentime", "state", "dataDistinction", "merge")
		self.m_tServerMap[nServerID] = {nDisplayID=nDisplayID, nOpenTime=nOpenTime, sServerName=sServerName, nState=nState
		, sServerGroup=sServerGroup, nDivision=nDivision, nMerge=nMerge}
	end

	self.m_oMgrMysql:Query("select serverid,serviceid,servicename from appinfo where groupid="..self.m_nGroupID)
	self.m_tServiceMap = {}
	while self.m_oMgrMysql:FetchRow() do
		local nServerID, nServiceID = self.m_oMgrMysql:ToInt32("serverid", "serviceid")
		local sServiceName = self.m_oMgrMysql:ToString("servicename")
		self.m_tServiceMap[nServerID] = self.m_tServiceMap[nServerID] or {}
		self.m_tServiceMap[nServerID][sServiceName] = self.m_tServiceMap[nServerID][sServiceName] or {}
		table.insert(self.m_tServiceMap[nServerID][sServiceName], nServiceID)
	end

	self.m_nUpdateTimer = goTimerMgr:Interval(60, function() self:InitNew(self.m_nServerID) end)
end

function CServerMgr:GetOpenTime(nServer)
	local tServer = assert(self.m_tServerMap[nServer], "服务器不存在:"..nServer)
	if tServer.nState == 0 then --[0不可用; 1白名单可进; 2对外开放]
		return os.time()
	end
	return tServer.nOpenTime
end

function CServerMgr:GetDisplayID(nServer)
	local tServer = assert(self.m_tServerMap[nServer], "服务器不存在:"..nServer)
	return tServer.nDisplayID
end

function CServerMgr:GetServerID()
	return self.m_nServerID
end

function CServerMgr:GetServerGroup(nServer)
	local tServer = assert(self.m_tServerMap[nServer], "服务器不存在:"..nServer)
	return tServer.sServerGroup
end

function CServerMgr:GetServerName(nServer)
	local tServer = assert(self.m_tServerMap[nServer], "服务器不存在:"..nServer)
	return tServer.sServerName
end

function CServerMgr:GetOpenZeroTime(nServer) 
	local nOpenTime = self:GetOpenTime(nServer)
	local tDate = os.date("*t", nOpenTime)
	tDate.hour, tDate.min, tDate.sec = 0, 0, 0
	return os.time(tDate)
end

--是否区分数据(ios,pc,android): 0不区分; 1区分
function CServerMgr:IsDivisionPlatform(nServer)
	local tServer = assert(self.m_tServerMap[nServer], "服务器不存在:"..nServer)
	return (tServer.nDivision or 0) == 1
end

--是否合服
function CServerMgr:IsMerged(nServer)
	local tServer = self.m_tServerMap[nServer]
	if not tServer then
		LuaTrace("服务器不存在:", nServer)
		return false
	end
	return (tServer.nMerge or 0) == 1
end

--开放天数(1开始)
function CServerMgr:GetOpenDays(nServer)
	local nOpenZeroTime = self:GetOpenZeroTime(nServer)	
	local nPassTime = os.time() - nOpenZeroTime
	return math.max(1, math.ceil(nPassTime/(24*3600)))
end

--取状态(1可用;0不可用)
function CServerMgr:GetServerState(nServer)
	local tServer = self.m_tServerMap[nServer] or {}
	return tServer.nState or 0
end

--服务器等级,返回服务器等级和下一等级时间
function CServerMgr:GetServerLevel(nServer)
	assert(nServer, "参数错误")
	local tCurrConf, tNextConf = nil, nil

	local nDays = self:GetOpenDays(nServer)
	for k=#ctServerLevelConf, 1, -1 do
		local tConf = ctServerLevelConf[k]
		if nDays >= tConf.nDays then
			tCurrConf = tConf
			tNextConf = ctServerLevelConf[k+1]
			break
		end
	end

	--已达等级上限
	if not tNextConf then
		return tCurrConf.nLevel, -1
	end

	--下一等级时间
	local nNextDays = tNextConf.nDays
	local nNextTime= self:GetOpenZeroTime(nServer)+(nNextDays-1)*24*3600
	return tCurrConf.nLevel, nNextTime
end

--服务器等级上限
function CServerMgr:GetMaxServerLevel()
	return ctServerLevelConf[#ctServerLevelConf].nLevel
end

--从所有服务器取最小的服务器等级
function CServerMgr:GetServerMinLevel()
	local nMinLevel
	for nServerID,tData in pairs(self.m_tServerMap) do
		local nServerLevel = self:GetServerLevel(nServerID)
		if not nMinLevel or nMinLevel > nServerLevel then
			nMinLevel = nServerLevel
		end
	end
	return nMinLevel
end

--取该跨服组服务器列表
function CServerMgr:GetServerMap()
	return self.m_tServerMap
end

--逻辑服启动成功
function CServerMgr:OnLogicStart()
	if self.m_nServerID == gnWorldServerID then
		return
	end
	local nServiceID = goServerMgr:GetGlobalService(self.m_nServerID, 20)
	goRemoteCall:Call("OnLogicStart",self.m_nServerID,nServiceID,0)
end


---------------------------------------服务配置
--取路由服务器
function CServerMgr:GetRouterService()
	return self.m_tServiceMap[gnWorldServerID]["ROUTER"][1]
end
--取网关服务列表
function CServerMgr:GetGateServiceList()
	local tList = {}
	if self.m_nServerID < gnWorldServerID then
		local nServiceID = self.m_tServiceMap[self.m_nServerID]["GATE"][1]
		table.insert(tList, {nServer=self.m_nServerID, nID=nServiceID})
	else
		for nServerID, tServiceMap in pairs(self.m_tServiceMap) do
			if nServerID < gnWorldServerID then
				if self:GetServerState(nServerID) > 0 then
					local nServiceID = tServiceMap["GATE"][1]
					table.insert(tList, {nServer=nServerID, nID=nServiceID})
				end
			end
		end
	end
	return tList
end
--取登录服务
function CServerMgr:GetLoginService(nServerID)
	local tServiceList = self.m_tServiceMap[nServerID]["LOGIN"]
	local nServiceID = tServiceList and tServiceList[1]
	assert(nServiceID, "LOGIN不存在:"..nServerID)
	return nServiceID
end

--取登录服务列表
function CServerMgr:GetLoginServiceList()
	local tList = {}
	if self.m_nServerID < gnWorldServerID then
		local nServiceID = self.m_tServiceMap[self.m_nServerID]["LOGIN"][1]
		table.insert(tList, {nServer=self.m_nServerID, nID=nServiceID})
	else
		for nServerID, tServiceMap in pairs(self.m_tServiceMap) do
			if nServerID < gnWorldServerID then
				local nServiceID = tServiceMap["LOGIN"][1]
				table.insert(tList, {nServer=nServerID, nID=nServiceID})
			end
		end
	end
	return tList
end

--取日志服务
function CServerMgr:GetLogService(nServerID)
	local tServiceList = self.m_tServiceMap[nServerID]["LOG"]
	local nServiceID = tServiceList and tServiceList[1]
	assert(nServiceID, "LOG不存在:"..nServerID)
	return nServiceID
end

--取日志服列表
function CServerMgr:GetLogServiceList()
	local tList = {}
	if self.m_nServerID < gnWorldServerID then
		local nServiceID = self.m_tServiceMap[self.m_nServerID]["LOG"][1]
		table.insert(tList, {nServer=self.m_nServerID, nID=nServiceID})
	else
		for nServerID, tServiceMap in pairs(self.m_tServiceMap) do
			if nServerID < gnWorldServerID then
				local nServiceID = tServiceMap["LOG"][1]
				table.insert(tList, {nServer=nServerID, nID=nServiceID})
			end
		end
	end
	return tList
end

--取逻辑服务列表
function CServerMgr:GetLogicServiceList()
	local tList = {}
	if self.m_nServerID < gnWorldServerID then
		local tLogicList = self.m_tServiceMap[self.m_nServerID]["LOGIC"]
		local tWLogicList = self.m_tServiceMap[gnWorldServerID]["WLOGIC"]
		for _, nServiceID in ipairs(tLogicList) do
			table.insert(tList, {nServer=self.m_nServerID, nID=nServiceID})
		end
		for _, nServiceID in ipairs(tWLogicList) do
			table.insert(tList, {nServer=gnWorldServerID, nID=nServiceID})
		end

	else
		local tWLogicList = self.m_tServiceMap[self.m_nServerID]["WLOGIC"]
		for _, nServiceID in ipairs(tWLogicList) do
			table.insert(tList, {nServer=self.m_nServerID, nID=nServiceID})
		end
	end
	return tList
end

--取GLOBAL服务
function CServerMgr:GetGlobalService(nServerID, nServiceID)
	local tList = {}
	if self.m_nServerID < gnWorldServerID then
		if nServerID < gnWorldServerID then
			assert(nServerID == self.m_nServerID, "不能取其他服的全局服")
			local nTmpServiceID = self.m_tServiceMap[nServerID]["GLOBAL"][1]
			assert(nTmpServiceID == nServiceID, "全局服务不存在:"..nServerID.."-"..nServiceID)
			return nTmpServiceID
		end	
		local tWGlobalList = self.m_tServiceMap[nServerID]["WGLOBAL"]
		for _, nTmpServiceID in ipairs(tWGlobalList) do
			if nTmpServiceID == nServiceID then
				return nTmpServiceID
			end
		end
		assert(false, "全局服务不存在:"..nServerID.."-"..nServiceID)

	else
		for nTmpServerID, tServiceMap in pairs(self.m_tServiceMap) do
			local tGlobalList = tServiceMap["GLOBAL"] or tServiceMap["WGLOBAL"] 
			for _, nTmpServiceID in ipairs(tGlobalList) do
				if nServerID == nTmpServerID and nTmpServiceID == nServiceID then
					return nTmpServiceID
				end
			end
		end
		assert(false, "全局服务不存在:"..nServerID.."-"..nServiceID)
	end
end

--取GLBOAL服务列表
--如果有提供nTarServer, 则取和nTarServer相关的全局服务
function CServerMgr:GetGlobalServiceList(nTarServer)
	local tList = {}
	if self.m_nServerID < gnWorldServerID then
		local tGlobalList = self.m_tServiceMap[self.m_nServerID]["GLOBAL"]
		local tWGlobalList = self.m_tServiceMap[gnWorldServerID]["WGLOBAL"]
		for _, nServiceID in ipairs(tGlobalList) do
			table.insert(tList, {nServer=self.m_nServerID, nID=nServiceID})
		end
		for _, nServiceID in ipairs(tWGlobalList) do
			table.insert(tList, {nServer=gnWorldServerID, nID=nServiceID})
		end

	else
		for nServerID, tServiceMap in pairs(self.m_tServiceMap) do
			if nServerID == gnWorldServerID or not nTarServer or nTarServer == nServerID then 
				local tGlobalList = tServiceMap["GLOBAL"] or tServiceMap["WGLOBAL"] 
				for _, nServiceID in ipairs(tGlobalList) do
					table.insert(tList, {nServer=nServerID, nID=nServiceID})
				end
			end
		end

	end
	return tList
end

function CServerMgr:GMTest()
end

goServerMgr = goServerMgr or CServerMgr:new()

