// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file alliedcam.c
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief Implementation of Allied Vision Camera Simple Interface API
 * @version Check Readme file for version info.
 * @date 2023-10-17
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "alliedcam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <assert.h>

#include <VmbC/VmbC.h>

#ifndef ADJUST_PACKAGE_SIZE_COMMAND
#define ADJUST_PACKAGE_SIZE_COMMAND "GVSPAdjustPacketSize"
#endif // !ADJUST_PACKAGE_SIZE_COMMAND

#ifndef CONTEXT_IDX_HANDLE
#define CONTEXT_IDX_HANDLE 0
#endif // !CONTEXT_IDX_HANDLE

#ifndef CONTEXT_DATA_HANDLE
#define CONTEXT_DATA_HANDLE 1
#endif // !CONTEXT_DATA_HANDLE

#ifndef CONTEXT_CB_HANDLE
#define CONTEXT_CB_HANDLE 2
#endif // !CONTEXT_CB_HANDLE

// Turn off assert checking in release mode
#if (!defined(ALLIED_DEBUG) || ALLIED_DEBUG == 0)
#define NDEBUG
#endif

static atomic_bool is_init = ATOMIC_VAR_INIT(false);

static void shutdown_atexit()
{
    if (is_init)
    {
        VmbShutdown();
    }
}

#if (defined(ALLIED_DEBUG) && (ALLIED_DEBUG & 1))
#define eprintlf(fmt, ...)                                                                     \
    {                                                                                          \
        fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        fflush(stderr);                                                                        \
    }
#else
#define eprintlf(fmt, ...) \
    {                      \
        (void)0;           \
    }
#endif

