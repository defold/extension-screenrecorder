local M = {}

local function next_pot(num)
	local result = 1
	while num > result do
		result = bit.lshift(result, 1)
	end
	return result
end

function M.get_render_target_size(size, max)
	local result = next_pot(size)
	result = result > max and max or result
	return max, max
end

M.width = 0
M.height = 0

--render target width and height
M.width_rt = 0
M.height_rt = 0

M.render_target = {}

return M