local M = {}

function M.is_avaliable()
	return share ~= nil
end

function M.send(file)
	share.file(file)
end

return M