#if (defined(ALLIED_DEBUG) && (ALLIED_DEBUG & 2))
#define ALLIEDCALL(func, ...)                                                                    \
    ({                                                                                           \
        VmbError_t err = func(__VA_ARGS__);                                                      \
        if (err != VmbErrorSuccess)                                                              \
        {                                                                                        \
            fprintf(stderr, "%s:%d:%s(): %s.\n", __FILE__, __LINE__, #func, allied_strerr(err)); \
            fflush(stderr);                                                                      \
        }                                                                                        \
        err;                                                                                     \
    })

#define ALLIEDEXIT(func, ...)                                                                    \
    ({                                                                                           \
        VmbError_t err = func(__VA_ARGS__);                                                      \
        if (err != VmbErrorSuccess)                                                              \
        {                                                                                        \
            fprintf(stderr, "%s:%d:%s(): %s.\n", __FILE__, __LINE__, #func, allied_strerr(err)); \
            fflush(stderr);                                                                      \
            return err;                                                                          \
        }                                                                                        \
        err;                                                                                     \
    })
#else

/**
 * @brief Macro to call a function.
 *
 * @param func Function to call
 * @param ... Arguments to pass to the function
 *
 * @return VmbError_t Error code
 */
#define ALLIEDCALL(func, ...) func(__VA_ARGS__)

/**
 * @brief Macro to call a function that exits from the call site on error.
 * Returns VmbErrorSuccess on success.
 *
 * @param func Function to call
 * @param ... Arguments to pass to the function
 *
 * @return VmbError_t Error code
 *
 */
#define ALLIEDEXIT(func, ...)               \
    ({                                      \
        VmbError_t err = func(__VA_ARGS__); \
        if (err != VmbErrorSuccess)         \
        {                                   \
            return err;                     \
        }                                   \
        err;                                \
    })
#endif

typedef struct framebuffer_s
{
    size_t alloc_size;  // how much memory is allocated, used for hard realloc
    size_t alignment;   // realloc on alignment change
    size_t num_frames;  // changes with bpp or size change
    VmbUchar_t *buffer; // the actual buffer that is split into frames
    VmbFrame_t *frames; // vmb frames
    bool announced;     // if the frames are announced
} AlliedFrameBuffer_s;

typedef AlliedFrameBuffer_s *AlliedFrameBuffer_t;

typedef struct camera_handle_s
{
    VmbHandle_t handle;
    bool acquiring;
    bool streaming;
    AlliedFrameBuffer_t framebuf;
} _AlliedCameraHandle_s;

/**
 * @brief Adjust the packet size of the camera.
 *
 * @param id Camera ID string
 * @return VmbError_t
 */
static VmbError_t VmbAdjustPktSz(const char *id);
/**
 * @brief Get the buffer alignment by handle.
 *
 * @param handle Internal VmbHandle_t to the camera
 * @param alignment Pointer to store the alignment
 * @return VmbError_t
 */
static VmbError_t VmbGetBufferAlignmentByHandle(VmbHandle_t handle, VmbInt64_t *alignment);
/**
 * @brief Allocate a frame buffer for the camera.
 *
 * @param handle Camera handle
 * @param bufsize Buffer size, in bytes
 * @return VmbError_t
 */
static VmbError_t allied_alloc_framebuf(AlliedCameraHandle_t handle, uint32_t bufsize);
/**
 * @brief Free a frame buffer.
 *
 * @param framebuf Handle to the frame buffer
 * @param frame_only Free only the frames that use the frame buffer, but not the buffer itself
 */
static void allied_free_framebuf(AlliedFrameBuffer_t framebuf, bool frame_only);

/**
 * @brief Reallocate the frame buffer.
 *
 * @param handle Camera handle
 * @return VmbError_t
 */
static VmbError_t allied_realloc_framebuffer(AlliedCameraHandle_t handle);

VmbError_t allied_init_api(const char *config_path)
{
    VmbError_t err = VmbErrorSuccess;
    if (atomic_load(&is_init) == false)
    {
        err = ALLIEDCALL(VmbStartup, config_path);
        if (err != VmbErrorSuccess)
        {
            return err;
        }
        atexit(shutdown_atexit);
        atomic_store(&is_init, true);
    }
    return err;
}

VmbError_t allied_list_cameras(VmbCameraInfo_t **cameras, VmbUint32_t *count)
{
    assert(cameras);
    assert(count);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    VmbUint32_t camCount = 0;
    err = ALLIEDEXIT(VmbCamerasList, NULL, 0, &camCount, sizeof(VmbCameraInfo_t)); // get the number of cameras
    if (camCount == 0)
    {
        eprintlf("no cameras found");
        return VmbErrorNotFound;
    }

    VmbCameraInfo_t *res = (VmbCameraInfo_t *)malloc(camCount * sizeof(VmbCameraInfo_t)); // get the camera info
    if (res == NULL)
    {
        printf("insufficient memory available");
        return VmbErrorResources;
    }

    VmbUint32_t countNew = 0;
    err = ALLIEDCALL(VmbCamerasList, res, camCount, &countNew, sizeof(VmbCameraInfo_t));
    if (err == VmbErrorSuccess || (err == VmbErrorMoreData && camCount < countNew))
    {
        if (countNew == 0)
        {
            err = VmbErrorNotFound;
        }
        else
        {
            *cameras = res;
            *count = countNew > camCount ? camCount : countNew;
            return VmbErrorSuccess;
        }
    }

    free(res);

    return err;
}

static VmbError_t VmbAdjustPktSz(const char *id)
{
    assert(id);
    VmbCameraInfo_t info;
    ALLIEDEXIT(VmbCameraInfoQuery, id, &info, sizeof(info));
    VmbHandle_t stream = info.streamHandles[0];
    if (VmbErrorSuccess == ALLIEDCALL(VmbFeatureCommandRun, stream, ADJUST_PACKAGE_SIZE_COMMAND))
    {
        VmbBool_t isCommandDone = VmbBoolFalse;
        do
        {
            if (VmbErrorSuccess != VmbFeatureCommandIsDone(stream,
                                                           ADJUST_PACKAGE_SIZE_COMMAND,
                                                           &isCommandDone))
            {
                break;
            }
        } while (VmbBoolFalse == isCommandDone);
    }
    return VmbErrorSuccess;
}

VmbError_t allied_open_camera_generic(AlliedCameraHandle_t *handle, const char *id, uint32_t bufsize, VmbAccessMode_t mode)
{
    assert(handle);
    assert(bufsize > 0);

    VmbError_t err;
    bool id_null = false;
    if (atomic_load(&is_init) == false)
    {
        eprintlf("API not initialized");
        return VmbErrorNotInitialized;
    }
    if (id == NULL)
    {
        VmbCameraInfo_t *cameras;
        VmbUint32_t count;
        err = allied_list_cameras(&cameras, &count);
        if (err != VmbErrorSuccess)
        {
            return err;
        }
        if (count == 0)
        {
            return VmbErrorNotFound;
        }
        id = strdup(cameras[0].cameraIdString);
        if (id == NULL)
        {
            return VmbErrorResources;
        }
        id_null = true;
    }
    // allocate memory for camera handle
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)malloc(sizeof(_AlliedCameraHandle_s));
    if (ihandle == NULL)
    {
        goto cleanup;
    }
    memset(ihandle, 0, sizeof(_AlliedCameraHandle_s));
    AlliedFrameBuffer_t framebuf = (AlliedFrameBuffer_t)malloc(sizeof(AlliedFrameBuffer_s));
    if (framebuf == NULL)
    {
        goto cleanup_handle;
    }
    memset(framebuf, 0, sizeof(AlliedFrameBuffer_s));
    // open the camera
    eprintlf("Open camera: Handle %p", ihandle);
    err = ALLIEDCALL(VmbCameraOpen, id, mode, &(ihandle->handle));
    if (err != VmbErrorSuccess)
    {
        goto cleanup_framebuf;
    }
    err = VmbAdjustPktSz(id);
    if (err != VmbErrorSuccess)
    {
        goto cleanup_close;
    }
    ihandle->acquiring = false;
    ihandle->streaming = false;
    ihandle->framebuf = framebuf;
    *handle = ihandle;
    err = allied_alloc_framebuf(ihandle, bufsize);
    if (err != VmbErrorSuccess)
    {
        goto cleanup_close;
    }
    eprintlf("Frame buffer allocated: %p (%zu)", framebuf->buffer, framebuf->alloc_size);
    err = allied_realloc_framebuffer(ihandle);
    if (err != VmbErrorSuccess)
    {
        goto cleanup_close;
    }
    eprintlf("Number of frames: %zu (%u)", framebuf->num_frames, framebuf->frames->bufferSize);
    goto cleanup;
cleanup_close:
    ALLIEDCALL(VmbCameraClose, ihandle->handle);
cleanup_framebuf:
    free(framebuf);
cleanup_handle:
    free(ihandle);
cleanup:
    if (id_null)
    {
        free((void *)id);
    }
    return err;
}

uint32_t allied_get_frame_size(AlliedCameraHandle_t handle)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return 0;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    if (ihandle->framebuf == NULL || ihandle->framebuf->frames == NULL)
    {
        return 0;
    }
    return ihandle->framebuf->frames->bufferSize;
}

static VmbError_t VmbGetBufferAlignmentByHandle(VmbHandle_t handle, VmbInt64_t *alignment)
{
    assert(alignment);
    VmbCameraInfo_t info;
    *alignment = 1;
    ALLIEDEXIT(VmbCameraInfoQueryByHandle, handle, &info, sizeof(info));
    VmbHandle_t stream = info.streamHandles[0];
    // Evaluate required alignment for frame buffer in case announce frame method is used
    VmbInt64_t nStreamBufferAlignment = 1; // Required alignment of the frame buffer
    if (VmbErrorSuccess != VmbFeatureIntGet(
                               stream,
                               "StreamBufferAlignment",
                               &nStreamBufferAlignment))
    {
        nStreamBufferAlignment = 1;
    }

    if (nStreamBufferAlignment < 1)
    {
        nStreamBufferAlignment = 1;
    }

    *alignment = nStreamBufferAlignment;
    return VmbErrorSuccess;
}

