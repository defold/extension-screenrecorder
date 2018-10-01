# Screenrecorder Extension

This extension captures video gameplay.

### Supported platforms
* Android 5.0 or later.
* iOS 11.0 or later.
* macOS x86_64.
* Linux x86_64.
* Windows x86_64.

macOS, Linux and Windows require OpenGL 3.0 or better video card.

### Formats
* Android and iOS
	* Encoder: H.264 codec in the MP4 container.
	* Muxer: accepts MP4 video and AAC audio.
* macOS, Linux, Windows
	* Encoder: VP8 codec in the WEBM container.
	* Muxer: accepts WEBM video and WEBM audio (Vorbis).

On of the key features of this extension is the ability to save just last N seconds of the gameplay. The extension maintains a circular buffer for encoded data and saves it when requested. On Android, macOS, Linux and Windows the circular encoder stores it's data in memory, on iOS the circular encoder saves to temporary video files and then joins them together to produce desired duration. The circular encoder is activated with the `duration` parameter.

Audio capture is not implemented, however the extension provides a function to mux a prepared audio file with the captured video.

On iOS the extension uses Apple's ReplayKit API and captures the entire game screen. Additionally it can show a native preview window, where the user can edit the captured video and share it. To enable the preview window, specify `enable_preview = true` parameter.

On other platforms the capture pipeline requires using a render target. First you render the content you wish to capture into a render target, then the extension passes the texture to the encoder. This way you can first render to a render target and then use it's texture for encoding and for displaying on the screen with a quad model like in the postprocessing tutorial.

It's important to take into account scaling of the captured texture and the render target. Very often screen size, render target size and video frame size don't match. To control aspect ratio and black borders around video frame you can specify `x_scale` and `y_scale` parameters. Except on iOS, on that platform you provide a predefined scaling algorithm using `scale` parameter.

On desktop platforms if windows size is smaller than video frame size, it's important to upscale when rendering to a texture to fill the render target, otherwise a blank space will be visible on the video.

On Android the extension uses the MediaCodec API and the encoder accepts an EGL surface as an input. The extension internally renders to this surface without accessing pixel data directly.

On desktop platforms the extension internally renders to it's own OpenGL Frame Buffer Object and converts RGB image to YUV image, then passing this data to the VPX encoder.

## Project setup

In `game.project` file add dependency `https://github.com/defold/extension-screenrecorder/archive/master.zip`.

Copy `screenrecorder.appmanifest` into your project and choose it in your `game.project` under `native_extension -> App Manifest`. This file disables Defold's internal `record` library.

In render script prepare a render target, the size of the render target should be not smaller than desired video frame size. On mobiles the size also has to be one of the power of two numbers (e.g. 512, 1024, 2048).

Put `screenrecorder.capture_frame()` somewhere in your render script's `update()` method.

Before starting the capture the extension has to be initialized with `screenrecorder.init(params)` function. And it has to be initialized each time after a recording is finished with `screenrecorder.stop()`.

When the setup is done, just start the capture with `screenrecorder.start()`.

