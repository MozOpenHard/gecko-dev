/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG

#include <TargetConditionals.h>
#include <assert.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdlib.h>
#include <AudioUnit/AudioUnit.h>
#if !TARGET_OS_IPHONE
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/HostTime.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#endif
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb_panner.h"
#if !TARGET_OS_IPHONE
#include "cubeb_osx_run_loop.h"
#endif

#if !defined(kCFCoreFoundationVersionNumber10_7)
/* From CoreFoundation CFBase.h */
#define kCFCoreFoundationVersionNumber10_7 635.00
#endif

#if !TARGET_OS_IPHONE && MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#define MACOSX_LESS_THAN_106
#endif

#define CUBEB_STREAM_MAX 8
#define NBUFS 4

static struct cubeb_ops const audiounit_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  pthread_mutex_t mutex;
  int active_streams;
  int limit_streams;
};

struct cubeb_stream {
  cubeb * context;
  AudioUnit unit;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  cubeb_device_changed_callback device_changed_callback;
  void * user_ptr;
  AudioStreamBasicDescription sample_spec;
  pthread_mutex_t mutex;
  uint64_t frames_played;
  uint64_t frames_queued;
  int shutdown;
  int draining;
  uint64_t current_latency_frames;
  uint64_t hw_latency_frames;
  float panning;
};

#if TARGET_OS_IPHONE
typedef UInt32 AudioDeviceID;
typedef UInt32 AudioObjectID;

#define AudioGetCurrentHostTime mach_absolute_time

uint64_t
AudioConvertHostTimeToNanos(uint64_t host_time)
{
  static struct mach_timebase_info timebase_info;
  static bool initialized = false;
  if (!initialized) {
    mach_timebase_info(&timebase_info);
    initialized = true;
  }

  long double answer = host_time;
  if (timebase_info.numer != timebase_info.denom) {
    answer *= timebase_info.numer;
    answer /= timebase_info.denom;
  }
  return (uint64_t)answer;
}
#endif

static int64_t
audiotimestamp_to_latency(AudioTimeStamp const * tstamp, cubeb_stream * stream)
{
  if (!(tstamp->mFlags & kAudioTimeStampHostTimeValid)) {
    return 0;
  }

  uint64_t pres = AudioConvertHostTimeToNanos(tstamp->mHostTime);
  uint64_t now = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

  return ((pres - now) * stream->sample_spec.mSampleRate) / 1000000000LL;
}

static OSStatus
audiounit_output_callback(void * user_ptr, AudioUnitRenderActionFlags * flags,
                          AudioTimeStamp const * tstamp, UInt32 bus, UInt32 nframes,
                          AudioBufferList * bufs)
{
  cubeb_stream * stm;
  unsigned char * buf;
  long got;
  OSStatus r;
  float panning;

  assert(bufs->mNumberBuffers == 1);
  buf = bufs->mBuffers[0].mData;

  stm = user_ptr;

  pthread_mutex_lock(&stm->mutex);

  stm->current_latency_frames = audiotimestamp_to_latency(tstamp, stm);
  panning = stm->panning;

  if (stm->draining || stm->shutdown) {
    pthread_mutex_unlock(&stm->mutex);
    if (stm->draining) {
      r = AudioOutputUnitStop(stm->unit);
      assert(r == 0);
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
    }
    return noErr;
  }

  pthread_mutex_unlock(&stm->mutex);
  got = stm->data_callback(stm, stm->user_ptr, buf, nframes);
  pthread_mutex_lock(&stm->mutex);
  if (got < 0) {
    /* XXX handle this case. */
    assert(false);
    pthread_mutex_unlock(&stm->mutex);
    return noErr;
  }

  if ((UInt32) got < nframes) {
    size_t got_bytes = got * stm->sample_spec.mBytesPerFrame;
    size_t rem_bytes = (nframes - got) * stm->sample_spec.mBytesPerFrame;

    stm->draining = 1;

    memset(buf + got_bytes, 0, rem_bytes);
  }

  stm->frames_played = stm->frames_queued;
  stm->frames_queued += got;
  pthread_mutex_unlock(&stm->mutex);

  if (stm->sample_spec.mChannelsPerFrame == 2) {
    cubeb_pan_stereo_buffer_float((float*)buf, got, panning);
  }

  return noErr;
}