static VmbError_t allied_alloc_framebuf(AlliedCameraHandle_t handle, uint32_t bufsize)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    if (bufsize == 0)
    {
        return VmbErrorBadParameter;
    }
    assert(handle);
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbInt64_t alignment = 0;
    ALLIEDEXIT(VmbGetBufferAlignmentByHandle, ihandle->handle, &alignment);
    eprintlf("Alignment: %llu", alignment);
    bufsize = (bufsize / alignment) * alignment;
    VmbUchar_t *ibuffer = (VmbUchar_t *)aligned_alloc(alignment, bufsize);
    if (ibuffer == NULL)
    {
        return VmbErrorResources;
    }
    ihandle->framebuf->buffer = ibuffer;
    ihandle->framebuf->alloc_size = bufsize;
    ihandle->framebuf->alignment = alignment;
    return VmbErrorSuccess;
}

VmbError_t allied_realloc_framebuffer(AlliedCameraHandle_t handle)
{
    assert(handle);
    VmbUint32_t payloadSize = 0;
    VmbInt64_t alignment = 0;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    AlliedFrameBuffer_t framebuf = ihandle->framebuf;
    assert(framebuf);
    ALLIEDEXIT(VmbGetBufferAlignmentByHandle, ihandle->handle, &alignment);
    ALLIEDEXIT(VmbPayloadSizeGet, ihandle->handle, &payloadSize);
    assert(payloadSize % alignment == 0);
    ALLIEDEXIT(allied_stop_capture, handle);
    // check if we need reallocation of the buffer
    bool need_realloc = false;
    if (framebuf->alignment != alignment || framebuf->alloc_size < payloadSize) // if alignment is different, we need to realloc
    {
        need_realloc = true;
    }
    if (need_realloc)
    {
        size_t alloc_size = ihandle->framebuf->alloc_size;
        eprintlf("Previous buffer: %p (%lu, %lu)",
                 ihandle->framebuf->buffer,
                 ihandle->framebuf->alloc_size,
                 ihandle->framebuf->alignment);
        alloc_size = (alloc_size / alignment) * alignment;                // re-align the size
        alloc_size = alloc_size > payloadSize ? alloc_size : payloadSize; // make sure we have enough space
        allied_free_framebuf(ihandle->framebuf, false);
        ALLIEDEXIT(allied_alloc_framebuf, handle, alloc_size);
        eprintlf("Current buffer: %p (%lu, %lu)",
                 ihandle->framebuf->buffer,
                 ihandle->framebuf->alloc_size,
                 ihandle->framebuf->alignment);
    }
    if (
        framebuf->frames == NULL || // if we have no frames, first time setup
        framebuf->frames->bufferSize != payloadSize) // if payload size has changed, we need to recreate the frames
    {
        // payload size has changed, but alignment has not - we need to recreate the frames
        allied_free_framebuf(framebuf, true);
        size_t num_frames = (framebuf->alloc_size / payloadSize) % (ALLIED_MAX_FRAMES + 1);
        assert(num_frames > 0);
        assert(payloadSize <= framebuf->alloc_size);
        VmbFrame_t *iframebuf = (VmbFrame_t *)malloc(num_frames * sizeof(VmbFrame_t));
        if (iframebuf == NULL)
        {
            return VmbErrorResources;
        }
        memset(iframebuf, 0, num_frames * sizeof(VmbFrame_t));
        for (VmbUint32_t i = 0; i < num_frames; i++)
        {
            iframebuf[i].buffer = ihandle->framebuf->buffer + i * payloadSize;
            iframebuf[i].bufferSize = payloadSize;
            iframebuf[i].context[CONTEXT_IDX_HANDLE] = ihandle; // store the handle in the context
        }
        framebuf->frames = iframebuf;
        framebuf->num_frames = num_frames;
        framebuf->announced = false;
    }
    return VmbErrorSuccess;
}

static void FrameCaptureCallback(const VmbHandle_t handle, const VmbHandle_t stream, VmbFrame_t *frame)
{
    // extract user data
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)frame->context[CONTEXT_IDX_HANDLE];
    void *user_data = frame->context[CONTEXT_DATA_HANDLE];
    AlliedCaptureCallback callback_handle = frame->context[CONTEXT_CB_HANDLE];
    // execute the user callback
    (*callback_handle)(ihandle, stream, frame, user_data);
    // make sure user can not mistakenly destroy the callback mechanism
    frame->context[CONTEXT_IDX_HANDLE] = ihandle;
    frame->context[CONTEXT_DATA_HANDLE] = user_data;
    frame->context[CONTEXT_CB_HANDLE] = callback_handle;
    // requeue the frame
    VmbCaptureFrameQueue(handle, frame, &FrameCaptureCallback);
}

VmbError_t allied_start_capture(AlliedCameraHandle_t handle, AlliedCaptureCallback callback, void *user_data)
{
    assert(handle);
    assert(callback);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    eprintlf("Starting capture: %p", handle);
    VmbError_t err;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    if (ihandle->framebuf == NULL)
    {
        return VmbErrorResources;
    }
    // announce the buffers
    for (VmbUint32_t i = 0; i < ihandle->framebuf->num_frames; i++)
    {
        err = ALLIEDCALL(VmbFrameAnnounce, ihandle->handle, &(ihandle->framebuf->frames[i]), sizeof(VmbFrame_t));
        if (err != VmbErrorSuccess)
        {
            goto err_cleanup;
        }
        ihandle->framebuf->announced = true;
    }
    // start the capture engine
    err = ALLIEDCALL(VmbCaptureStart, ihandle->handle);
    if (err != VmbErrorSuccess)
    {
        goto err_cleanup;
    }
    eprintlf("Started capture");
    ihandle->streaming = true;
    // store callback ptr
    for (VmbUint32_t i = 0; i < ihandle->framebuf->num_frames; i++)
    {
        ihandle->framebuf->frames[i].context[CONTEXT_CB_HANDLE] = callback;
        ihandle->framebuf->frames[i].context[CONTEXT_DATA_HANDLE] = user_data;
    }
    // queue up the frames
    for (VmbUint32_t i = 0; i < ihandle->framebuf->num_frames; i++)
    {
        err = ALLIEDCALL(VmbCaptureFrameQueue, ihandle->handle, &(ihandle->framebuf->frames[i]), &FrameCaptureCallback);
        if (err != VmbErrorSuccess)
        {
            break;
        }
        eprintlf("Queued frame %d", i);
    }

    if (err == VmbErrorSuccess)
    {
        err = ALLIEDCALL(VmbFeatureCommandRun, ihandle->handle, "AcquisitionStart");
        if (err == VmbErrorSuccess)
        {
            ihandle->acquiring = true;
            eprintlf("Started acquisition");
            return VmbErrorSuccess;
        }
    }
    // if we reach here, something went wrong
err_cleanup:
    allied_stop_capture(handle);
    return err;
}

