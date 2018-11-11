#if defined(DM_PLATFORM_IOS)

// On iOS recording is done using ReplayKit and it's RPScreenRecording class.
// In case of circular encoding, the recorder writes to three vides files sequentially and cycles them
// to maintain desired duration. Saving to memory turned out to be much slower.

#include <dmsdk/sdk.h>
#include "screenrecorder_private.h"

#import <Foundation/Foundation.h>
#import <ReplayKit/ReplayKit.h>
#import <CoreMedia/CoreMedia.h>
#import "ios/utils.h"

// Requested recording parameters.
@interface CaptureParams : NSObject
@property NSString *filename;
@property NSNumber *duration;
@property int width;
@property int height;
@property int bitrate;
@property int iframe;
@property bool enable_preview;
@property NSString *scaling_mode;
@end

@implementation CaptureParams
@end

// Using proper Objective-C object for main extension entity.
@interface ScreenRecorderInterface : NSObject <RPPreviewViewControllerDelegate>
@end

@implementation ScreenRecorderInterface {
    bool is_initialized;
    bool is_recording_bool;
    int lua_listener;
    NSMutableArray *asset_writers;
    NSMutableArray *asset_writer_videos;
    RPPreviewViewController *preview_view_controller;
    dispatch_queue_t capture_queue;
    int active_index;
    CMTime asset_writer_start_time;
    CMTime duration_time;
    CaptureParams *capture_params;
    NSDictionary *video_settings;
    bool circular_encoder_has_looped;
}

typedef NS_ENUM(NSUInteger, ScalingMode) {
    ScalingModeFit,
    ScalingModeResize,
    ScalingModeResizeAspect,
    ScalingModeResizeAspectFill
};

static NSString *const SCREENRECORDER = @"screenrecorder";
static NSString *const EVENT_PHASE = @"phase";
static NSString *const EVENT_INIT = @"init";
static NSString *const EVENT_MUXED = @"muxed";
static NSString *const EVENT_RECORDED = @"recorded";
static NSString *const EVENT_PREVIEW = @"preview";
static NSString *const EVENT_IS_ERROR = @"is_error";
static NSString *const EVENT_ERROR_MESSAGE = @"error_message";

// Each ScreenRecorder_* function invokes corresponding Objective-C object method.
static ScreenRecorderInterface *sr;
int ScreenRecorder_init(lua_State *L) {return [sr init_:L];}
int ScreenRecorder_start(lua_State *L) {return [sr start:L];}
int ScreenRecorder_stop(lua_State *L) {return [sr stop:L];}
int ScreenRecorder_mux_audio_video(lua_State *L) {return [sr mux_audio_video:L];}
int ScreenRecorder_capture_frame(lua_State *L) {return [sr capture_frame:L];}
int ScreenRecorder_is_recording(lua_State *L) {return [sr is_recording:L];}
int ScreenRecorder_is_preview_available(lua_State *L) {return [sr is_preview_available:L];}
int ScreenRecorder_show_preview(lua_State *L) {return [sr show_preview:L];}

-(id)init:(lua_State*)L {
	self = [super init];
	lua_getglobal(L, EXTENSION_NAME_STRING);

    // Additional API for iOS - video scaling modes.
    lua_pushnumber(L, ScalingModeFit);
    lua_setfield(L, -2, "SCALING_FIT");

    lua_pushnumber(L, ScalingModeResize);
    lua_setfield(L, -2, "SCALING_RESIZE");

    lua_pushnumber(L, ScalingModeResizeAspect);
    lua_setfield(L, -2, "SCALING_RESIZE_ASPECT");

    lua_pushnumber(L, ScalingModeResizeAspectFill);
    lua_setfield(L, -2, "SCALING_RESIZE_ASPECT_FILL");

	lua_pop(L, 1);

    is_initialized = false;
    is_recording_bool = false;
    lua_listener = LUA_REFNIL;
    capture_params = [CaptureParams alloc];
    asset_writers = nil;
    asset_writer_videos = nil;
    preview_view_controller = nil;
    capture_queue = dispatch_queue_create("capture_video_queue", NULL);
    active_index = 0;

	return self;
}

