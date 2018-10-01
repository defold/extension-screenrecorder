-- This is a helper module for the screenrecorder extension.
-- It provides a simple toggle recording function with different recording parameters.

local _M = {}

local camera = require('libs.camera')

local audio_filename
local is_screenrecorder_init = false
local reboot_i = 1

local system_name = sys.get_sys_info().system_name
local is_desktop = system_name == 'Darwin' or system_name == 'Linux' or system_name == 'Windows'

function _M.toggle()
	-- Uncomment to test rebooting.
	--[[if reboot_i == 4 then
		print('Reboot!')
		msg.post('@system:', 'reboot')
		return
	end
	reboot_i = reboot_i + 1]]

	local video_ext = is_desktop and 'webm' or 'mp4'
	local audio_ext = is_desktop and 'webm' or 'aac'

	-- Temporary audio track file, saved from resources.
	if not audio_filename then
		local audio_content = sys.load_resource('/resources/audio.' .. audio_ext)
		audio_filename = directories.path_for_file('audio.' .. audio_ext, directories.documents)
		local audio_file = io.open(audio_filename, 'wb')
		audio_file:write(audio_content)
		audio_file:close()
	end

	-- Temporary video track file.
	local video_filename = directories.path_for_file('video.' .. video_ext, directories.documents)
	-- Video file with muxed-in audio track.
	local output_filename = directories.path_for_file('gameplay.' .. video_ext, directories.documents)

	local params = {
		render_target = camera.render_target,
		x_scale = camera.width_p2 / camera.width, -- Compensate render target being larger than video frame size on Android.
		y_scale = camera.height_p2 / camera.height,
		filename = video_filename,
		width = 1280, height = 720,
		listener = function(event)
			if event.phase == 'init' then
				if not event.is_error then
					is_screenrecorder_init = true
					print('Screenrecorder initialized.')
				else
					print('Screenrecorder failed to initialize:')
					print(event.error_message)
				end
			elseif event.phase == 'recorded' then
				if not event.is_error then
					print('Screenrecorder saved the recording.')
					if screenrecorder.is_preview_available() then
						print('Screenrecorder showing preview.')
						screenrecorder.show_preview()
					else
						screenrecorder.mux_audio_video{
							audio_filename = audio_filename,
							video_filename = video_filename,
							filename = output_filename
						}
					end
				else
					print('Screenrecorder failed to save the recording:')
					print(event.error_message)
				end
			elseif event.phase == 'muxed' then
				if not event.is_error then
					print('Screenrecorder muxed audio and video tracks.')
					if is_desktop then
						-- On desktop just print out file location.
						print('Video saved: ' .. output_filename)
					else
						-- On mobiles use native share dialog.
						share.file(output_filename)
					end
				else
					print('Screenrecorder failed to mux audio and video tracks:')
					print(event.error_message)
				end
			elseif event.phase == 'preview' then
				if event.actions then
					print('Screenrecorder preview ended with actions:', table.concat(event.actions, ', '), '.')
				else
					print('Screenrecorder preview is closed.')
				end
			end
		end
	}
	-- Tell render script to capture less frames to match fps.
	screenrecorder.is_half_fps = not params.fps or params.fps == 30
	-- Set to Full HD if screen is big enough.
	-- TODO: determine if device is actually powerful enough.
	--if camera.width >= 1920 and camera.height >= 1080 then
	--	params.width, params.height, params.bitrate = 1920, 1080, 3 * 1024 * 1024
	--end

	if not screenrecorder.is_recording() then
		if not is_screenrecorder_init then
			screenrecorder.init(params)
		else
			print('Staring recording.')
			screenrecorder.start()
		end
	else
		screenrecorder.stop()
		-- Reinit is required each time.
		is_screenrecorder_init = false
	end
end

return _M