VmbError_t allied_stop_capture(AlliedCameraHandle_t handle)
{
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    assert(handle);
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    if (!ihandle->acquiring && !ihandle->streaming)
    {
        return VmbErrorSuccess;
    }
    if (ihandle->acquiring)
    {
        ALLIEDEXIT(VmbFeatureCommandRun, ihandle->handle, "AcquisitionStop");
        ihandle->acquiring = false;
    }
    if (ihandle->streaming)
    {
        ALLIEDEXIT(VmbCaptureEnd, ihandle->handle);
        ihandle->streaming = false;
    }
    VmbCaptureQueueFlush(ihandle->handle);
    while (ihandle->framebuf->announced && (VmbErrorSuccess != VmbFrameRevokeAll(ihandle->handle)))
    {
    }
    ihandle->framebuf->announced = false;
    return VmbErrorSuccess;
}

void allied_free_framebuf(AlliedFrameBuffer_t framebuf, bool frame_only)
{
    assert(framebuf);
    if (framebuf->frames != NULL)
    {
        free(framebuf->frames);
        framebuf->frames = NULL;
    }
    if (frame_only)
    {
        return;
    }
    // clean up the buffer allocation
    if (framebuf->buffer != NULL)
    {
        free(framebuf->buffer);
        framebuf->buffer = NULL;
    }
    memset(framebuf, 0, sizeof(AlliedFrameBuffer_s));
    return;
}

VmbError_t allied_reset_camera(AlliedCameraHandle_t *handle)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)(*handle);
    err = ALLIEDCALL(VmbFeatureCommandRun, ihandle->handle, "DeviceReset");
    VmbCameraClose(ihandle->handle);
    allied_free_framebuf(ihandle->framebuf, false);
    free(ihandle);
    *handle = NULL;
    return err;
}

VmbError_t allied_close_camera(AlliedCameraHandle_t *handle)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)(*handle);
    ALLIEDEXIT(allied_stop_capture, *handle);
    allied_free_framebuf(ihandle->framebuf, false);
    ALLIEDEXIT(VmbCameraClose, ihandle->handle);
    free(ihandle);
    *handle = NULL;
    return VmbErrorSuccess;
}

VmbError_t allied_set_temperature_src(AlliedCameraHandle_t handle, const char *src)
{
    assert(handle);
    assert(src);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "DeviceTemperatureSelector", src);
}

VmbError_t allied_get_temperature_src(AlliedCameraHandle_t handle, const char **src)
{
    assert(handle);
    assert(src);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "DeviceTemperatureSelector", src);
}

VmbError_t allied_get_temperature(AlliedCameraHandle_t handle, double *temp)
{
    assert(handle);
    assert(temp);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureFloatGet, ihandle->handle, "DeviceTemperature", temp);
}

VmbError_t allied_get_sensor_size(AlliedCameraHandle_t handle, VmbInt64_t *width, VmbInt64_t *height)
{
    assert(handle);
    assert(width);
    assert(height);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    ALLIEDEXIT(VmbFeatureIntGet, ihandle->handle, "SensorWidth", width);
    return ALLIEDCALL(VmbFeatureIntGet, ihandle->handle, "SensorHeight", height);
}

VmbError_t allied_get_image_size(AlliedCameraHandle_t handle, VmbInt64_t *width, VmbInt64_t *height)
{
    assert(handle);
    assert(width);
    assert(height);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *width = 0;
    *height = 0;
    ALLIEDEXIT(VmbFeatureIntGet, ihandle->handle, "Width", width);
    return ALLIEDCALL(VmbFeatureIntGet, ihandle->handle, "Height", height);
}

VmbError_t allied_set_image_size(AlliedCameraHandle_t handle, VmbUint32_t width, VmbUint32_t height)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    if (width == 0 || height == 0)
    {
        return VmbErrorBadParameter;
    }
    if (ihandle->acquiring || ihandle->streaming)
    {
        return VmbErrorBusy;
    }
    ALLIEDEXIT(VmbFeatureIntSet, ihandle->handle, "Width", width);
    ALLIEDEXIT(VmbFeatureIntSet, ihandle->handle, "Height", height);
    return ALLIEDCALL(allied_realloc_framebuffer, handle);
}

VmbError_t allied_get_image_ofst(AlliedCameraHandle_t handle, VmbInt64_t *ofst_x, VmbInt64_t *ofst_y)
{
    assert(handle);
    assert(ofst_x);
    assert(ofst_y);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *ofst_x = 0;
    *ofst_y = 0;
    ALLIEDEXIT(VmbFeatureIntGet, ihandle->handle, "OffsetX", ofst_x);
    return ALLIEDCALL(VmbFeatureIntGet, ihandle->handle, "OffsetY", ofst_y);
}

VmbError_t allied_set_image_ofst(AlliedCameraHandle_t handle, VmbUint32_t ofst_x, VmbUint32_t ofst_y)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    ALLIEDEXIT(VmbFeatureIntSet, ihandle->handle, "OffsetX", ofst_x);
    return ALLIEDCALL(VmbFeatureIntSet, ihandle->handle, "OffsetY", ofst_y);
}

VmbError_t allied_set_binning_mode(AlliedCameraHandle_t handle, const char *mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    ALLIEDEXIT(VmbFeatureEnumSet, ihandle->handle, "BinningVerticalMode", mode);
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "BinningHorizontalMode", mode);
}

VmbError_t allied_get_binning_mode(AlliedCameraHandle_t handle, const char **mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "BinningVerticalMode", mode);
}