-(bool)check_is_initialized {
	if (is_initialized) {
		return true;
	} else {
		dmLogInfo("The extension is not initialized.");
		return false;
	}
}

# pragma mark - Lua functions -

// screenrecorder.capture_frame()
-(int)capture_frame:(lua_State*)L {
    [Utils checkArgCount:L count:0];
    return 0;
}

// screenrecorder.is_recording()
-(int)is_recording:(lua_State*)L {
    [Utils checkArgCount:L count:0];
    lua_pushboolean(L, is_recording_bool);
    return 1;
}

// screenrecorder.init(params)
-(int)init_:(lua_State*)L {
	[Utils checkArgCount:L count:1];
	if (is_initialized) {
		dmLogInfo("The extension is already initialized.");
		return 0;
	}

    // Scheme defines Lua syntax for the params table argument.
    // Provides basic type checking.
    Scheme *scheme = [[Scheme alloc] init];
    [scheme string:@"filename"];
    [scheme number:@"width"];
    [scheme number:@"height"];
    [scheme number:@"scaling"];
    [scheme number:@"bitrate"];
    [scheme number:@"iframe"];
    [scheme number:@"duration"];
    [scheme boolean:@"enable_preview"];
    [scheme function:@"listener"];

    // Perform the check and convert Lua values to Objective-C values.
    Table *params = [[Table alloc] init:L index:1];
    [params parse:scheme];

    capture_params.filename = [params getStringNotNull:@"filename"];
    capture_params.duration = [params getDouble:@"duration"];
    capture_params.width = [params getInteger:@"width" default:1280];
    capture_params.height = [params getInteger:@"height" default:720];
    int scaling = [params getInteger:@"scaling" default:ScalingModeResizeAspect];
    capture_params.bitrate = [params getInteger:@"bitrate" default:2 * 1024 * 1024];
    capture_params.iframe = [params getInteger:@"iframe" default:1];
    lua_listener = [params getFunction:@"listener" default:LUA_REFNIL];
    capture_params.enable_preview = [params getBoolean:@"enable_preview" default:false];

    NSError *error = nil;
    if (!capture_params.enable_preview) {
        // More parameters if the simple "preview" recorder is not used.
        switch (scaling) {
            case ScalingModeFit:
                capture_params.scaling_mode = AVVideoScalingModeFit;
                break;
            case ScalingModeResize:
                capture_params.scaling_mode = AVVideoScalingModeResize;
                break;
            case ScalingModeResizeAspectFill:
                capture_params.scaling_mode = AVVideoScalingModeResizeAspectFill;
                break;
            default:
                capture_params.scaling_mode = AVVideoScalingModeResizeAspect;
        }

        NSDictionary *compression_properties = @{
            AVVideoProfileLevelKey : AVVideoProfileLevelH264HighAutoLevel,
            AVVideoH264EntropyModeKey : AVVideoH264EntropyModeCABAC,
            AVVideoAverageBitRateKey : @(capture_params.bitrate),
            AVVideoMaxKeyFrameIntervalDurationKey : @(capture_params.iframe),
            AVVideoAllowFrameReorderingKey : @NO
        };

        video_settings = @{
            AVVideoCompressionPropertiesKey : compression_properties,
            AVVideoCodecKey : AVVideoCodecTypeH264,
            AVVideoWidthKey : @(capture_params.width),
            AVVideoHeightKey : @(capture_params.height),
            AVVideoScalingModeKey : capture_params.scaling_mode
        };

        asset_writers = [[NSMutableArray alloc] init];
        asset_writer_videos = [[NSMutableArray alloc] init];
        int asset_writers_count = 1;
        if (capture_params.duration) {
            // Create three video files for cycled recording.
            asset_writers_count = 3;
            duration_time = CMTimeMakeWithSeconds([capture_params.duration floatValue], 1);
        }
        for (int i = 0; i < asset_writers_count; ++i) {
            NSString *full_filename = capture_params.filename;
            if (capture_params.duration) {
                full_filename = [NSString stringWithFormat:@"%@_%d.mp4", capture_params.filename, i];
            }
            [[NSFileManager defaultManager] removeItemAtPath:full_filename error:nil];
            AVAssetWriter *asset_writer = [AVAssetWriter assetWriterWithURL:[NSURL fileURLWithPath:full_filename] fileType:AVFileTypeMPEG4 error:&error];
            AVAssetWriterInput *asset_writer_video = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo outputSettings:video_settings];
            [asset_writer addInput:asset_writer_video];
            asset_writer_video.expectsMediaDataInRealTime = YES;

            asset_writers[i] = asset_writer;
            asset_writer_videos[i] = asset_writer_video;
        }
        active_index = 0;
        circular_encoder_has_looped = false;
    }
    is_initialized = true;
    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
    event[EVENT_PHASE] = EVENT_INIT;
    bool is_error = false;
    if (error) {
        is_error = true;
        event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
    }
    event[EVENT_IS_ERROR] = @(is_error);
    [Utils dispatchEvent:lua_listener event:event];
	return 0;
}

