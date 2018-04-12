local event = require "event"
local util = require "util"
local model = require "model"
local protocol = require "protocol"
local mongo = require "mongo"
local route = require "route"
local channel = require "channel"
local startup = import "server.startup"



model.register_value("db_channel")


local agent_channel = channel:inherit()
function agent_channel:disconnect()
	if self.id ~= nil then
		server_handler:agent_server_down(self.id)
	end
end

event.fork(function ()
	startup.run()
	protocol.parse("login")
	protocol.load()

	local mongodb,reason = event.connect(env.mongodb,4,true,mongodb_channel)
	if not mongodb then
		print(string.format("connect db:%s faield:%s",env.mongodb,reason))
		os.exit(1)
	end
	mongodb:init("sunset")
	model.set_db_channel(mongodb)

	startup.connect_server("world")

	startup.connect_server("master")

	local listener,reason = event.listen(env.scene,4,channel_accept,agent_channel)
	if not listener then
		event.breakout(reason)
		return
	end

	local ip,port = listener:addr()
	local addr_info = {}
	if port == 0 then
		addr_info.file = ip
	else
		addr_info.ip = ip
		addr_info.port = port
	end

	local master_channel = model.get_master_channel()
	master_channel:send("handler.server_handler","register_scene_server",addr_info)
end)