To work with filenames on different platforms the [directories extension](https://www.defold.com/community/projects/121601/) is recommended.

## API reference
___
### `screenrecorder.init(params)`

Prepares the capture, sets up the encoder. Once setup is complete, an `'init'` event will be dispatched to the `listener` function. All other events will also be directed to this function.

`params` - table with parameters.

* iOS parameters:
	* `enable_preview` - `boolean`, if `true`, enables showing a preview and edit window after a completed recording, default is `false`. Incompatible with set `duration` and other video params. iOS takes full control over the video.
	* `scaling` - `constant`, scaling mode determines how frame is placed into specified width/height region. Default is `screenrecorder.SCALING_RESIZE_ASPECT`. Possible values:
		* `screenrecorder.SCALING_FIT` - crop to remove edge processing region; preserve aspect ratio of cropped source by reducing specified width or height if necessary.  Will not scale a small source up to larger dimensions.
		* `screenrecorder.SCALING_RESIZE` - crop to remove edge processing region; scale remainder to destination area.  Does not preserve aspect ratio.
		* `screenrecorder.SCALING_RESIZE_ASPECT` - preserve aspect ratio of the source, and fill remaining areas with black to fit destination dimensions.
		* `screenrecorder.SCALING_RESIZE_ASPECT_FILL` - preserve aspect ratio of the source, and crop picture to fit destination dimensions.
* Desktop parameters:
	* `async_encoding` - `boolean`, experimental - if `true` use a separate encoding thread. Might improve performance, might make it worse. Default is `false`.
* Common parameters:
	* `render_target` - `render_target`, specifies a render target to work with, the extension uses it's internal texture to pass data into encoder. What is rendered into this target gets into the video file. Required on all platforms, except iOS.
	* `x_scale` - `number`, horizontal scale of the render target's texture. Use it with `y_scale` to maintain desired aspect ratio and frame fill. Default is `1.0`.
	* `y_scale` - `number`, vertical scale of the render target's texture. Default is `1.0`.
	* `filename` - `string`, path to the output video file. Required.
	* `width` - `number`, width of the video frame. Default is `1280`.
	* `height` - `number`, height of the video frame. Default is `720`.
	* `iframe` - `number`, video keyframe interval in seconds. Default is `1.0`.
	* `duration` - `number`, if set, use circular encoder to record last N seconds. Default is `nil`.
	* `fps` - `number`, video framerate, Default is `30`. On iOS fps is chosen by the OS and this setting has no effect.
	* `bitrate` - `number`, video bitrate in bits per second. Default is `2 * 1024 * 1024`.
	* `listener` - `function`, this function receives various events from the extension. See Events section.

Parameters that are not available on iOS: `render_target`, `x_scale`, `y_scale`, `fps`.
___
### `screenrecorder.start()`

Starts the capture process. The extension has to be initialized before calling this function. Once the recording is started, you can supply frames to encode with the `screenrecorder.capture_frame()` function. If the encoder failed to start, an error event is dispatched.
___
### `screenrecorder.stop()`

Stops the capture process, finishes writing encoded data into the video file and closes it. Releases internal resorces and deinitializes the extension. `screenrecorder.init()` has to be called again after stopping the recording. If an error occured during finalization, an error event is dispatched.
___
### `screenrecorder.capture_frame()`

Captures the current frame and submits it to the encoder. Has no effect on iOS due to differnt capture approach. You must match the capture framerate and calls of this function, e.g. if your game is 60 fps and the recording is at 60 fps, then you call this function every frame. But if your recording is at 30 fps, you have to skip every other frame.
___
### `screenrecorder.is_recording()`

Returns `true` if the extension is currently recording. `false` otherwise.
___
### `screenrecorder.mux_audio_video(params)`

Muxes one audio file and one video file into one combined file. The duration of the output file is matched to the video file. On desktop platforms this function accepts WEBM files, on mobiles - MP4 and AAC files. Once muxing is done, a `'muxed'` event is dispatched.

`params` - table with parameters.
* `audio_filename` - string, path to the audio file.
* `video_filename` - string, path to the video file.
* `filename` - string, path to the output file.
___
### `screenrecorder.is_preview_available()`

Returns `true` if the extension has captured video with enabled preview on iOS and this preview is ready to show up. `false` otherwise.
___
### `screenrecorder.show_preview()`

Shows a special window on iOS that lets the user to edit and share the recorded video.
___
## Events

The `listener` function receives an `event` table with information about succesful and failed operations.

`event` table.
* `name` - `string`, `'screenrecorder'`. The name of the event.
* `phase` - `string`, the phase of the extension, corresponds to various calls. Phases:
	* `'init'` - initialization phase.
	* `'recorded'` - saving the recording phase.
	* `'muxed'` - muxing audio and video phase.
* `is_error` - `boolean`, indicates if an error has occured.
* `error_message` - `string`, if `is_error` is `true` holds details about the error.

## Used technologies
* Android
	* [MediaCodec](https://developer.android.com/reference/android/media/MediaCodec)
	* [MediaRecorder](https://developer.android.com/reference/android/media/MediaRecorder)
	* [MediaMuxer](https://developer.android.com/reference/android/media/MediaMuxer)
	* [EGL Surface](https://source.android.com/devices/graphics/arch-egl-opengl)
	* [OpenGL ES](https://developer.android.com/guide/topics/graphics/opengl)
	* [H.264](https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC)
	* [MP4](https://en.wikipedia.org/wiki/MPEG-4_Part_14)
* iOS
	* [ReplayKit](https://developer.apple.com/documentation/replaykit?language=objc)
	* [RPScreenRecorder](https://developer.apple.com/documentation/replaykit/rpscreenrecorder?language=objc)
	* [RPPreviewViewController](https://developer.apple.com/documentation/replaykit/rppreviewviewcontroller?language=objc)
	* [CoreMedia](https://developer.apple.com/documentation/coremedia?language=objc)
	* [H.264](https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC)
	* [MP4](https://en.wikipedia.org/wiki/MPEG-4_Part_14)
* macOS, Linux, Windows
	* [libvpx](https://en.wikipedia.org/wiki/Libvpx)
	* [VP8 codec](https://en.wikipedia.org/wiki/VP8)
	* [libwebm](https://github.com/webmproject/libwebm)
	* [WebM](https://en.wikipedia.org/wiki/WebM)
	* [OpenGL](https://en.wikipedia.org/wiki/OpenGL)
	