VmbError_t allied_set_binning_factor(AlliedCameraHandle_t handle, VmbUint32_t factor)
{
    assert(handle);
    assert(factor > 0);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    if (ihandle->streaming || ihandle->acquiring)
    {
        return VmbErrorBusy;
    }
    ALLIEDEXIT(VmbFeatureIntSet, ihandle->handle, "BinningVertical", factor);
    ALLIEDEXIT(VmbFeatureIntSet, ihandle->handle, "BinningHorizontal", factor);
    return ALLIEDCALL(allied_realloc_framebuffer, handle);
}

VmbError_t allied_get_binning_factor(AlliedCameraHandle_t handle, VmbInt64_t *factor)
{
    assert(handle);
    assert(factor);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *factor = 0;
    VmbInt64_t hx = 0, hy = 0;
    ALLIEDEXIT(VmbFeatureIntGet, ihandle->handle, "BinningHorizontal", &hx);
    ALLIEDEXIT(VmbFeatureIntGet, ihandle->handle, "BinningVertical", &hy);
    if (hx != hy)
    {
        return VmbErrorInternalFault;
    }
    *factor = hx;
    return VmbErrorSuccess;
}

VmbError_t allied_get_image_flip(AlliedCameraHandle_t handle, VmbBool_t *flipx, VmbBool_t *flipy)
{
    assert(handle);
    assert(flipx);
    assert(flipy);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *flipx = VmbBoolFalse;
    *flipy = VmbBoolFalse;
    ALLIEDEXIT(VmbFeatureBoolGet, ihandle->handle, "ReverseX", flipx);
    return ALLIEDCALL(VmbFeatureBoolGet, ihandle->handle, "ReverseY", flipy);
}

VmbError_t allied_set_image_flip(AlliedCameraHandle_t handle, VmbBool_t flipx, VmbBool_t flipy)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    ALLIEDEXIT(VmbFeatureBoolSet, ihandle->handle, "ReverseX", flipx);
    return ALLIEDCALL(VmbFeatureBoolSet, ihandle->handle, "ReverseY", flipy);
}

VmbError_t allied_set_image_format(AlliedCameraHandle_t handle, const char *format)
{
    assert(handle);
    assert(format);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    ALLIEDEXIT(VmbFeatureEnumSet, ihandle->handle, "PixelFormat", format);
    return ALLIEDCALL(allied_realloc_framebuffer, handle);
}

VmbError_t allied_get_image_format(AlliedCameraHandle_t handle, const char **format)
{
    assert(handle);
    assert(format);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "PixelFormat", format);
}

VmbError_t allied_get_features_list(AlliedCameraHandle_t handle, VmbFeatureInfo_t **features, VmbUint32_t *count)
{
    assert(handle);
    assert(features);
    assert(count);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbUint32_t list_len = 0;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    // get the length of the list
    ALLIEDEXIT(VmbFeaturesList, moduleHandle, NULL, 0, &list_len, sizeof(VmbFeatureInfo_t));
    // allocate space
    VmbFeatureInfo_t *_features = (VmbFeatureInfo_t *)malloc(list_len * sizeof(VmbFeatureInfo_t));
    if (_features == NULL)
    {
        return VmbErrorResources;
    }
    memset(_features, 0, list_len * sizeof(VmbFeatureInfo_t));
    // get the list
    VmbError_t err = ALLIEDCALL(VmbFeaturesList, moduleHandle, _features, list_len, NULL, sizeof(VmbFeatureInfo_t));
    if (err != VmbErrorSuccess)
    {
        free(_features);
        return err;
    }
    // return vals
    *features = _features;
    *count = list_len;
    return VmbErrorSuccess;
}

VmbError_t allied_get_feature_info(AlliedCameraHandle_t handle, const char *name, VmbFeatureInfo_t *info)
{
    assert(handle);
    assert(name);
    assert(info);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    return ALLIEDCALL(VmbFeatureInfoQuery, moduleHandle, name, info, sizeof(VmbFeatureInfo_t));
}

VmbError_t allied_get_feature_int(AlliedCameraHandle_t handle, const char *name, VmbInt64_t *value)
{
    assert(handle);
    assert(name);
    assert(value);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    return ALLIEDCALL(VmbFeatureIntGet, moduleHandle, name, value);
}

VmbError_t allied_set_feature_int(AlliedCameraHandle_t handle, const char *name, VmbInt64_t value)
{
    assert(handle);
    assert(name);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    return ALLIEDCALL(VmbFeatureIntSet, moduleHandle, name, value);
}

VmbError_t allied_get_feature_int_range(AlliedCameraHandle_t handle, const char *name, VmbInt64_t *minval, VmbInt64_t *maxval, VmbInt64_t *step)
{
    assert(handle);
    assert(name);
    assert(minval);
    assert(maxval);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbInt64_t _step = 0;
    *minval = 0;
    *maxval = 0;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    ALLIEDEXIT(VmbFeatureIntRangeQuery, moduleHandle, name, minval, maxval);
    if (step == NULL)
    {
        return VmbErrorSuccess;
    }
    ALLIEDEXIT(VmbFeatureIntIncrementQuery, moduleHandle, name, &_step);
    *step = _step;
    return VmbErrorSuccess;
}

VmbError_t allied_get_feature_int_valset(AlliedCameraHandle_t handle, const char *name, VmbInt64_t **buffer, VmbUint32_t *count)
{
    assert(handle);
    assert(name);
    assert(buffer);
    assert(count);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    *buffer = NULL;
    *count = 0;

    VmbUint32_t list_len = 0;
    VmbInt64_t *list = NULL;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    // get the length of the list
    ALLIEDEXIT(VmbFeatureIntValidValueSetQuery, moduleHandle, name, NULL, 0, &list_len);
    // allocate space
    list = (VmbInt64_t *)malloc(list_len * sizeof(VmbInt64_t));
    if (list == NULL)
    {
        return VmbErrorResources;
    }
    memset(list, 0, list_len * sizeof(VmbInt64_t));
    // get the list
    VmbError_t err = ALLIEDCALL(VmbFeatureIntValidValueSetQuery, moduleHandle, name, list, list_len, NULL);
    if (err != VmbErrorSuccess)
    {
        free(list);
        return err;
    }
    // return vals
    *buffer = list;
    *count = list_len;
    return VmbErrorSuccess;
}

