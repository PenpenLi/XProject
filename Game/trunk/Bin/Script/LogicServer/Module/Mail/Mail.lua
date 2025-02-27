local table, string, math, os, pairs, ipairs, assert = table, string, math, os, pairs, ipairs, assert
local nMaxMailCount = ctMailConf[1].nMaxMail

function CMail:Ctor(oPlayer)
	self.m_oPlayer = oPlayer
	--[id]={charname, title, tItems, time, readed}
	self.m_tMailMap = {}
	self.m_nCount = 0
	self.m_bDirty = false
end

function CMail:LoadData(tData)
	self.m_tMailMap = tData.tMailMap
	self.m_nCount = tData.nCount
end

function CMail:SaveData()
	if not self.m_bDirty then
		return
	end
	local tData = {}
	tData.tMailMap = self.m_tMailMap
	tData.nCount = self.m_nCount
	self.m_bDirty = false
	return tData
end

function CMail:GetType()
	return gtModuleDef.tMail.nID, gtModuleDef.tMail.sName
end

function CMail:MarkDirty(bDirty)
	self.m_bDirty = bDirty
end

function CMail:_do_recv_mail(nCharID, tMailMap, nCount, nMailID, sSenderName, sTitle, tItems)
	assert(not tMailMap[nMailID], "邮件ID冲突")
	if nCount >= nMaxMailCount then
		local sMail = string.format("邮件满了 %s: %d %s %s %s", nCharID, nMailID, sSenderName, sTitle, cjson.encode(tItems))
		LuaTrace(sMail)
		return
	end
	tMailMap[nMailID] = {sSenderName, sTitle, tItems, os.time(), 0}
	return true
end

function CMail:RecvMail(nMailID, sSenderName, sTitle, tItems)
	if self:_do_recv_mail(self.m_oPlayer:GetCharID(), self.m_tMailMap, self.m_nCount, nMailID, sSenderName, sTitle, tItems) then
		self.m_nCount = self.m_nCount + 1
		self:SyncMailList()
		self:MarkDirty(true)
		return true
	end
end

function CMail:RecvMailOffline(nCharID, nMailID, sSenderName, sTitle, tItems)
    local _, sModuleName = self:GetType()
    local sData = goSSDB:HGet(sModuleName, nCharID)
    local tData
    if sData ~= "" then
    	tData = cjson.decode(sData)
    else
    	tData = {tMailMap={}, nCount= 0}
    end
    if self:_do_recv_mail(nCharID, tData.tMailMap, tData.nCount, nMailID, sSenderName, sTitle, tItems) then
		tData.nCount = tData.nCount + 1
		goSSDB:HSet(sModuleName, nCharID, cjson.encode(tData))
		return true
	end
end

--同步邮件列表
function CMail:SyncMailList()
	local tMailList = {}
	for nMailID, tMail in pairs(self.m_tMailMap) do
		local tInfo = {}
		tInfo.nMailID = nMailID
		tInfo.sSenderName = tMail[1]
		tInfo.sTitle = tMail[2]
		tInfo.tItems = {}
		for _, v in ipairs(tMail[3]) do
			local tItem = {nType=v[1], nID=v[2], nNum=v[3]}
			table.insert(tInfo.tItems, tItem)
		end
		tInfo.nTime = tMail[4]
		tInfo.nReaded = tMail[5]
		table.insert(tMailList, tInfo)
	end
	print("CMail:MailListReq***", tMailList)
	CmdNet.PBSrv2Clt(self.m_oPlayer:GetSession(), "MailListRet", {tMailList=tMailList})
end

--删除邮件
function CMail:DelMailReq(nMailID)
	--删除指定
	if nMailID > 0 then
		if self.m_tMailMap[nMailID] then
			self.m_tMailMap[nMailID] = nil
			self.m_nCount = self.m_nCount - 1 
			goMailMgr:DelMailBody(self.m_oPlayer:GetCharID(), nMailID)
			self.m_oPlayer:Tips(ctLang[27])
		end
	else
	--删除所有
		local nCount = self.m_nCount
		for nMailID, tMail in pairs(self.m_tMailMap) do
			if tMail[5] > 0 then
				self.m_tMailMap[nMailID] = nil
				self.m_nCount = self.m_nCount - 1
				goMailMgr:DelMailBody(self.m_oPlayer:GetCharID(), nMailID)
			end
		end
		if nCount ~= self.m_nCount then
			self.m_oPlayer:Tips(ctLang[27])
		end
	end
	self:MarkDirty(true)
end

--领取物品
function CMail:MailItemsReq(nMailID)
	local function _get_mail_item_(tMail)
		local bRes = true
		for _, tItem in ipairs(tMail[3]) do
			if not self.m_oPlayer:AddItem(tItem[1], tItem[2], tItem[3], gtReason.eGetMailItem) then
				bRes = false
			end
		end
		tMail[3], tMail[5] = {}, 1
		return bRes
	end

	--领取指定
	local nGotNum = 0
	if nMailID > 0 then
		local tMail = self.m_tMailMap[nMailID]
		if tMail and #tMail[3] > 0 then
			if _get_mail_item_(tMail) then
				nGotNum = nGotNum + 1
			end
		end
	--领取所有
	else
		for nMailID, tMail in pairs(self.m_tMailMap) do
			if #tMail[3] > 0 then
				if _get_mail_item_(tMail) then
					nGotNum = nGotNum + 1
				else
					break
				end
			end
		end
	end
	if nGotNum > 0 then
		self.m_oPlayer:Tips(ctLang[28])
	end
	self:MarkDirty(true)
end

--请求邮件列表
function CMail:MailListReq()
	self:SyncMailList()
end

--请求邮件体
function CMail:MailBodyReq(nMailID)
	local tMail = self.m_tMailMap[nMailID]
	if not tMail then
		return
	end
	tMail[5] = 1
	local sMailBody = goMailMgr:GetMailBody(self.m_oPlayer:GetCharID(), nMailID)
	CmdNet.PBSrv2Clt(self.m_oPlayer:GetSession(), "MailBodyRet", {nMailID=nMailID, sMailBody=sMailBody})
	self:MarkDirty(true)
end