// screenrecorder.start()
-(int)start:(lua_State*)L {
    [Utils checkArgCount:L count:0];

    if (![self check_is_initialized]) return 0;
    preview_view_controller = nil;
    if (capture_params.enable_preview) {
        [RPScreenRecorder.sharedRecorder startRecordingWithHandler:^(NSError * _Nullable error) {
            if (!error) {
                self->is_recording_bool = true;
            } else {
                NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                event[EVENT_PHASE] = EVENT_RECORDED;
                event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
                event[EVENT_IS_ERROR] = @((bool)true);
                [Utils dispatchEvent:self->lua_listener event:event];
            }
        }];
    } else {
        [RPScreenRecorder.sharedRecorder
            startCaptureWithHandler:^(CMSampleBufferRef _Nonnull sample_buffer, RPSampleBufferType buffer_type, NSError * _Nullable error) {
                if (!error) {
                    CFRetain(sample_buffer);
                    dispatch_sync(self->capture_queue, ^{
                        if (self->capture_params.duration) {
                            [self circular_capture_handler:sample_buffer buffer_type:buffer_type];
                        } else {
                            [self capture_handler:sample_buffer buffer_type:buffer_type];
                        }
                    });
                } else {
                    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                    event[EVENT_PHASE] = EVENT_RECORDED;
                    event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
                    event[EVENT_IS_ERROR] = @((bool)true);
                    [Utils dispatchEvent:self->lua_listener event:event];
                }
            }
            completionHandler:^(NSError * _Nullable error) {
                if (!error) {
                    self->is_recording_bool = true;
                }
                if (error) {
                    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                    event[EVENT_PHASE] = EVENT_RECORDED;
                    event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
                    event[EVENT_IS_ERROR] = @((bool)true);
                    [Utils dispatchEvent:self->lua_listener event:event];
                }
            }
        ];
    }
    return 0;
}