VmbError_t allied_get_feature_float(AlliedCameraHandle_t handle, const char *name, double *value)
{
    assert(handle);
    assert(name);
    assert(value);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    *value = 0;
    return ALLIEDCALL(VmbFeatureFloatGet, moduleHandle, name, value);
}

VmbError_t allied_set_feature_float(AlliedCameraHandle_t handle, const char *name, double value)
{
    assert(handle);
    assert(name);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    return ALLIEDCALL(VmbFeatureFloatSet, moduleHandle, name, value);
}

VmbError_t allied_get_feature_float_range(AlliedCameraHandle_t handle, const char *name, double *minval, double *maxval, double *step)
{
    assert(handle);
    assert(name);
    assert(minval);
    assert(maxval);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    *minval = 0;
    *maxval = 0;
    if (step != NULL)
    {
        *step = 0;
    }

    double _step = 0;
    VmbBool_t _unused = VmbBoolFalse;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;

    ALLIEDEXIT(VmbFeatureFloatRangeQuery, moduleHandle, name, minval, maxval);
    if (step == NULL)
    {
        return VmbErrorSuccess;
    }
    ALLIEDEXIT(VmbFeatureFloatIncrementQuery, moduleHandle, name, &_unused, &_step);
    *step = _step;
    return VmbErrorSuccess;
}

VmbError_t allied_get_feature_enum(AlliedCameraHandle_t handle, const char *name, char **const value)
{
    static char *empty = "";
    assert(handle);
    assert(name);
    assert(value);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    *value = empty;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;

    return ALLIEDCALL(VmbFeatureEnumGet, moduleHandle, name, (const char **)value);
}

VmbError_t allied_set_feature_enum(AlliedCameraHandle_t handle, const char *name, const char *value)
{
    assert(handle);
    assert(name);
    assert(value);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, name, value);
}

VmbError_t allied_get_feature_enum_list(AlliedCameraHandle_t handle, const char *name, char ***list, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    assert(name);
    assert(list);
    assert(count);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    VmbUint32_t list_len = 0;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    err = ALLIEDCALL(VmbFeatureEnumRangeQuery, moduleHandle, name, NULL, 0, &list_len);
    if (err == VmbErrorSuccess && list_len > 0)
    {
        // 1. Allocate space
        char **_formats = (char **)malloc(list_len * sizeof(char **));
        if (_formats == NULL)
        {
            return VmbErrorResources;
        }
        VmbBool_t *_available = (VmbBool_t *)malloc(list_len * sizeof(VmbBool_t));
        if (_available == NULL)
        {
            free(_formats);
            return VmbErrorResources;
        }
        memset(_available, 0, list_len * sizeof(VmbBool_t));
        memset(_formats, 0, list_len * sizeof(char **));
        // 2. Get the features
        err = ALLIEDCALL(VmbFeatureEnumRangeQuery, moduleHandle, name, (const char **)_formats, list_len, NULL);
        if (err == VmbErrorSuccess && list != NULL)
        {
            for (int i = 0; i < list_len; i++)
            {
                err = ALLIEDCALL(VmbFeatureEnumIsAvailable, moduleHandle, name, _formats[i], &(_available[i]));
            }
        }
        else
        {
            free(_formats);
            free(_available);
            *list = NULL;
            if (available != NULL)
            {
                *available = NULL;
            }
        }
        *list = _formats;
        if (available != NULL)
        {
            *available = _available;
        }
        else
        {
            free(_available);
        }
    }
    *count = list_len;
    return VmbErrorSuccess;
}

void allied_free_list(char ***list)
{
    assert(list);
    char **_list = *list;
    if (_list == NULL)
    {
        return;
    }
    free(_list);
    *list = NULL;
    return;
}

VmbError_t allied_get_trigline(AlliedCameraHandle_t handle, const char **line)
{
    assert(handle);
    assert(line);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "LineSelector", line);
}

VmbError_t allied_set_trigline(AlliedCameraHandle_t handle, const char *line)
{
    assert(handle);
    assert(line);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "LineSelector", line);
}

VmbError_t allied_get_trigline_mode(AlliedCameraHandle_t handle, const char **mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "LineMode", mode);
}

VmbError_t allied_set_trigline_mode(AlliedCameraHandle_t handle, const char *mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "LineMode", mode);
}

VmbError_t allied_get_trigline_src(AlliedCameraHandle_t handle, const char **src)
{
    assert(handle);
    assert(src);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "LineSource", src);
}

VmbError_t allied_set_trigline_src(AlliedCameraHandle_t handle, const char *src)
{
    assert(handle);
    assert(src);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "LineSource", src);
}

VmbError_t allied_get_trigline_src_list(AlliedCameraHandle_t handle, char ***srcs, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "LineSource", srcs, available, count);
}

VmbError_t allied_get_trigline_mode_list(AlliedCameraHandle_t handle, char ***modes, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "LineMode", modes, available, count);
}

VmbError_t allied_get_trigline_polarity(AlliedCameraHandle_t handle, VmbBool_t *polarity)
{
    assert(handle);
    assert(polarity);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureBoolGet, ihandle->handle, "LineInverter", polarity);
}

VmbError_t allied_set_trigline_polarity(AlliedCameraHandle_t handle, VmbBool_t polarity)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureBoolSet, ihandle->handle, "LineInverter", polarity);
}

VmbError_t allied_get_trigline_debounce_mode(AlliedCameraHandle_t handle, char **mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "LineDebouncerMode", (const char **)mode);
}

VmbError_t allied_set_trigline_debounce_mode(AlliedCameraHandle_t handle, const char *mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "LineDebouncerMode", mode);
}

VmbError_t allied_get_trigline_debounce_mode_list(AlliedCameraHandle_t handle, char ***modes, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "LineDebouncerMode", modes, available, count);
}

VmbError_t allied_get_trigline_debounce_time(AlliedCameraHandle_t handle, double *time)
{
    assert(handle);
    assert(time);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureFloatGet, ihandle->handle, "LineDebounceDuration", time);
}

