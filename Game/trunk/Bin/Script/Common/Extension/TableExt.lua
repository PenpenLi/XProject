--Gen strict table
function table.new()
	local tNew = {}
	AddStrict(tNew)
	return tNew
end

--Print table
function table.Print(root, bNotExpend)
	local sTable = table.ToString(root, bNotExpend)
	print(sTable)
end

--Table to string 
function table.ToString(root, bNotExpend)
	if not(type(root)=="table") then
		local sErr = "What you input is not a table: "..root
		return sErr
	else
		local paths = {}
		local type = type
		local pairs = pairs
		local print = print
		local srep = string.rep
		local tostring = tostring
		local tconcat = table.concat
		local tinsert = table.insert

		local name = "root"
		local space = bNotExpend and 0 or 2
		local newline = bNotExpend and "" or "\n"
		local function ttos(tb, indent, name)
			paths[tb] = name
			local chgs = {"{"}
			for k,v in pairs(tb) do
				local key = tostring(k)
				--local head = "["..(type(k)=="string" and "\""..key.."\"" or key).."]="
				local head = (type(k)=="string" and key or ("["..key.."]")).."="
				if paths[v] then
					tinsert(chgs, head..paths[v]..",")
				elseif type(v)=="table" then
					tinsert(chgs, head..ttos(v, indent+space, name..(type(k)=="string" and "."..key or "["..key.."]"))..",")
				elseif type(v)=="string" then
					tinsert(chgs, head.."\""..v.."\",")
				else
					tinsert(chgs, head..tostring(v)..",")
				end
			end
			return tconcat(chgs, newline..srep(" ", indent+space))..newline..srep(" ", indent).."}"
		end
		local sTable = ttos(root, 0, name)
		return sTable
	end
end

--Deep copy
function table.DeepCopy(tTable, bIpairs)
	local iter = bIpairs and ipairs or pairs
	local tCopy = {}
	for k, v in iter(tTable) do
		assert(type(k) ~= "table")
		if type(v) == "table" then
			tCopy[k] = table.DeepCopy(v, bIpairs)
		else
			tCopy[k] = v
		end
	end
	return tCopy
end

--In array
function table.InArray(Val, tArray)
	for _, v in ipairs(tArray) do
		if Val == v then
			return true
		end
	end
	return false
end
