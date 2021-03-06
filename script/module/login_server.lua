local event = require "event"
local model = require "model"
local route = require "route"
local login_user = import "module.login_user"
local server_manager = import "module.server_manager"

_login_ctx = _login_ctx or {}
_account_queue = _account_queue or {}
_enter_agent_user = _enter_agent_user or {}

_name_set = _name_set or {}
_id_set = _id_set or {}

function start(self)
	self.db_timer = event.timer(30,function ()
		local all = model.fetch_login_user()
		for _,user in pairs(all) do
			user:save()
		end
	end)

	local db_channel = model.get_db_channel()
	local result = db_channel:findAll("login_user","role_list")
	for _,info in pairs(result) do
		for _,detail in pairs(info.list) do
			_name_set[detail.name] = info.account
			_id_set[detail.uid] = info.account
		end
	end

	server_manager:listen("AGENT_DOWN",self,"agent_down")
	import "handler.login_handler"
	import "handler.cmd_handler"
end

function flush(self)
	local all = model.fetch_login_user()
	for _,user in pairs(all) do
		user:save()
	end
end

function set_enter_user(self,enter_user)
	_enter_agent_user = enter_user
end

function agent_down(self,listener,agent_id)
	for account,info in pairs(_enter_agent_user) do
		if info.agent_server == agent_id then
			_enter_agent_user[account] = nil
		end
	end
end

function enter(self,cid,addr)
	local info = {cid = cid,addr = addr}
	_login_ctx[cid] = info
	-- event.error(string.format("cid:%d addr:%s enter",cid,addr))
end

function leave(self,cid)
	-- event.error(string.format("cid:%d leave",cid))
	local info = _login_ctx[cid]
	if not info then
		return
	end
	
	if info.account then
		local user = model.fetch_login_user_with_account(info.account)
		if user and user.cid == cid then
			user:leave()
		end
	end
	_login_ctx[cid] = nil
end

function dispatch_client(self,cid,message_id,data,size)
	route.dispatch_client(message_id,data,size,cid)
end

function user_auth(self,cid,account)
	local info = _login_ctx[cid]
	assert(info ~= nil,cid)
	assert(info.account == nil,info.account)

	local enter_info = _enter_agent_user[account]
	if enter_info then
		server_manager:send_agent(enter_info.agent_server,"handler.agent_handler","user_kick",{uid = enter_info.uid})
		local queue = _account_queue[account]
		if not queue then
			queue = {}
			_account_queue[account] = queue
		end
		table.insert(queue,cid)
		return
	end

	info.account = account
	local user = model.fetch_login_user_with_account(info.account)
	if user then
		client_manager:close(user.cid)
		user:leave()
	end
	local user = login_user.cls_login_user:new(cid,account)
	user:auth()
end

function user_create_role(self,cid,career)
	local info = _login_ctx[cid]
	if info.account then
		user = model.fetch_login_user_with_account(info.account)
		user:create_role(career)
	end
end

function user_enter_agent(self,cid,uid)
	local info = _login_ctx[cid]
	if info.account then
		local user = model.fetch_login_user_with_account(info.account)
		local agent_server = user:enter_agent(uid)
		user:leave()
		local client_manager = model.get_client_manager()
		client_manager:close(user.cid,1)
		_enter_agent_user[user.account] = {uid = uid,agent_server = agent_server}
		server_manager:agent_count_add(agent_server)
	end
end

function user_leave_agent(self,account)
	local enter_info = _enter_agent_user[account]
	_enter_agent_user[account] = nil
	assert(enter_info ~= nil,account)
	server_manager:agent_count_add(enter_info.agent_server)
	
	local queue = _account_queue[account]
	if not queue then
		return
	end
	_account_queue[account] = nil

	local client_manager = model.get_client_manager()

	local count = #queue
	for i = 1,count-1 do
		local cid = queue[i]
		if _login_ctx[cid] then
			client_manager:close(cid)
			_login_ctx[cid] = nil
		end
	end

	local last_cid = queue[count]
	local info = _login_ctx[last_cid]
	if info then
		info.account = account
		local user = login_user.cls_login_user:new(last_cid,account)
		user:auth()
	end
end

function server_stop(self)
	local client_manager = model.get_client_manager()
	client_manager:stop()

	local all = model.fetch_login_user()
	for _,user in pairs(all) do
		user:leave()
	end

	local db_channel = model.get_db_channel()
	
	local updater = {}
	updater["$inc"] = {version = 1}
	updater["$set"] = {time = os.time()}
	db_channel:findAndModify("common","login_version",{query = {uid = env.dist_id},update = updater,upsert = true})

	local agent_set = server_manager:how_many_agent()
	for _,agent_id in pairs(agent_set) do
		server_manager:send_agent(agent_id,"handler.agent_handler","server_stop")
	end
end