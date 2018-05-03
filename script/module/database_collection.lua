local object = import "module.object"

cls_collection = object.cls_base:inherit("database_collection")

function __init__(self)
	self.cls_collection:save_field("__name")
end

function cls_collection:dirty_field(field)
	self.__dirty[field] = true
	self.__dirty["__name"] = true
end

local function clone_table(data)
	local result = {}
	for k,v in pairs(data) do
		if type(v) == "table" then
			if v.save_data then
				result[k] = v:save_data(false)
			else
				result[k] = clone_table(v)
			end
		else
			result[k] = v
		end
	end
	return result
end

function cls_collection:save_data(root)
	if root then
		local set = {}
		local unset = {}
		for field in pairs(self.__dirty) do
			if self.__save_fields[field] then
				local data = self[field]
				if data then
					if type(data) == "table" then
						if data.save_data then
							set[field] = data:save_data(false)
						else
							set[field] = clone_table(data)
						end
					else
						set[field] = data
					end
				else
					unset[field] = true
				end
			end
		end

		return set,unset
	else
		local result = {}
		for field in pairs(self.__save_fields) do
			local data = self[field]
			if data then
				if type(data) == "table" then
					if data.save_data then
						result[field] = data:save_data(false)
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
end
