local event = require "event"
local bson = require "bson"
local driver = require "mongo.driver"
local channel = require "channel"

local empty_bson = bson.encode({})

local mongo_channel = channel:inherit()

function mongo_channel:disconnect()
	print("mongo_channel closed")
end

function mongo_channel:init(db)
	self.session_ctx = {}
end

function mongo_channel:data(data,size)
	local data = string.copy(data,size)
	local result = {}
	local succ,	session, document, cursor, _ = driver.reply(data,result)
	local session_ctx = self.session_ctx[session]

	if not succ then
		if session_ctx.callback then
			session_ctx.callback(false,bson.decode(document))
		else
			event.wakeup(session,false,bson.decode(document))
		end
		self.session_ctx[session] = nil
	else
		for _,doc in pairs(result) do
			table.insert(session_ctx.result,doc)
		end
		table.insert(session_ctx.hold,data)

		if cursor ~= nil then
			local more_data = driver.more(session, session_ctx.name, 50000, cursor)
			self.buffer:write(more_data)
		else
			local list = {}
			for _,doc in pairs(session_ctx.result) do
				local data = bson.decode(doc)
				data._id = nil
				table.insert(list,data)
			end
			if session_ctx.callback then
				if session_ctx.count == 1 then
					session_ctx.callback(list[1])
				else
					session_ctx.callback(list)
				end
			else
				event.wakeup(session,true,list)	
			end
			self.session_ctx[session] = nil
		end
	end
end

function mongo_channel:runCommand(db,cmd,cmd_v,...)
	local full_name = string.format("%s.$cmd",db)
	local bson_cmd
	if not cmd_v then
		bson_cmd = bson.encode_order(cmd,1)
	else
		bson_cmd = bson.encode_order(cmd,cmd_v,...)
	end

	local session = event.gen_session()
	self.session_ctx[session] = {count = 1,name = full_name,hold = {},result = {}}

	local data = driver.query(session, 0, full_name, 0,	1, bson_cmd)
	self.buffer:write(data)

	local ok,result = event.wait(session)
	if not ok then
		return false,result
	end

	if #result == 0 then
		return
	end
	return result[1]
end

local function find(self,db,name,query,selector,count,callback)
	local session = event.gen_session()
	name = string.format("%s.%s",db,name)
	self.session_ctx[session] = {count = count,name = name,hold = {},result = {},callback = callback}
	local data = driver.query(session, 0, name, 0, count, query and bson.encode(query) or empty_bson, selector and bson.encode(selector))
	self.buffer:write(data)

	if callback then
		return
	end
	local ok,result = event.wait(session)
	if not ok then
		return false,result
	end
	return result
end

--name:collection
--args:{query,selector}
--callback:
function mongo_channel:findOne(db,name,args,callback)
	args = args or {}
	local result,err = find(self,db,name,args.query,args.selector,1,callback)
	if result then
		return result[1]
	end
	return result,err
end

--name:collection
--args:{query,selector}
--callback:
function mongo_channel:findAll(db,name,args,callback)
	args = args or {}
	return find(self,db,name,args.query,args.selector,5000,callback)
end

function mongo_channel:update(db,name,selector,update,upsert,multi)
	name = string.format("%s.%s",db,name)
	local flags	= (upsert and 1	or 0) +	(multi and 2 or	0)
	local data = driver.update(name, flags, bson.encode(selector), bson.encode(update))
	self:write(data)
end

function mongo_channel:insert(db,name,doc)
	name = string.format("%s.%s",db,name)

	if doc._id == nil then
		doc._id	= bson.objectid()
	end

	local data = driver.insert(0, name, bson.encode(doc))
	self:write(data)
end

function mongo_channel:drop(db,name)
	return self:runCommand(db,"drop",name)
end

function mongo_channel:findAndModify(db,name,doc)
	assert(doc.query)
	assert(doc.update or doc.remove)

	local cmd = {"findAndModify", name}
	for k, v in pairs(doc) do
		table.insert(cmd, k)
		table.insert(cmd, v)
	end
	return self:runCommand(db,table.unpack(cmd))
end

function mongo_channel:ensureIndex(db,name,indexes)
	local function make_index(index,unique)
		local list = {}
		local name = "_index_"
		for field,value in pairs(index) do
			table.insert(list,field)
			table.insert(list,value)
			name = name..field.."_"
		end
		local doc = {}
		doc.name = name
		doc.unique = unique
		doc.key = bson.encode_order(table.unpack(list))
		return doc
	end

	local idx = {}
	for i,index in pairs(indexes) do
		idx[i] = make_index(index.index,index.unique or true)
	end

	return self:runCommand(db,"createIndexes",name,"indexes",idx)
end



return mongo_channel