// screenrecorder.stop()
-(int)stop:(lua_State*)L {
    [Utils checkArgCount:L count:0];
    if (capture_params.enable_preview) {
        [RPScreenRecorder.sharedRecorder stopRecordingWithHandler:^(RPPreviewViewController * _Nullable previewViewController, NSError * _Nullable error) {
            if (!error && previewViewController) {
                self->preview_view_controller = previewViewController;
            }
            NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
            event[EVENT_PHASE] = EVENT_RECORDED;
            bool is_error = false;
            if (error) {
                is_error = true;
                event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
            } else if (!previewViewController) {
                is_error = true;
                event[EVENT_ERROR_MESSAGE] = @"No preview view controller available.";
            }
            event[EVENT_IS_ERROR] = @(is_error);
            [Utils dispatchEvent:self->lua_listener event:event];
        }];
    } else {
        [RPScreenRecorder.sharedRecorder stopCaptureWithHandler:^(NSError * _Nullable error) {
            if (self->capture_params.duration) {
                [self->asset_writer_videos[self->active_index] markAsFinished];
                [self->asset_writers[self->active_index] finishWritingWithCompletionHandler:^{
                    [self join_partial_video_files];
                }];
            } else {
                [self->asset_writer_videos[0] markAsFinished];
                [self->asset_writers[0] finishWritingWithCompletionHandler:^{
                    self->asset_writer_videos = nil;
                    self->asset_writers = nil;
                    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                    event[EVENT_PHASE] = EVENT_RECORDED;
                    bool is_error = false;
                    if (error) {
                        is_error = true;
                        event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
                    }
                    event[EVENT_IS_ERROR] = @(is_error);
                    [Utils dispatchEvent:self->lua_listener event:event];
                }];
            }
        }];
    }
    is_recording_bool = false;
    is_initialized = false;
    return 0;
}

// screenrecorder.mux_audio_video(params)
-(int)mux_audio_video:(lua_State*)L {
    [Utils checkArgCount:L count:1];

    Scheme *scheme = [[Scheme alloc] init];
    [scheme string:@"audio_filename"];
    [scheme string:@"video_filename"];
    [scheme string:@"filename"];

    Table *params = [[Table alloc] init:L index:1];
    [params parse:scheme];

    NSString *audio_filename = [params getStringNotNull:@"audio_filename"];
    NSString *video_filename = [params getStringNotNull:@"video_filename"];
    NSString *filename = [params getStringNotNull:@"filename"];

    [[NSFileManager defaultManager] removeItemAtPath:filename error:nil];

    NSURL *audio_filename_url = [NSURL fileURLWithPath:audio_filename];
    NSURL *video_filename_url = [NSURL fileURLWithPath:video_filename];
    NSURL *filename_url = [NSURL fileURLWithPath:filename];

    AVMutableComposition *mux_composition = [AVMutableComposition composition];
    NSError *error = nil;

    AVURLAsset *video_asset = [[AVURLAsset alloc] initWithURL:video_filename_url options:nil];
    AVMutableCompositionTrack *video_track = [mux_composition addMutableTrackWithMediaType:AVMediaTypeVideo preferredTrackID:kCMPersistentTrackID_Invalid];
    NSArray *video_tracks = [video_asset tracksWithMediaType:AVMediaTypeVideo];
    if (video_tracks.count > 0) {
        [video_track insertTimeRange:CMTimeRangeMake(kCMTimeZero, video_asset.duration) ofTrack:video_tracks[0] atTime:kCMTimeZero error:&error];
    } else {
        error = [NSError errorWithDomain:SCREENRECORDER code:-1 userInfo:@{NSLocalizedDescriptionKey: NSLocalizedString(@"Video file contains no video tracks.", nil)}];
    }

    if (!error) {
        AVURLAsset *audio_asset = [[AVURLAsset alloc]initWithURL:audio_filename_url options:nil];
        AVMutableCompositionTrack *audio_track = [mux_composition addMutableTrackWithMediaType:AVMediaTypeAudio preferredTrackID:kCMPersistentTrackID_Invalid];
        CMTime audio_duration = audio_asset.duration;
        if (video_asset.duration.value < audio_asset.duration.value) {
            audio_duration = video_asset.duration;
        }
        NSArray *audio_tracks = [audio_asset tracksWithMediaType:AVMediaTypeAudio];
        AVAssetExportSession *asset_export;
        if (audio_tracks.count > 0) {
            [audio_track insertTimeRange:CMTimeRangeMake(kCMTimeZero, audio_duration) ofTrack:audio_tracks[0] atTime:kCMTimeZero error:&error];

            asset_export = [[AVAssetExportSession alloc] initWithAsset:mux_composition presetName:AVAssetExportPresetPassthrough];
            asset_export.outputFileType = AVFileTypeMPEG4;
            asset_export.outputURL = filename_url;
        } else {
            error = [NSError errorWithDomain:SCREENRECORDER code:-1 userInfo:@{NSLocalizedDescriptionKey: NSLocalizedString(@"Audio file contains no audio tracks.", nil)}];
        }

        if (!error) {
            [asset_export exportAsynchronouslyWithCompletionHandler: ^(void ) {
                 bool is_error = asset_export.status != AVAssetExportSessionStatusCompleted;
                 NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                 event[EVENT_PHASE] = EVENT_MUXED;
                 event[EVENT_IS_ERROR] = @(is_error);
                 if (is_error) {
                     event[EVENT_ERROR_MESSAGE] = asset_export.error.localizedDescription;
                 }
                 [Utils dispatchEvent:self->lua_listener event:event];
             }];
        }
    }
    if (error) {
        NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
        event[EVENT_PHASE] = EVENT_MUXED;
        event[EVENT_IS_ERROR] = @((bool)true);
        event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
        [Utils dispatchEvent:lua_listener event:event];
    }
    return 0;
}