VmbError_t allied_set_trigline_debounce_time(AlliedCameraHandle_t handle, double time)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureFloatSet, ihandle->handle, "LineDebounceDuration", time);
}

VmbError_t allied_get_trigline_debounce_time_range(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    return allied_get_feature_float_range(handle, "LineDebounceDuration", minval, maxval, step);
}

VmbError_t allied_get_image_format_list(AlliedCameraHandle_t handle, char ***formats, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "PixelFormat", formats, available, count);
}

VmbError_t allied_get_sensor_bit_depth_list(AlliedCameraHandle_t handle, char ***depths, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "SensorBitDepth", depths, available, count);
}

VmbError_t allied_get_temperature_src_list(AlliedCameraHandle_t handle, char ***srcs, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "DeviceTemperatureSelector", srcs, available, count);
}

VmbError_t allied_get_triglines_list(AlliedCameraHandle_t handle, char ***lines, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    assert(lines);

    assert(count);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "LineSelector", lines, available, count);
}

VmbError_t allied_get_indicator_mode_list(AlliedCameraHandle_t handle, char ***modes, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);

    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_feature_enum_list(handle, "DeviceIndicatorMode", modes, available, count);
}

VmbError_t allied_get_indicator_mode(AlliedCameraHandle_t handle, const char **mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "DeviceIndicatorMode", mode);
}

VmbError_t allied_set_indicator_mode(AlliedCameraHandle_t handle, const char *mode)
{
    assert(handle);
    assert(mode);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "DeviceIndicatorMode", mode);
}

VmbError_t allied_get_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t *luma)
{
    assert(handle);
    assert(luma);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureIntGet, ihandle->handle, "DeviceIndicatorLuminance", luma);
}

VmbError_t allied_set_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t luma)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureIntSet, ihandle->handle, "DeviceIndicatorLuminance", luma);
}

VmbError_t allied_get_indicator_luma_range(AlliedCameraHandle_t handle, VmbInt64_t *minval, VmbInt64_t *maxval, VmbInt64_t *step)
{
    return allied_get_feature_int_range(handle, "DeviceIndicatorLuminance", minval, maxval, step);
}

VmbError_t allied_set_sensor_bit_depth(AlliedCameraHandle_t handle, const char *depth)
{
    assert(handle);
    assert(depth);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    if (ihandle->streaming || ihandle->acquiring)
    {
        return VmbErrorBusy;
    }
    return ALLIEDCALL(VmbFeatureEnumSet, ihandle->handle, "SensorBitDepth", depth);
}

VmbError_t allied_get_sensor_bit_depth(AlliedCameraHandle_t handle, const char **depth)
{
    assert(handle);
    assert(depth);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureEnumGet, ihandle->handle, "SensorBitDepth", depth);
}

VmbError_t allied_get_exposure_range_us(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    return allied_get_feature_float_range(handle, "ExposureTime", minval, maxval, step);
}

VmbError_t allied_set_exposure_us(AlliedCameraHandle_t handle, double value)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    if (value <= 0.0)
    {
        return VmbErrorInvalidValue;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureFloatSet, ihandle->handle, "ExposureTime", value);
}

VmbError_t allied_get_exposure_us(AlliedCameraHandle_t handle, double *value)
{
    assert(handle);
    assert(value);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *value = 0;
    return ALLIEDCALL(VmbFeatureFloatGet, ihandle->handle, "ExposureTime", value);
}

VmbError_t allied_get_gain_range(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    return allied_get_feature_float_range(handle, "Gain", minval, maxval, step);
}

VmbError_t allied_get_gain(AlliedCameraHandle_t handle, double *value)
{
    assert(handle);
    assert(value);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *value = 0;
    return ALLIEDCALL(VmbFeatureFloatGet, ihandle->handle, "Gain", value);
}

VmbError_t allied_set_gain(AlliedCameraHandle_t handle, double value)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    if (value <= 0.0)
    {
        return VmbErrorInvalidValue;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureFloatSet, ihandle->handle, "Gain", value);
}

VmbError_t allied_get_acq_framerate_auto(AlliedCameraHandle_t handle, bool *auto_on)
{
    assert(handle);
    assert(auto_on);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    VmbBool_t _auto_on = VmbBoolFalse;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    err = ALLIEDCALL(VmbFeatureBoolGet, ihandle->handle, "AcquisitionFrameRateEnable", &_auto_on);
    if (err != VmbErrorSuccess)
    {
        *auto_on = true;
        return err;
    }
    *auto_on = !(_auto_on == VmbBoolTrue);
    return VmbErrorSuccess;
}

VmbError_t allied_set_acq_framerate_auto(AlliedCameraHandle_t handle, bool auto_on)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureBoolSet, ihandle->handle, "AcquisitionFrameRateEnable", !auto_on ? VmbBoolTrue : VmbBoolFalse);
}

VmbError_t allied_get_acq_framerate(AlliedCameraHandle_t handle, double *framerate)
{
    assert(handle);
    assert(framerate);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    *framerate = 0;
    return ALLIEDCALL(VmbFeatureFloatGet, ihandle->handle, "AcquisitionFrameRate", framerate);
}

VmbError_t allied_set_acq_framerate(AlliedCameraHandle_t handle, double framerate)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    if (framerate <= 0.0)
    {
        return VmbErrorInvalidValue;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureFloatSet, ihandle->handle, "AcquisitionFrameRate", framerate);
}

VmbError_t allied_get_acq_framerate_range(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    return allied_get_feature_float_range(handle, "AcquisitionFrameRate", minval, maxval, step);
}

VmbError_t allied_get_camera_id(AlliedCameraHandle_t handle, char **id)
{
    assert(handle);
    assert(id);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbCameraInfo_t info;
    ALLIEDEXIT(VmbCameraInfoQueryByHandle, ihandle->handle, &info, sizeof(info));
    *id = strdup(info.cameraIdString);
    return VmbErrorSuccess;
}

VmbError_t allied_get_link_speed(AlliedCameraHandle_t handle, VmbInt64_t *speed)
{
    assert(handle);
    assert(speed);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureIntGet, ihandle->handle, "DeviceLinkSpeed", speed);
}

