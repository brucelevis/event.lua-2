local event = require "event"
local protocol = require "protocol"
local cjson = require "cjson"
local util = require "util"
local model = require "model"

local login_user = import "module.login_user"

_login_ctx = _login_ctx or {}
_account_queue = _account_queue or {}

function __init__()
	protocol.handler["c2s_login_auth"] = req_enter
	protocol.handler["c2s_login_enter"] = req_enter_game
end

function enter(cid,addr)
	local info = {cid = cid,addr = addr}
	_login_ctx[cid] = info
	event.error(string.format("cid:%d addr:%s enter",cid,addr))
end

function leave(cid)
	event.error(string.format("cid:%d leave",cid))
	local info = _login_ctx[cid]
	if not info then
		return
	end
	
	if info.account then
		local user = model.fetch_login_user_with_account(account)
		if user then
			user:leave()
		end
	end
	_login_ctx[cid] = nil
end

function req_enter(cid,args)
	local account = args.account
	
	local queue = _account_queue[account]
	if not queue then
		queue = {}
		_account_queue[account] = queue
	end
	table.insert(queue,cid)

	local user = model.fetch_login_user_with_account(account)
	if user then
		user:leave(function ()
			local last_cid = table.remove(queue)
			local client_manager = model.get_client_manager()
			for _,cid in pairs(queue) do
				client_manager:close(cid)
				leave(cid)
			end
			_account_queue[account] = nil
			local info = _login_ctx[last_cid]
			if info then
				info.account = account
				user:new(last_cid,account)
				user:auth()
			end
		end)
	else
		assert(#queue == 1)
		_account_queue[account] = nil

		local info = _login_ctx[cid]
		if info then
			info.account = account
			local user = login_user.cls_login_user:new(cid,account)
			user:auth()
		end
	end
end

function req_enter_game(client_id,args)
	local account = args.account
	local user_id = args.user_id
	local json = cjson.encode({account = account,user_id = user_id})
	local md5 = util.md5(json)
	local now = util.time()
	local token = util.rc4(json,now)

end