// screenrecorder.is_preview_available()
-(int)is_preview_available:(lua_State*)L {
    [Utils checkArgCount:L count:0];
    lua_pushboolean(L, preview_view_controller != nil);
    return 1;
}

// screenrecorder.show_preview()
-(int)show_preview:(lua_State*)L {
    [Utils checkArgCount:L count:0];
    if (preview_view_controller) {
        preview_view_controller.modalPresentationStyle = UIModalPresentationFullScreen;
        [preview_view_controller setPreviewControllerDelegate:self];
        UIWindow *window = dmGraphics::GetNativeiOSUIWindow();
        [window.rootViewController presentViewController:preview_view_controller animated:YES completion:nil];
    }
    return 0;
}

-(void)circular_capture_handler:(CMSampleBufferRef _Nonnull)sample_buffer buffer_type:(RPSampleBufferType)buffer_type {
    if (CMSampleBufferDataIsReady(sample_buffer)) {
        AVAssetWriter *asset_writer = asset_writers[active_index];
        AVAssetWriterInput *asset_writer_video = asset_writer_videos[active_index];
        CMTime current_time = CMSampleBufferGetPresentationTimeStamp(sample_buffer);
        if (asset_writer.status == AVAssetWriterStatusUnknown) {
            asset_writer_start_time = CMSampleBufferGetPresentationTimeStamp(sample_buffer);
            [asset_writer startWriting];
            [asset_writer startSessionAtSourceTime:asset_writer_start_time];
        } else if (asset_writer.status == AVAssetWriterStatusFailed) {
            NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
            event[EVENT_PHASE] = EVENT_RECORDED;
            event[EVENT_ERROR_MESSAGE] = asset_writer.error.localizedDescription;
            event[EVENT_IS_ERROR] = @((bool)true);
            [Utils dispatchEvent:self->lua_listener event:event];
        }
        if (buffer_type == RPSampleBufferTypeVideo && asset_writer_video.isReadyForMoreMediaData && is_recording_bool) {
            [asset_writer_video appendSampleBuffer:sample_buffer];
        }
        if (CMTimeCompare(CMTimeSubtract(current_time, asset_writer_start_time), duration_time) > 0) {
            circular_encoder_has_looped = true;
            active_index = (active_index + 1) % 3;
            [asset_writer_video markAsFinished];
            [asset_writer finishWritingWithCompletionHandler:^{
                int index = (self->active_index + 1) % 3;
                NSString *full_filename = [NSString stringWithFormat:@"%@_%d.mp4", self->capture_params.filename, index];
                [[NSFileManager defaultManager] removeItemAtPath:full_filename error:nil];
                NSError *error = nil;
                self->asset_writers[index] = [AVAssetWriter assetWriterWithURL:[NSURL fileURLWithPath:full_filename] fileType:AVFileTypeMPEG4 error:&error];
                if (!error) {
                    self->asset_writer_videos[index] = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo outputSettings:self->video_settings];
                    [self->asset_writers[index] addInput:self->asset_writer_videos[index]];
                    [self->asset_writer_videos[index] setExpectsMediaDataInRealTime:YES];
                } else {
                    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                    event[EVENT_PHASE] = EVENT_RECORDED;
                    event[EVENT_IS_ERROR] = @((bool)true);
                    event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
                    [Utils dispatchEvent:self->lua_listener event:event];
                }
            }];
        }
    }
    CFRelease(sample_buffer);
}

