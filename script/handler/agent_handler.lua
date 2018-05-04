local util = require "util"
local protocol = require "protocol"
local event = require "event"
local model = require "model"

local agent_server = import "module.agent_server"

function __init__(self)
	protocol.handler["c2s_agent_auth"] = req_auth
end

function req_auth(cid,args)
	event.fork(function ()
		agent_server:user_auth(cid,args.token)
	end)
end

function user_register(_,args)
	agent_server:user_register(args.account,args.uid,args.token,args.time)
end

function user_kick(_,args)
	agent_server:user_kick(args.uid)
end

function connect_scene_server(_,args)
	return agent_server:connect_scene_server(args.scene_server,args.scene_addr)
end

function forward_client(_,args)
	local user = model.fetch_agent_user_with_uid(args.uid)
	if not user then
		return
	end
	user:forward_client(args.message_id,args.data)
end

function get_enter_user(_,_)
	return agent_server:get_all_enter_user()
end

function server_stop()
	agent_server:server_stop()
end

function sync_scene_info(_,args)
	local user = model.fetch_agent_user_with_uid(args.user_uid)
	if not user then
		return false
	end
	user:sync_scene_info(args.scene_id,args.scene_uid,args.scene_server)
end