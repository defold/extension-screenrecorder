local M = {}

local dirtylarry = require "gooey.themes.dirtylarry.dirtylarry"
local recorder = require('example.modules.recorder')
local share = require('example.modules.sharing')

local STATE_OPTION_BTN = 0
local STATE_OPTIONS_MENU = 1
local STATE_START_RECORD_BTN = 2
local STATE_RECORD_IN_PROGRESS = 3
local STATE_POST_RECORD = 4

-- in example_ui.lua hide all UI specific things.
-- All necessary for screenrecorder NE code in example.gui_script and recorder.lua

M.func_start_record = nil
M.func_init_recorder = nil
M.func_stop_record = nil
M.func_mux = nil
M.func_share = nil

function M.show_post_record_options(self)
	gui.set_enabled(self.btn_mux, true)
	if share.is_avaliable() then
		gui.set_enabled(self.btn_share, true)
	end
end

function M.record_end(self)
	self.current_state = STATE_POST_RECORD
	gui.set_enabled(self.btn_stop, false)
end

function M.show_preview(self)
	gui.set_enabled(self.btn_preview, true)
end

function M.recorder_inited(self)
	gui.set_enabled(self.btn_record, true)
	self.current_state = STATE_START_RECORD_BTN
end

local function start_record(self, state)
	gui.set_enabled(self.btn_stop, true)
	gui.set_enabled(self.btn_record, false)

	self.current_state = state
end

local function hide_options(self, state)
	gui.set_enabled(self.opt_screen, false)
	gui.set_enabled(self.scr_ios, false)
	gui.set_enabled(self.scr_not_ios, false)
	gui.set_enabled(self.scr_common, false)
	gui.set_enabled(self.scr_dekstop, false)
	gui.set_enabled(self.btn_record, false)
	
	self.current_state = state
end

function M.init(self)
	self.btn_options = gui.get_node("options/bg")
	self.btn_record = gui.get_node("record/bg")
	self.btn_stop = gui.get_node("stop/bg")
	self.btn_mux = gui.get_node("mux/bg")
	self.btn_share = gui.get_node("share/bg")
	self.btn_preview = gui.get_node("preview/bg")
	self.btn_init = gui.get_node("init/bg")

	gui.set_enabled(self.btn_stop, false)
	gui.set_enabled(self.btn_mux, false)
	gui.set_enabled(self.btn_share, false)
	gui.set_enabled(self.btn_preview, false)

	self.opt_screen = gui.get_node("options_scr")
	self.scr_ios = gui.get_node("ios")
	self.scr_not_ios = gui.get_node("not_ios")
	self.scr_common = gui.get_node("common")
	self.scr_dekstop = gui.get_node("dekstop")

	hide_options(self, STATE_OPTION_BTN)

	self.ios_list_scaling = {"SCALING_RESIZE_ASPECT", "SCALING_FIT", "SCALING_RESIZE", "SCALING_RESIZE_ASPECT_FILL"}
end


local function show_options(self, state)
	self.current_state = state
	gui.set_enabled(self.opt_screen, true)
	gui.set_enabled(self.btn_init, true)
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
	if self.current_state == STATE_OPTIONS_MENU then
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
		--init button
		dirtylarry.button("init", action_id, action, function() 
			hide_options(self)
			M.func_init_recorder(self)
		end)
	elseif self.current_state == STATE_OPTION_BTN then
		dirtylarry.button("options", action_id, action, function() 
			show_options(self, STATE_OPTIONS_MENU) 
		end)
	elseif self.current_state == STATE_RECORD_IN_PROGRESS then
		dirtylarry.button("stop", action_id, action, function() 
			M.func_stop_record(self)
			M.record_end(self, STATE_POST_RECORD)
		end)
	elseif self.current_state == STATE_START_RECORD_BTN then
		dirtylarry.button("record", action_id, action, function() 
			M.func_start_record(self)
			start_record(self, STATE_RECORD_IN_PROGRESS)
		end)
	elseif self.current_state == STATE_POST_RECORD then
		dirtylarry.button("mux", action_id, action, function() 
			M.func_mux(self)
		end)
		dirtylarry.button("share", action_id, action, function() 
			M.func_share(self)
		end)
	end
end

return M