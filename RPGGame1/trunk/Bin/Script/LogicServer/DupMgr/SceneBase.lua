--场景基类
local table, string, math, os, pairs, ipairs, assert = table, string, math, os, pairs, ipairs, assert
--CPP场景管理器
local oNativeSceneMgr = GlobalExport.GetSceneMgr()

function CSceneBase:Ctor(oParentDup, nSceneConfID)
    self.m_oParentDup = oParentDup
    self.m_nSceneConfID = nSceneConfID
    local tDupConf = oParentDup:GetDupConf()
    self.m_nLineRoles = tDupConf.nLineRoles
    self.m_nSceneID = CUtil:GenSceneID(nSceneConfID)

    self.m_oNativeScene = oNativeSceneMgr:CreateScene(self.m_oParentDup:GetDupType(), self.m_nSceneID, self.m_nSceneConfID, self.m_nLineRoles)
    self.m_oNativeScene:BindLuaObj(self)
end

--销毁场景
function CSceneBase:Release()
    oNativeSceneMgr:RemoveScene(self.m_nSceneID)
end

function CSceneBase:GetParentDup() return self.m_oParentDup end
function CSceneBase:GetSceneID() return self.m_nSceneID end
function CSceneBase:GetSceneConfID() return ctSceneConf[self.m_nSceneConfID] end
function CSceneBase:GetSceneConf() return ctSceneConf[self.m_nSceneConfID] end
function CSceneBase:DumpSceneInfo() self.m_oNativeScene:DumpSceneInfo() end
function CSceneBase:GetGameNativeObj(nObjID) return self.m_oNativeScene:GetGameObj(nObjID) end
--将场景内所有类型为nObjType的对象踢出场景
--@nObjType 要踢出场景的对象类型,0为所有
function CSceneBase:KickAllObjs(nObjType) self.m_oNativeScene:KickAllGameObjs(nObjType) end
--添加角色的观察者身份
function CSceneBase:AddObserver(nObjID) return self.m_oNativeScene:AddObserver(nObjID) end
--添加角色的被观察者身份
function CSceneBase:AddObserved(nObjID) return self.m_oNativeScene:AddObserved(nObjID) end
--移除角色的观察者身份
--@bLeaveScene 是否离开场景,如果是就不会收到被观察者离开视野的回调
function CSceneBase:RemoveObserver(nObjID, bLeaveScene) return self.m_oNativeScene:RemoveObserver(nObjID, bLeaveScene) end
--移除角色的被观察者身份
function CSceneBase:RemoveObserved(nObjID) return self.m_oNativeScene:RemoveObserved(nObjID) end
--取场景内某分线所有的角色对象列表
--@nLine -1所有分线;>=0指定分线
--@nObjType: 游戏对象类型,0表示所有
--返回Native对象列表
function CSceneBase:GetGameObjList(nLine, nObjType) return self.m_oNativeScene:GetObjList(nLine, nObjType) end
--取观察该角色的观察者角色对象列表
--@nObjType: 游戏对象类型,0表示所有
--返回Native对象列表
function CSceneBase:GetAreaObservers(nObjID, nObjType) return self.m_oNativeScene:GetAreaObservers(nObjID, nObjType) end
--取该角色观察区域内的角色对象列表
--@nObjType: 游戏对象类型,0表示所有
--返回Native对象列表
function CSceneBase:GetAreaObserveds(nObjID, nObjType) return self.m_oNativeScene:GetAreaObserveds(nObjID, nObjType) end

--广播信息给场景某分线所有角色
--@nLine -1所有分线; >=0指定分线
--@sCmd 指令名
--@tMsg 消息
function CSceneBase:BroadcastScene(nLine, sCmd, tMsg)
    local tObjList = self:GetGameObjList(nLine, gtGDef.tObjType.eRole)
    if #tObjList <= 0 then
        return
    end
    local tSessionList = {}
    for _, oGameNativeObj in ipairs(tObjList) do
        local nServerID = oGameNativeObj:GetServerID()
        local nSessionID = oGameNativeObj:GetSessionID()
        if nServerID > 0 and nSessionID > 0 then
            table.insert(tSessionList, nServerID)
            table.insert(tSessionList, nSessionID)
        end
    end
    Network.PBBroadcastExter(sCmd, tSessionList, tMsg)
end