-(void)capture_handler:(CMSampleBufferRef _Nonnull)sample_buffer buffer_type:(RPSampleBufferType)buffer_type {
    if (CMSampleBufferDataIsReady(sample_buffer)) {
        AVAssetWriter *asset_writer = asset_writers[0];
        AVAssetWriterInput *asset_writer_video = asset_writer_videos[0];
        if (asset_writer.status == AVAssetWriterStatusUnknown) {
            [asset_writer startWriting];
            [asset_writer startSessionAtSourceTime:CMSampleBufferGetPresentationTimeStamp(sample_buffer)];
        } else if (asset_writer.status == AVAssetWriterStatusFailed) {
            NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
            event[EVENT_PHASE] = EVENT_RECORDED;
            event[EVENT_ERROR_MESSAGE] = asset_writer.error.localizedDescription;
            event[EVENT_IS_ERROR] = @((bool)true);
            [Utils dispatchEvent:self->lua_listener event:event];
        }
        if (buffer_type == RPSampleBufferTypeVideo && asset_writer_video.isReadyForMoreMediaData && is_recording_bool) {
            [asset_writer_video appendSampleBuffer:sample_buffer];
        }
    }
    CFRelease(sample_buffer);
}

// Join two temporary video files to produce output video file with desired duration.
-(void)join_partial_video_files {
    [[NSFileManager defaultManager] removeItemAtPath:capture_params.filename error:nil];
    NSURL *filename_url = [NSURL fileURLWithPath:capture_params.filename];
    NSURL *video_filename_url_1 = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@_%d.mp4", capture_params.filename, (active_index + 2) % 3]];
    NSURL *video_filename_url_2 = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@_%d.mp4", capture_params.filename, active_index]];
    NSError *error = nil;

    if (!circular_encoder_has_looped) {
        [[NSFileManager defaultManager] moveItemAtURL:video_filename_url_2 toURL:filename_url error:&error];
        NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
        event[EVENT_PHASE] = EVENT_RECORDED;
        if (error) {
            event[EVENT_IS_ERROR] = @((bool)true);
            event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
        }
        [Utils dispatchEvent:self->lua_listener event:event];
    } else {
        AVMutableComposition *join_composition = [AVMutableComposition composition];

        AVURLAsset *video_asset_1 = [[AVURLAsset alloc] initWithURL:video_filename_url_1 options:nil];
        AVURLAsset *video_asset_2 = [[AVURLAsset alloc] initWithURL:video_filename_url_2 options:nil];
        AVAssetTrack *video_asset_track_1 = nil;
        AVAssetTrack *video_asset_track_2 = nil;

        NSArray *video_tracks = [video_asset_1 tracksWithMediaType:AVMediaTypeVideo];
        if (video_tracks.count > 0) {
            video_asset_track_1 = video_tracks[0];
        } else {
            error = [NSError errorWithDomain:SCREENRECORDER code:-1 userInfo:@{NSLocalizedDescriptionKey: NSLocalizedString(@"Video file contains no video tracks.", nil)}];
        }
        video_tracks = [video_asset_2 tracksWithMediaType:AVMediaTypeVideo];
        if (video_tracks.count > 0) {
            video_asset_track_2 = video_tracks[0];
        } else {
            error = [NSError errorWithDomain:SCREENRECORDER code:-1 userInfo:@{NSLocalizedDescriptionKey: NSLocalizedString(@"Video file contains no video tracks.", nil)}];
        }

        if (!error) {
            AVMutableCompositionTrack *video_track = [join_composition addMutableTrackWithMediaType:AVMediaTypeVideo preferredTrackID:kCMPersistentTrackID_Invalid];

            CMTime video_asset_1_trim_duration = CMTimeSubtract(video_asset_1.duration, video_asset_2.duration);
            [video_track insertTimeRange:CMTimeRangeMake(video_asset_2.duration, video_asset_1_trim_duration) ofTrack:video_asset_track_1 atTime:kCMTimeZero error:&error];
            [video_track insertTimeRange:CMTimeRangeMake(kCMTimeZero, video_asset_2.duration) ofTrack:video_asset_track_2 atTime:video_asset_1_trim_duration error:&error];
            AVAssetExportSession *asset_export;
            asset_export = [[AVAssetExportSession alloc] initWithAsset:join_composition presetName:AVAssetExportPresetPassthrough];
            asset_export.outputFileType = AVFileTypeMPEG4;
            asset_export.outputURL = filename_url;

            if (!error) {
                [asset_export exportAsynchronouslyWithCompletionHandler: ^(void ) {
                    bool is_error = asset_export.status != AVAssetExportSessionStatusCompleted;
                    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
                    event[EVENT_PHASE] = EVENT_RECORDED;
                    event[EVENT_IS_ERROR] = @(is_error);
                    if (is_error) {
                        event[EVENT_ERROR_MESSAGE] = asset_export.error.localizedDescription;
                    }
                    [Utils dispatchEvent:self->lua_listener event:event];
                }];
            }
        }
    }
    self->asset_writer_videos = nil;
    self->asset_writers = nil;
    if (error) {
        NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
        event[EVENT_PHASE] = EVENT_RECORDED;
        event[EVENT_IS_ERROR] = @((bool)true);
        event[EVENT_ERROR_MESSAGE] = error.localizedDescription;
        [Utils dispatchEvent:lua_listener event:event];
    }
}

