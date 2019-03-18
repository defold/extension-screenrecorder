local M = {}

local dirtylarry = require "gooey.themes.dirtylarry.dirtylarry"
local recorder = require('example.modules.recorder')

-- in example_ui.lua hide all UI specific things.
-- All necessary for screenrecorder NE code in example.gui_script and recorder.lua

M.func_start_record = nil

local function hide_options(self)
	gui.set_enabled(self.opt_screen, false)
	gui.set_enabled(self.scr_ios, false)
	gui.set_enabled(self.scr_not_ios, false)
	gui.set_enabled(self.scr_common, false)
	gui.set_enabled(self.scr_dekstop, false)

	gui.set_enabled(self.btn_record, false)
end

function M.init(self)
	self.btn_options = gui.get_node("options/bg")
	self.btn_record = gui.get_node("record/bg")

	self.opt_screen = gui.get_node("options_scr")
	self.scr_ios = gui.get_node("ios")
	self.scr_not_ios = gui.get_node("not_ios")
	self.scr_common = gui.get_node("common")
	self.scr_dekstop = gui.get_node("dekstop")

	hide_options(self)
	self.is_options_on = false

	self.ios_list_scaling = {"SCALING_RESIZE_ASPECT", "SCALING_FIT", "SCALING_RESIZE", "SCALING_RESIZE_ASPECT_FILL"}
end


local function show_options(self)
	self.is_options_on = true
	gui.set_enabled(self.opt_screen, true)
	gui.set_enabled(self.btn_record, true)
	gui.set_enabled(self.btn_options, false)

	if recorder.platform.is_ios then
		--gui ios
		gui.set_enabled(self.scr_ios, true)
		dirtylarry.dynamic_list("scaling", self.ios_list_scaling).selected_item = 1
		dirtylarry.checkbox("check_preview").set_checked(recorder.params.enable_preview)
	else
		--gui not ios
		gui.set_enabled(self.scr_not_ios, true)
		self.cfg_xscale = {empty_text = recorder.params.x_scale, allowed_characters = "[0123456789.]"}
		dirtylarry.input("inp_xscale", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_xscale)
		self.cfg_yscale = {empty_text = recorder.params.y_scale, allowed_characters = "[0123456789.]"}
		dirtylarry.input("inp_yscale", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_yscale)
		self.cfg_fps = {empty_text = recorder.params.fps, allowed_characters = "[0123456789.]"}
		dirtylarry.input("inp_fps", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_fps)

	end
	--gui common
	gui.set_enabled(self.scr_common, true)

	self.cfg_iframe = {empty_text = recorder.params.iframe, allowed_characters = "[0123456789.]"}
	dirtylarry.input("inp_iframe", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_iframe)
	self.cfg_duration = {empty_text = recorder.params.duration, allowed_characters = "[0123456789.]"}
	dirtylarry.input("inp_duration", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_duration)
	self.cfg_width = {empty_text = recorder.params.width, allowed_characters = "[0123456789.]"}
	dirtylarry.input("inp_width", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_width)
	self.cfg_height = {empty_text = recorder.params.height, allowed_characters = "[0123456789.]"}
	dirtylarry.input("inp_height", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_height)

	if recorder.platform.is_desktop then
		-- gui dekstop
		gui.set_enabled(self.scr_dekstop, true)
		dirtylarry.checkbox("check_async").set_checked(recorder.params.async_encoding)
	end
	self.params = {}
end

function M.on_input(self, action_id, action)
	if self.is_options_on then
		if recorder.platform.is_ios then
			--gui ios
			dirtylarry.checkbox("check_preview", action_id, action, function(checkbox)
				self.params.enable_preview = checkbox.checked
			end)
			dirtylarry.dynamic_list("scaling", self.ios_list_scaling, action_id, action, function(list)
				self.params.scaling = self.ios_list_scaling[list.selected_item]
			end)
		else
			--gui not ios
			local ipt = dirtylarry.input("inp_xscale", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_xscale)
			if ipt.out_now then
				self.params.xscale = tonumber(ipt.text)
			end
			ipt = dirtylarry.input("inp_yscale", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_yscale)
			if ipt.out_now then
				self.params.yscale = tonumber(ipt.text)
			end
			ipt = dirtylarry.input("inp_fps", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_fps)
			if ipt.out_now then
				self.params.fps = tonumber(ipt.text)
			end
		end
		--gui common
		local ipt = dirtylarry.input("inp_iframe", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_iframe)
		if ipt.out_now then
			self.params.iframe = tonumber(ipt.text)
		end
		ipt = dirtylarry.input("inp_duration", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_duration)
		if ipt.out_now then
			self.params.duration = tonumber(ipt.text)
		end
		ipt = dirtylarry.input("inp_width", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_width)
		if ipt.out_now then
			self.params.width = tonumber(ipt.text)
		end
		ipt = dirtylarry.input("inp_height", gui.KEYBOARD_TYPE_DEFAULT, action_id, action, self.cfg_height)
		if ipt.out_now then
			self.params.height = tonumber(ipt.text)
		end
		if recorder.platform.is_desktop then
			-- gui dekstop

			dirtylarry.checkbox("check_async", action_id, action, function(checkbox)
				self.params.async_encoding = checkbox.checked
			end)
		end
		--record button
		dirtylarry.button("record", action_id, action, function() 
			hide_options(self)
			M.func_start_record(self) 
		end)
	else
		dirtylarry.button("options", action_id, action, function() show_options(self) end)
	end
end

return M