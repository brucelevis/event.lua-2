



AGENT_PHASE = {
	LOAD = 1,
	ENTER = 2,
	LEAVE = 3
}

SERVER_TYPE = {
	AGENT = 0,
	WORLD = 1,
	SCENE = 2
}

SCENE_TYPE = {
	CITY = 1,
	STAGE = 2	
}

PROTOCOL_FORWARD = {
	c2s_move = SERVER_TYPE.SCENE
}

LUA_TYPE = {
	LUA_TNIL = 0,
	LUA_TBOOLEAN = 1,
	LUA_TLIGHTUSERDATA = 2,
	LUA_TNUMBER	= 3,
	LUA_TSTRING	= 4,
	LUA_TTABLE	= 5,
	LUA_TFUNCTION = 6,
	LUA_TUSERDATA = 7,
	LUA_TTHREAD	= 8,
}

FIGHTER_STATE = {
	IDLE = 1,
	MOVE = 2,
	SKILL = 3,
}