#pragma mark - RPPreviewViewControllerDelegate -

-(void)previewControllerDidFinish:(RPPreviewViewController *)preview_controller {
    [preview_controller dismissViewControllerAnimated:YES completion:nil];
    preview_view_controller = nil;
    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
    event[EVENT_PHASE] = EVENT_PREVIEW;
    event[EVENT_IS_ERROR] = @((bool)false);
    [Utils dispatchEvent:self->lua_listener event:event];
}

-(void)previewController:(RPPreviewViewController *)preview_controller didFinishWithActivityTypes:(NSSet<NSString *> *)activity_types {
    [preview_controller dismissViewControllerAnimated:YES completion:nil];
    preview_view_controller = nil;
    NSMutableDictionary *actions = [[NSMutableDictionary alloc] init];
    int i = 1;
    for (NSString *action in activity_types) {
        actions[@(i++)] = action;
    }
    NSMutableDictionary *event = [Utils newEvent:SCREENRECORDER];
    event[EVENT_PHASE] = EVENT_PREVIEW;
    event[@"actions"] = actions;
    event[EVENT_IS_ERROR] = @((bool)false);
    [Utils dispatchEvent:self->lua_listener event:event];
}

@end

#pragma mark - Defold lifecycle -

void ScreenRecorder_initialize(lua_State *L) {
	sr = [[ScreenRecorderInterface alloc] init:L];
}

void ScreenRecorder_update(lua_State *L) {
	[Utils executeTasks:L];
}

void ScreenRecorder_finalize(lua_State *L) {
    sr = nil;
}

#endif