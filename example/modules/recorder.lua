local M = {}

local system_name = sys.get_sys_info().system_name
local is_debug = sys.get_engine_info().is_debug

M.platform = {}
M.platform.name = system_name
M.platform.is_desktop = system_name == 'Darwin' or system_name == 'Linux' or system_name == 'Windows' or system_name == 'HTML5'
M.platform.is_html5 = system_name == 'HTML5'
M.platform.is_ios = system_name == 'iPhone OS'
M.platform.is_android = system_name == 'Android'
M.IS_NEED_RENDER_TAGET = system_name ~= 'iPhone OS'

local is_first_init = true
local is_inited = false

local video_ext = M.platform.is_desktop and 'webm' or 'mp4'
local audio_ext = M.platform.is_desktop and 'webm' or 'aac'

M.VIDEO_FILENAME = "video"
M.RESULT_FILENAME = "gameplay"

-- Temporary video track file.
local video_filename = nil
-- Video file with muxed-in audio track.
local output_filename = nil
-- Temporary audio track file, saved from resources.
local audio_filename = nil

local external_listener = nil
local _self =nil

local record_frame_every_sec = 0
local sec_from_last_frame = 0

M.params = {}

local function log(...)
	if is_debug then
		print("SCREEN_RECORDER: ",...)
	end
end

local function round(num, idp)
	local mult = 10^(idp or 0)
	return math.floor(num * mult + 0.5) / mult
end

local function listener(event)
	if event.phase == 'init' then
		if not event.is_error then
			is_inited = true
			log('Initialized.')
		else
			log('Failed to initialize: ', event.error_message)
		end
	elseif event.phase == 'recorded' then
		if not event.is_error then
			log('Saved the recording. ', video_filename)
		else
			log('Failed to save the recording: ', event.error_message)
		end
	elseif event.phase == 'muxed' then
		if not event.is_error then
			log('Muxed audio and video tracks.')
		else
			log('Failed to mux audio and video tracks: ', event.error_message)
		end
	elseif event.phase == 'preview' then
		if event.actions then
			log('Preview ended with actions: ', table.concat(event.actions, ', '), '.')
		else
			log('Preview is closed.')
		end
	end
	
	if external_listener then
		external_listener(_self, event)
	end
end

local function files_init()
	-- paths creation

	-- Temporary video track file.
	video_filename = directories.path_for_file(M.VIDEO_FILENAME..'.'.. video_ext, directories.documents)

	-- Video file with muxed-in audio track.
	output_filename = directories.path_for_file(M.RESULT_FILENAME..'.'.. video_ext, directories.documents)

	-- Temporary audio track file, saved from resources.
	local audio_content = sys.load_resource('/resources/audio.' .. audio_ext)
	audio_filename = directories.path_for_file('audio.' .. audio_ext, directories.documents)
	local audio_file = io.open(audio_filename, 'wb')
	audio_file:write(audio_content)
	audio_file:close()
end

function M.register_listener(self, listener)
	external_listener = listener
	_self = self
end

function M.fill_defaulf_params(params)
	params = params or M.params

	if is_first_init then
		files_init()
	end

	M.params.listener = listener
	video_filename = params.filename or video_filename
	M.params.filename = video_filename

	M.params.iframe = params.iframe or 1 
	M.params.duration = params.duration or nil
	M.params.bitrate = params.bitrate or 2 * 1024 * 1024

	if M.platform.is_html5 then
		M.params.width = params.width or 640
		M.params.height = params.height or 360
	else
		M.params.width = params.width or 1280
		M.params.height = params.height or 720
	end

	if M.platform.is_ios then
		M.params.enable_preview = params.enable_preview or false
		M.params.scaling = params.scaling and screenrecorder[params.scaling] or screenrecorder.SCALING_RESIZE_ASPECT
		--[[screenrecorder.SCALING_FIT, 
		screenrecorder.SCALING_RESIZE_ASPECT,
		screenrecorder.SCALING_RESIZE, 
		screenrecorder.SCALING_RESIZE_ASPECT_FILL]]--
	else
		M.params.render_target = params.render_target or nil
		M.params.x_scale = params.x_scale or 1
		M.params.y_scale = params.y_scale or 1
		M.params.fps = params.fps or 30
		record_frame_every_ms = round(1/M.params.fps, 3)
	end

	if M.platform.is_desktop then
		M.params.async_encoding = params.async_encoding or false
	end
	return params
end

function M.init(params)
	if not screenrecorder then log("screenrecorder module do not exist.") return end

	params = M.fill_defaulf_params(params)
	
	log("Initializing the screenrecorder.")
	screenrecorder.init(M.params)
end

function M.start_record()
	if not screenrecorder then log("screenrecorder module do not exist.") return end

	sec_from_last_frame = 1
	
	if screenrecorder.is_recording() then
		log("Video recording already in progress.")
	elseif is_inited then
		screenrecorder.start()
	else
		log("Recorder hasn't inited.")
	end
end

function M.stop_record()
	if not screenrecorder then log("screenrecorder module do not exist.") return end

	if screenrecorder.is_recording() then
		is_inited = false
		screenrecorder.stop()
	else
		log("You can't stop video recording while recording wasn't started.")
	end
end

function M.toggle()
	if not screenrecorder then log("screenrecorder module do not exist.") return end
	
	if screenrecorder.is_recording() then
		M.stop_record()
	else
		M.start_record()
	end
end

function M.is_recording()
	return screenrecorder and screenrecorder.is_recording() or false
end

function M.is_preview_avaliable()
	if not screenrecorder then log("screenrecorder module do not exist.") return end
	
	return screenrecorder.is_preview_available()
end

function M.show_preview()
	if not screenrecorder then log("screenrecorder module do not exist.") return end
	
	if screenrecorder.is_preview_available() then
		log('Showing preview.')
		screenrecorder.show_preview()
	else
		log('Preview does not available')
	end
end

function M.mux_audio_video()
	if not screenrecorder then log("screenrecorder module do not exist.") return end
	
	screenrecorder.mux_audio_video({
		audio_filename = audio_filename,
		video_filename = video_filename,
		filename = output_filename
	})
end

function M.capture_frame(dt)
	if M.IS_NEED_RENDER_TAGET then
		sec_from_last_frame = sec_from_last_frame + dt
		if sec_from_last_frame >= record_frame_every_ms then
			screenrecorder.capture_frame()
			sec_from_last_frame = 0
		end
	end
end

function M.get_video_file_path()
	return video_filename
end

function M.get_muxed_file_path()
	return output_filename
end

return M