--广播信息给我的观察者角色
--@nAOID 我的AOI编号
--@sCmd 指令名
--@tMsg 消息
function CSceneBase:BroadcastObserver(nObjID, sCmd, tMsg)
    local tObserverList = self:GetAreaObservers(nObjID, gtGDef.tObjType.eRole)
    if #tObserverList <= 0 then
        return
    end
    local tSessionList = {}
    for _, oObserver in ipairs(tObserverList) do
        local nSessionID = oObserver:GetSessionID()
        if nSessionID > 0 then
            local nServerID = oObserver:GetServerID()
            table.insert(tSessionList, nServerID)
            table.insert(tSessionList, nSessionID)
        end
    end
    Network.PBBroadcastExter(sCmd, tSessionList, tMsg)
end


--进入场景
--@oGameLuaObj: 游戏LUA对象
--@nPosX,nPosY: 坐标
--@nLine: 0公共分线; -1自动分线
--@nFace: 模型朝向
function CSceneBase:EnterScene(oGameLuaObj, nPosX, nPosY, nLine, nFace)
    assert(self.m_oNativeScene, "场景已释放")
    local oGameNativeObj = oGameLuaObj:GetNativeObj()
    assert(oGameNativeObj, "获取对象CPP对象失败")

    local tMapConf = self:GetMapConf()
    nPosX = math.max(10, math.min(nPosX, tMapConf.nWidth-10))
    nPosY = math.max(10, math.min(nPosY, tMapConf.nHeight-10))
    nLine = nLine or -1 --默认为自动分线

    --先离开旧场景
    local nCurrSceneID = oGameLuaObj:GetSceneID()
    if nCurrSceneID == self:GetSceneID() then
        --角色已经在场景中更新坐标和分线
        local nOldPosX, nOldPosY = oGameLuaObj:GetPos()
        if nOldPosX ~= nPosX or nOldPosY ~= nPosY then
            oGameLuaObj:SetPos(nPosX, nPosY)
        end
        local nOldLine = oGameLuaObj:GetLine()
        if nLine > 0 and nLine ~= nOldLine then
            oGameLuaObj:SetLine(nLine)
        end
        return
    end
    if nCurrSceneID > 0 then
        oGameLuaObj:StopRun()
        oGameLuaObj:SetNextSceneID(self.m_nSceneID) --设置将要进入的场景ID
        oGameLuaObj:LeaveScene()
    end

    --掉线的玩家和怪物没有观察者身份
    local nAOIMode = gtAOIType.eObserved
    if oGameLuaObj:IsOnline() then
        nAOIMode = nAOIMode | gtAOIType.eObserver
    end

    --进入新场景
    local nAOIWidth = gtAOISize.eWidth
    local nAOIHeight = gtAOISize.eHeight
    self.m_oNativeScene:EnterScene(self:GetSceneID(), oNativeNativeObj, nPosX, nPosY, nAOIMode, nAOIWidth, nAOIHeight, nLine, nFace)
end

--离开场景
--@nNextSceneID 将要进入的场景ID
function CSceneBase:LeaveScene(oGameLuaObj)
    local nObjID = oGameLuaObj:GetObjID()
    self.m_oNativeScene:LeaveScene(nObjID)
end

--对象进入场景事件
function CSceneBase:OnObjEnterScene(oGameNativeObj)
    local oGameLuaObj = oGameNativeObj:GetLuaObj()
    assert(oGameLuaObj, "游戏对象未绑定LUA对象")
    self.m_oParentDup:OnObjEnterScene(self, oGameLuaObj)
end

--对象离开场景事件
--@bSceneReleasedKick 场景释放提出
function CSceneBase:OnObjLeaveScene(oGameNativeObj, bSceneReleasedKick)
    local oGameLuaObj = oGameNativeObj:GetLuaObj()
    assert(oGameLuaObj, "游戏对象未绑定LUA对象")
    local nNextSceneID = oGameLuaObj:GetNextSceneID()
    oGameLuaObj:SetNextSceneID(nil)
    self.m_oParentDup:OnObjLeaveScene(self, oGameLuaObj, bSceneReleasedKick, nNextSceneID)
end

function CSceneBase:OnObjEnterObj(tObserver, tObserved)
    self.m_oParentDup:OnObjEnterObj(self, tObserver, tObserved)
end

function CSceneBase:OnObjLeaveObj(tObserver, tObserved)
    self.m_oParentDup:OnObjLeaveObj(self, tObserver, tObserved)
end

function CSceneBase:OnObjReachTargetPos(oGameNativeObj, nPosX, nPosY)
    local oGameLuaObj = oGameNativeObj:GetLuaObj()
    assert(oGameLuaObj, "游戏对象未绑定LUA对象")
    self.m_oParentDup:OnObjReachTargetPos(self, oGameLuaObj, nPosX, nPosY)
end

