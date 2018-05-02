local object = import "module.object"

cls_collection = object.cls_base:inherit("database_collection")

function __init__(self)
	self.cls_collection:save_field("__name")
end

local function clone_table(data)
	for k,v in pairs(data) do
		if type(v) == "table" then
			if v.save_data then
				data[k] = v:save_data()
			else
				data[k] = clone_table(v)
			end
		end
	end
	return result
end

function cls_collection:save_data()
	local result = {}
	for field in pairs(self.__save_fields) do
		local data = self[field]
		if data then
			if type(data) == "table" then
				if data.save_data then
					result[field] = data:save_data()
				else
					result[field] = clone_table(data)
				end
			else
				result[field] = data
			end
		end
	end
	return result
end