VmbError_t allied_get_throughput_limit(AlliedCameraHandle_t handle, VmbInt64_t *speed)
{
    assert(handle);
    assert(speed);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ALLIEDCALL(VmbFeatureIntGet, ihandle->handle, "DeviceLinkThroughputLimit", speed);
}

VmbError_t allied_set_throughput_limit(AlliedCameraHandle_t handle, VmbInt64_t speed)
{
    assert(handle);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    if (speed <= 0)
    {
        return VmbErrorInvalidValue;
    }
    VmbError_t err;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    VmbInt64_t minval = 0, maxval = 0;
    err = allied_get_throughput_limit_range(handle, &minval, &maxval, NULL);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    if ((speed > maxval) || (speed < minval))
    {
        return VmbErrorInvalidValue;
    }
    return ALLIEDCALL(VmbFeatureIntSet, ihandle->handle, "DeviceLinkThroughputLimit", speed);
}

VmbError_t allied_get_throughput_limit_range(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull minval, VmbInt64_t *_Nonnull maxval, VmbInt64_t *_Nullable step)
{
    assert(handle);
    assert(minval);
    assert(maxval);
    if (atomic_load(&is_init) == false)
    {
        return VmbErrorNotInitialized;
    }
    VmbInt64_t _step = 0;
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    ALLIEDEXIT(VmbFeatureIntRangeQuery, ihandle->handle, "DeviceLinkThroughputLimit", minval, maxval);
    if (step == NULL)
    {
        return VmbErrorSuccess;
    }
    ALLIEDEXIT(VmbFeatureIntIncrementQuery, ihandle->handle, "DeviceLinkThroughputLimit", &_step);
    *step = _step;
    return VmbErrorSuccess;
}

bool allied_camera_streaming(AlliedCameraHandle_t handle)
{
    assert(handle);
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ihandle->streaming;
}

bool allied_camera_acquiring(AlliedCameraHandle_t handle)
{
    assert(handle);
    _AlliedCameraHandle_s *ihandle = (_AlliedCameraHandle_s *)handle;
    return ihandle->acquiring;
}

const char *allied_strerr(VmbError_t status)
{
    switch (status)
    {
    case VmbErrorSuccess:
        return "Success";
    case VmbErrorInternalFault:
        return "Unexpected fault in VmbApi or driver";
    case VmbErrorApiNotStarted:
        return "API not started";
    case VmbErrorNotFound:
        return "Not found";
    case VmbErrorBadHandle:
        return "Invalid handle";
    case VmbErrorDeviceNotOpen:
        return "Device not open";
    case VmbErrorInvalidAccess:
        return "Invalid access";
    case VmbErrorBadParameter:
        return "Bad parameter";
    case VmbErrorStructSize:
        return "Wrong DLL version";
    case VmbErrorMoreData:
        return "More data is available";
    case VmbErrorWrongType:
        return "Wrong type";
    case VmbErrorInvalidValue:
        return "Invalid value";
    case VmbErrorTimeout:
        return "Timeout";
    case VmbErrorOther:
        return "TL error";
    case VmbErrorResources:
        return "Resource not available";
    case VmbErrorInvalidCall:
        return "Invalid call";
    case VmbErrorNoTL:
        return "No TL loaded";
    case VmbErrorNotImplemented:
        return "Not implemented";
    case VmbErrorNotSupported:
        return "Not supported";
    case VmbErrorIncomplete:
        return "Operation is not complete";
    case VmbErrorIO:
        return "IO error";
    case VmbErrorValidValueSetNotPresent:
        return "No valid value set available";
    case VmbErrorGenTLUnspecified:
        return "Unspecified GenTL runtime error";
    case VmbErrorUnspecified:
        return "Unspecified runtime error";
    case VmbErrorBusy:
        return "The responsible module/entity is busy executing actions";
    case VmbErrorNoData:
        return "The function has no data to work on";
    case VmbErrorParsingChunkData:
        return "An error occurred parsing a buffer containing chunk data";
    case VmbErrorInUse:
        return "Already in use";
    case VmbErrorUnknown:
        return "Unknown error condition";
    case VmbErrorXml:
        return "Error parsing xml";
    case VmbErrorNotAvailable:
        return "Something is not available";
    case VmbErrorNotInitialized:
        return "Something is not initialized";
    case VmbErrorInvalidAddress:
        return "The given address is out of range or invalid for internal reasons";
    case VmbErrorAlready:
        return "Something has already been done";
    case VmbErrorNoChunkData:
        return "A frame expected to contain chunk data does not contain chunk data";
    case VmbErrorUserCallbackException:
        return "A callback provided by the user threw an exception";
    case VmbErrorFeaturesUnavailable:
        return "Feature unavailable for a module";
    case VmbErrorTLNotFound:
        return "A required transport layer could not be found or loaded";
    case VmbErrorAmbiguous:
        return "Entity cannot be uniquely identified based on the information provided";
    case VmbErrorRetriesExceeded:
        return "Allowed retries exceeded without successfully completing the operation";
    default:
        return status >= VmbErrorCustom ? "User defined error" : "Unknown";
    }
}

const char *transport_layer_to_string(VmbTransportLayerType_t layer_type)
{
    switch (layer_type)
    {
    case VmbTransportLayerTypeCL:
        return "Camera Link";
    case VmbTransportLayerTypeCLHS:
        return "Camera Link HS";
    case VmbTransportLayerTypeCustom:
        return "Custom";
    case VmbTransportLayerTypeCXP:
        return "CoaXPress";
    case VmbTransportLayerTypeEthernet:
        return "Generic Ethernet";
    case VmbTransportLayerTypeGEV:
        return "GigE Vision";
    case VmbTransportLayerTypeIIDC:
        return "IIDC 1394";
    case VmbTransportLayerTypeMixed:
        return "Mixed";
    case VmbTransportLayerTypePCI:
        return "PCI / PCIe";
    case VmbTransportLayerTypeU3V:
        return "USB 3 Vision";
    case VmbTransportLayerTypeUVC:
        return "USB video class";
    default:
        return "[Unknown]";
    }
}