/*static*/ int
audiounit_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;
  int r;

  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &audiounit_ops;

  r = pthread_mutex_init(&ctx->mutex, NULL);
  assert(r == 0);

  ctx->active_streams = 0;

  ctx->limit_streams = kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber10_7;
#if !TARGET_OS_IPHONE
  cubeb_set_coreaudio_notification_runloop();
#endif

  *context = ctx;

  return CUBEB_OK;
}

static char const *
audiounit_get_backend_id(cubeb * ctx)
{
  return "audiounit";
}

#if !TARGET_OS_IPHONE
static int
audiounit_get_output_device_id(AudioDeviceID * device_id)
{
  UInt32 size;
  OSStatus r;
  AudioObjectPropertyAddress output_device_address = {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  size = sizeof(*device_id);

  r = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &output_device_address,
                                 0,
                                 NULL,
                                 &size,
                                 device_id);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
audiounit_get_input_device_id(AudioDeviceID * device_id)
{
  UInt32 size;
  OSStatus r;
  AudioObjectPropertyAddress input_device_address = {
    kAudioHardwarePropertyDefaultInputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  size = sizeof(*device_id);

  r = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &input_device_address,
                                 0,
                                 NULL,
                                 &size,
                                 device_id);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int audiounit_install_device_changed_callback(cubeb_stream * stm);
static int audiounit_uninstall_device_changed_callback();

static OSStatus
audiounit_property_listener_callback(AudioObjectID id, UInt32 address_count,
                                     const AudioObjectPropertyAddress * addresses,
                                     void * user)
{
  cubeb_stream * stm = (cubeb_stream*) user;

  for (UInt32 i = 0; i < address_count; i++) {
    switch(addresses[i].mSelector) {
    case kAudioHardwarePropertyDefaultOutputDevice:
      /* fall through */
    case kAudioDevicePropertyDataSource:
      pthread_mutex_lock(&stm->mutex);
      if (stm->device_changed_callback) {
        stm->device_changed_callback(stm->user_ptr);
      }
      pthread_mutex_unlock(&stm->mutex);
      break;
    }
  }

  return noErr;
}

static int
audiounit_install_device_changed_callback(cubeb_stream * stm)
{
  OSStatus r;
  AudioDeviceID id;

  /* This event will notify us when the data source on the same device changes,
   * for example when the user plugs in a normal (non-usb) headset in the
   * headphone jack. */
  AudioObjectPropertyAddress alive_address = {
    kAudioDevicePropertyDataSource,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  if (audiounit_get_output_device_id(&id) != noErr) {
    return CUBEB_ERROR;
  }

  r = AudioObjectAddPropertyListener(id, &alive_address,
                                     &audiounit_property_listener_callback,
                                     stm);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  /* This event will notify us when the default audio device changes,
   * for example when the user plugs in a USB headset and the system chooses it
   * automatically as the default, or when another device is chosen in the
   * dropdown list. */
  AudioObjectPropertyAddress default_device_address = {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  r = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                                     &default_device_address,
                                     &audiounit_property_listener_callback,
                                     stm);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
audiounit_uninstall_device_changed_callback(cubeb_stream * stm)
{
  OSStatus r;
  AudioDeviceID id;

  AudioObjectPropertyAddress datasource_address = {
    kAudioDevicePropertyDataSource,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  if (audiounit_get_output_device_id(&id) != noErr) {
    return CUBEB_ERROR;
  }

  r = AudioObjectRemovePropertyListener(id, &datasource_address,
                                        &audiounit_property_listener_callback,
                                        stm);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  AudioObjectPropertyAddress default_device_address = {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  r = AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
                                        &default_device_address,
                                        &audiounit_property_listener_callback,
                                        stm);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

/* Get the acceptable buffer size (in frames) that this device can work with. */
static int
audiounit_get_acceptable_latency_range(AudioValueRange * latency_range)
{
  UInt32 size;
  OSStatus r;
  AudioDeviceID output_device_id;
  AudioObjectPropertyAddress output_device_buffer_size_range = {
    kAudioDevicePropertyBufferFrameSizeRange,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  /* Get the buffer size range this device supports */
  size = sizeof(*latency_range);

  r = AudioObjectGetPropertyData(output_device_id,
                                 &output_device_buffer_size_range,
                                 0,
                                 NULL,
                                 &size,
                                 latency_range);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}
#endif /* !TARGET_OS_IPHONE */

int
audiounit_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
#if TARGET_OS_IPHONE
  //TODO: [[AVAudioSession sharedInstance] maximumOutputNumberOfChannels]
  *max_channels = 2;
#else
  UInt32 size;
  OSStatus r;
  AudioDeviceID output_device_id;
  AudioStreamBasicDescription stream_format;
  AudioObjectPropertyAddress stream_format_address = {
    kAudioDevicePropertyStreamFormat,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  assert(ctx && max_channels);

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  size = sizeof(stream_format);

  r = AudioObjectGetPropertyData(output_device_id,
                                 &stream_format_address,
                                 0,
                                 NULL,
                                 &size,
                                 &stream_format);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  *max_channels = stream_format.mChannelsPerFrame;
#endif
  return CUBEB_OK;
}

static int
audiounit_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_ms)
{
#if TARGET_OS_IPHONE
  //TODO: [[AVAudioSession sharedInstance] inputLatency]
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  AudioValueRange latency_range;
  if (audiounit_get_acceptable_latency_range(&latency_range) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  *latency_ms = (latency_range.mMinimum * 1000 + params.rate - 1) / params.rate;
#endif

  return CUBEB_OK;
}

static int
audiounit_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
#if TARGET_OS_IPHONE
  //TODO
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  UInt32 size;
  OSStatus r;
  Float64 fsamplerate;
  AudioDeviceID output_device_id;
  AudioObjectPropertyAddress samplerate_address = {
    kAudioDevicePropertyNominalSampleRate,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  size = sizeof(fsamplerate);
  r = AudioObjectGetPropertyData(output_device_id,
                                 &samplerate_address,
                                 0,
                                 NULL,
                                 &size,
                                 &fsamplerate);

  if (r != noErr) {
    return CUBEB_ERROR;
  }

  *rate = (uint32_t)fsamplerate;
#endif
  return CUBEB_OK;
}

static void
audiounit_destroy(cubeb * ctx)
{
  int r;

  // Disabling this assert for bug 1083664 -- we seem to leak a stream
  // assert(ctx->active_streams == 0);

  r = pthread_mutex_destroy(&ctx->mutex);
  assert(r == 0);

  free(ctx);
}

static void audiounit_stream_destroy(cubeb_stream * stm);

static int
audiounit_stream_init(cubeb * context, cubeb_stream ** stream, char const * stream_name,
                      cubeb_stream_params stream_params, unsigned int latency,
                      cubeb_data_callback data_callback, cubeb_state_callback state_callback,
                      void * user_ptr)
{
  AudioStreamBasicDescription ss;
#if MACOSX_LESS_THAN_106
  ComponentDescription desc;
  Component comp;
#else
  AudioComponentDescription desc;
  AudioComponent comp;
#endif
  cubeb_stream * stm;
  AURenderCallbackStruct input;
  unsigned int buffer_size, default_buffer_size;
  OSStatus r;
  UInt32 size;
  AudioValueRange latency_range;

  assert(context);
  *stream = NULL;

  memset(&ss, 0, sizeof(ss));
  ss.mFormatFlags = 0;

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    ss.mBitsPerChannel = 16;
    ss.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    break;
  case CUBEB_SAMPLE_S16BE:
    ss.mBitsPerChannel = 16;
    ss.mFormatFlags |= kAudioFormatFlagIsSignedInteger |
      kAudioFormatFlagIsBigEndian;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    ss.mBitsPerChannel = 32;
    ss.mFormatFlags |= kAudioFormatFlagIsFloat;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    ss.mBitsPerChannel = 32;
    ss.mFormatFlags |= kAudioFormatFlagIsFloat |
      kAudioFormatFlagIsBigEndian;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  ss.mFormatID = kAudioFormatLinearPCM;
  ss.mFormatFlags |= kLinearPCMFormatFlagIsPacked;
  ss.mSampleRate = stream_params.rate;
  ss.mChannelsPerFrame = stream_params.channels;

  ss.mBytesPerFrame = (ss.mBitsPerChannel / 8) * ss.mChannelsPerFrame;
  ss.mFramesPerPacket = 1;
  ss.mBytesPerPacket = ss.mBytesPerFrame * ss.mFramesPerPacket;

  pthread_mutex_lock(&context->mutex);
  if (context->limit_streams && context->active_streams >= CUBEB_STREAM_MAX) {
    pthread_mutex_unlock(&context->mutex);
    return CUBEB_ERROR;
  }
  context->active_streams += 1;
  pthread_mutex_unlock(&context->mutex);

  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType =
#if TARGET_OS_IPHONE
    kAudioUnitSubType_RemoteIO;
#else
    kAudioUnitSubType_DefaultOutput;
#endif
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
#if MACOSX_LESS_THAN_106
  comp = FindNextComponent(NULL, &desc);
#else
  comp = AudioComponentFindNext(NULL, &desc);
#endif
  assert(comp);

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->device_changed_callback = NULL;

  stm->sample_spec = ss;

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  r = pthread_mutex_init(&stm->mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  assert(r == 0);

  stm->frames_played = 0;
  stm->frames_queued = 0;
  stm->current_latency_frames = 0;
  stm->hw_latency_frames = UINT64_MAX;

#if MACOSX_LESS_THAN_106
  r = OpenAComponent(comp, &stm->unit);
#else
  r = AudioComponentInstanceNew(comp, &stm->unit);
#endif
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  input.inputProc = audiounit_output_callback;
  input.inputProcRefCon = stm;
  r = AudioUnitSetProperty(stm->unit, kAudioUnitProperty_SetRenderCallback,
                           kAudioUnitScope_Global, 0, &input, sizeof(input));
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  buffer_size = latency / 1000.0 * ss.mSampleRate;

  /* Get the range of latency this particular device can work with, and clamp
   * the requested latency to this acceptable range. */
#if !TARGET_OS_IPHONE
  if (audiounit_get_acceptable_latency_range(&latency_range) != CUBEB_OK) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  if (buffer_size < (unsigned int) latency_range.mMinimum) {
    buffer_size = (unsigned int) latency_range.mMinimum;
  } else if (buffer_size > (unsigned int) latency_range.mMaximum) {
    buffer_size = (unsigned int) latency_range.mMaximum;
  }

  /**
   * Get the default buffer size. If our latency request is below the default,
   * set it. Otherwise, use the default latency.
   **/
  size = sizeof(default_buffer_size);
  r = AudioUnitGetProperty(stm->unit, kAudioDevicePropertyBufferFrameSize,
                           kAudioUnitScope_Output, 0, &default_buffer_size, &size);

  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }
#else  // TARGET_OS_IPHONE
  //TODO: [[AVAudioSession sharedInstance] inputLatency]
  // http://stackoverflow.com/questions/13157523/kaudiodevicepropertybufferframesize-replacement-for-ios
#endif

  // Setting the latency doesn't work well for USB headsets (eg. plantronics).
  // Keep the default latency for now.
#if 0
  if (buffer_size < default_buffer_size) {
    /* Set the maximum number of frame that the render callback will ask for,
     * effectively setting the latency of the stream. This is process-wide. */
    r = AudioUnitSetProperty(stm->unit, kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Output, 0, &buffer_size, sizeof(buffer_size));
    if (r != 0) {
      audiounit_stream_destroy(stm);
      return CUBEB_ERROR;
    }
  }
#endif

  r = AudioUnitSetProperty(stm->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                           0, &ss, sizeof(ss));
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  r = AudioUnitInitialize(stm->unit);
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

#if !TARGET_OS_IPHONE
  /* we dont' check the return value here, because we want to be able to play
   * even if we can't detect device changes. */
  audiounit_install_device_changed_callback(stm);
#endif

  return CUBEB_OK;
}

static void
audiounit_stream_destroy(cubeb_stream * stm)
{
  int r;

  stm->shutdown = 1;

  if (stm->unit) {
    AudioOutputUnitStop(stm->unit);
    AudioUnitUninitialize(stm->unit);
#if MACOSX_LESS_THAN_106
    CloseComponent(stm->unit);
#else
    AudioComponentInstanceDispose(stm->unit);
#endif
  }

#if !TARGET_OS_IPHONE
  audiounit_uninstall_device_changed_callback(stm);
#endif

  r = pthread_mutex_destroy(&stm->mutex);
  assert(r == 0);

  pthread_mutex_lock(&stm->context->mutex);
  assert(stm->context->active_streams >= 1);
  stm->context->active_streams -= 1;
  pthread_mutex_unlock(&stm->context->mutex);

  free(stm);
}

static int
audiounit_stream_start(cubeb_stream * stm)
{
  OSStatus r;
  r = AudioOutputUnitStart(stm->unit);
  assert(r == 0);
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);
  return CUBEB_OK;
}

static int
audiounit_stream_stop(cubeb_stream * stm)
{
  OSStatus r;
  r = AudioOutputUnitStop(stm->unit);
  assert(r == 0);
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  return CUBEB_OK;
}

static int
audiounit_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  pthread_mutex_lock(&stm->mutex);
  *position = stm->frames_played;
  pthread_mutex_unlock(&stm->mutex);
  return CUBEB_OK;
}

int
audiounit_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
#if TARGET_OS_IPHONE
  //TODO
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  pthread_mutex_lock(&stm->mutex);
  if (stm->hw_latency_frames == UINT64_MAX) {
    UInt32 size;
    uint32_t device_latency_frames, device_safety_offset;
    double unit_latency_sec;
    AudioDeviceID output_device_id;
    OSStatus r;
    AudioObjectPropertyAddress latency_address = {
      kAudioDevicePropertyLatency,
      kAudioDevicePropertyScopeOutput,
      kAudioObjectPropertyElementMaster
    };
    AudioObjectPropertyAddress safety_offset_address = {
      kAudioDevicePropertySafetyOffset,
      kAudioDevicePropertyScopeOutput,
      kAudioObjectPropertyElementMaster
    };

    r = audiounit_get_output_device_id(&output_device_id);

    if (r != noErr) {
      pthread_mutex_unlock(&stm->mutex);
      return CUBEB_ERROR;
    }

    size = sizeof(unit_latency_sec);
    r = AudioUnitGetProperty(stm->unit,
                             kAudioUnitProperty_Latency,
                             kAudioUnitScope_Global,
                             0,
                             &unit_latency_sec,
                             &size);
    if (r != noErr) {
      pthread_mutex_unlock(&stm->mutex);
      return CUBEB_ERROR;
    }

    size = sizeof(device_latency_frames);
    r = AudioObjectGetPropertyData(output_device_id,
                                   &latency_address,
                                   0,
                                   NULL,
                                   &size,
                                   &device_latency_frames);
    if (r != noErr) {
      pthread_mutex_unlock(&stm->mutex);
      return CUBEB_ERROR;
    }

    size = sizeof(device_safety_offset);
    r = AudioObjectGetPropertyData(output_device_id,
                                   &safety_offset_address,
                                   0,
                                   NULL,
                                   &size,
                                   &device_safety_offset);
    if (r != noErr) {
      pthread_mutex_unlock(&stm->mutex);
      return CUBEB_ERROR;
    }

    /* This part is fixed and depend on the stream parameter and the hardware. */
    stm->hw_latency_frames =
      (uint32_t)(unit_latency_sec * stm->sample_spec.mSampleRate)
      + device_latency_frames
      + device_safety_offset;
  }

  *latency = stm->hw_latency_frames + stm->current_latency_frames;
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
#endif
}

int audiounit_stream_set_volume(cubeb_stream * stm, float volume)
{
  OSStatus r;

  r = AudioUnitSetParameter(stm->unit,
                            kHALOutputParam_Volume,
                            kAudioUnitScope_Global,
                            0, volume, 0);

  if (r != noErr) {
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

int audiounit_stream_set_panning(cubeb_stream * stm, float panning)
{
  if (stm->sample_spec.mChannelsPerFrame > 2) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  pthread_mutex_lock(&stm->mutex);
  stm->panning = panning;
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
}

int audiounit_stream_get_current_device(cubeb_stream * stm,
                                        cubeb_device ** const  device)
{
#if TARGET_OS_IPHONE
  //TODO
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  OSStatus r;
  UInt32 size;
  UInt32 data;
  char strdata[4];
  AudioDeviceID output_device_id;
  AudioDeviceID input_device_id;

  AudioObjectPropertyAddress datasource_address = {
    kAudioDevicePropertyDataSource,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  AudioObjectPropertyAddress datasource_address_input = {
    kAudioDevicePropertyDataSource,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  *device = NULL;

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  *device = malloc(sizeof(cubeb_device));
  if (!*device) {
    return CUBEB_ERROR;
  }

  size = sizeof(UInt32);
  /* This fails with some USB headset, so simply return an empty string. */
  r = AudioObjectGetPropertyData(output_device_id,
                                 &datasource_address,
                                 0, NULL, &size, &data);
  if (r != noErr) {
    size = 0;
    data = 0;
  }

  (*device)->output_name = malloc(size + 1);
  if (!(*device)->output_name) {
    return CUBEB_ERROR;
  }

  (*device)->output_name = malloc(size + 1);
  if (!(*device)->output_name) {
    return CUBEB_ERROR;
  }

  // Turn the four chars packed into a uint32 into a string
  strdata[0] = (char)(data >> 24);
  strdata[1] = (char)(data >> 16);
  strdata[2] = (char)(data >> 8);
  strdata[3] = (char)(data);

  memcpy((*device)->output_name, strdata, size);
  (*device)->output_name[size] = '\0';

  if (audiounit_get_input_device_id(&input_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  size = sizeof(UInt32);
  r = AudioObjectGetPropertyData(input_device_id, &datasource_address_input, 0, NULL, &size, &data);
  if (r != noErr) {
    printf("Error when getting device !\n");
    size = 0;
    data = 0;
  }

  (*device)->input_name = malloc(size + 1);
  if (!(*device)->input_name) {
    return CUBEB_ERROR;
  }

  // Turn the four chars packed into a uint32 into a string
  strdata[0] = (char)(data >> 24);
  strdata[1] = (char)(data >> 16);
  strdata[2] = (char)(data >> 8);
  strdata[3] = (char)(data);

  memcpy((*device)->input_name, strdata, size);
  (*device)->input_name[size] = '\0';

  return CUBEB_OK;
#endif
}

int audiounit_stream_device_destroy(cubeb_stream * stream,
                                    cubeb_device * device)
{
  free(device->output_name);
  free(device->input_name);
  free(device);
  return CUBEB_OK;
}

int audiounit_stream_register_device_changed_callback(cubeb_stream * stream,
                                                      cubeb_device_changed_callback  device_changed_callback)
{
  pthread_mutex_lock(&stream->mutex);
  stream->device_changed_callback = device_changed_callback;
  pthread_mutex_unlock(&stream->mutex);

  return CUBEB_OK;
}

static struct cubeb_ops const audiounit_ops = {
  .init = audiounit_init,
  .get_backend_id = audiounit_get_backend_id,
  .get_max_channel_count = audiounit_get_max_channel_count,
  .get_min_latency = audiounit_get_min_latency,
  .get_preferred_sample_rate = audiounit_get_preferred_sample_rate,
  .destroy = audiounit_destroy,
  .stream_init = audiounit_stream_init,
  .stream_destroy = audiounit_stream_destroy,
  .stream_start = audiounit_stream_start,
  .stream_stop = audiounit_stream_stop,
  .stream_get_position = audiounit_stream_get_position,
  .stream_get_latency = audiounit_stream_get_latency,
  .stream_set_volume = audiounit_stream_set_volume,
  .stream_set_panning = audiounit_stream_set_panning,
  .stream_get_current_device = audiounit_stream_get_current_device,
  .stream_device_destroy = audiounit_stream_device_destroy,
  .stream_register_device_changed_callback = audiounit_stream_register_device_changed_callback
};
