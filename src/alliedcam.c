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

static atomic_bool is_init = false;

static void shutdown_atexit()
{
    if (is_init)
    {
        VmbShutdown();
    }
}

static VmbError_t vmb_adjust_pkt_sz(const char *id);
static VmbError_t vmb_get_buffer_alignment_by_handle(VmbHandle_t handle, VmbInt64_t * alignment);
static VmbError_t allied_free_framebuf(VmbFrame_t **framebuf, VmbUint32_t num_frames);

#ifdef ALLIED_DEBUG
#define eprintlf(fmt, ...)                                                                     \
    {                                                                                          \
        fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    }
#else
#define eprintlf(fmt, ...) \
    {                      \
        (void)0;           \
    }
#endif

typedef struct _name_handlewrapper
{
    VmbHandle_t handle;
    bool acquiring;
    bool streaming;
    VmbFrame_t *framebuf;
    VmbUint32_t num_frames;
} _AlliedCameraHandle_t;

VmbError_t allied_list_cameras(VmbCameraInfo_t **cameras, VmbUint32_t *count)
{
    assert(cameras);
    assert(count);

    VmbError_t err;
    if (!is_init)
    {
        err = VmbStartup(NULL);
        if (err != VmbErrorSuccess)
        {
            return VmbErrorNotInitialized;
        }
        atexit(shutdown_atexit);
        is_init = true;
    }
    VmbUint32_t camCount = 0;
    err = VmbCamerasList(NULL, 0, &camCount, sizeof(VmbCameraInfo_t)); // get the number of cameras
    if (err != VmbErrorSuccess)
    {
        return err;
    }

    if (camCount == 0)
    {
        printf("no cameras found\n");
        return VmbErrorNotFound;
    }

    VmbCameraInfo_t *res = (VmbCameraInfo_t *)malloc(camCount * sizeof(VmbCameraInfo_t)); // get the camera info
    if (res == NULL)
    {
        printf("insufficient memory available");
        return VmbErrorResources;
    }

    VmbUint32_t countNew = 0;
    err = VmbCamerasList(res, camCount, &countNew, sizeof(VmbCameraInfo_t));
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

char *strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    void *new = malloc(len);
    if (new == NULL)
    {
        return NULL;
    }
    return (char *)memcpy(new, s, len);
}

static VmbError_t vmb_adjust_pkt_sz(const char *id)
{
    assert(id);
    VmbError_t err;
    VmbCameraInfo_t info;
    err = VmbCameraInfoQuery(id, &info, sizeof(info));
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    VmbHandle_t stream = info.streamHandles[0];
    if (VmbErrorSuccess == VmbFeatureCommandRun(stream, ADJUST_PACKAGE_SIZE_COMMAND))
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

VmbError_t allied_open_camera_generic(AlliedCameraHandle_t *handle, const char *id, VmbAccessMode_t mode)
{
    assert(handle);

    VmbError_t err;
    bool id_null = false;
    if (!is_init)
    {
        err = VmbStartup(NULL);
        if (err != VmbErrorSuccess)
        {
            return VmbErrorNotInitialized;
        }
        atexit(shutdown_atexit);
        is_init = true;
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
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)malloc(sizeof(_AlliedCameraHandle_t));
    if (ihandle == NULL)
    {
        goto cleanup;
    }
    memset(ihandle, 0, sizeof(_AlliedCameraHandle_t));
    // open the camera
    eprintlf("Open camera: Handle %p", ihandle);
    err = VmbCameraOpen(id, mode, &(ihandle->handle));
    if (err != VmbErrorSuccess)
    {
        goto cleanup_handle;
    }
    eprintlf("Camera handle: %p", ihandle->handle);
    err = vmb_adjust_pkt_sz(id);
    if (err != VmbErrorSuccess)
    {
        VmbCameraClose(ihandle->handle);
        goto cleanup_handle;
    }
    ihandle->acquiring = false;
    ihandle->streaming = false;
    ihandle->framebuf = NULL;
    ihandle->num_frames = 0;
    *handle = ihandle;
    goto cleanup;
cleanup_handle:
    free(ihandle);
cleanup:
    if (id_null)
    {
        free((void *)id);
    }
    return err;
}

static VmbError_t vmb_get_buffer_alignment_by_handle(VmbHandle_t handle, VmbInt64_t * alignment)
{
    assert(alignment);
    VmbError_t err;
    VmbCameraInfo_t info;
    err = VmbCameraInfoQueryByHandle(handle, &info, sizeof(info));
    *alignment = 1;
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    VmbHandle_t stream = info.streamHandles[0];
    // Evaluate required alignment for frame buffer in case announce frame method is used
    VmbInt64_t nStreamBufferAlignment = 1; // Required alignment of the frame buffer
    if (VmbErrorSuccess != VmbFeatureIntGet(stream, "StreamBufferAlignment", &nStreamBufferAlignment))
        nStreamBufferAlignment = 1;

    if (nStreamBufferAlignment < 1)
        nStreamBufferAlignment = 1;

    *alignment = nStreamBufferAlignment;
    return VmbErrorSuccess;
}

VmbError_t allied_alloc_framebuffer(AlliedCameraHandle_t handle, VmbUint32_t num_frames)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    if (num_frames == 0)
    {
        return VmbErrorBadParameter;
    }
    eprintlf("Allocating %d frames: Handle %p", num_frames, handle);
    eprintlf("Camera handle: %p", ((_AlliedCameraHandle_t *)handle)->handle);
    VmbError_t err;
    VmbUint32_t payloadSize = 0;
    VmbInt64_t alignment = 0;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = vmb_get_buffer_alignment_by_handle(ihandle->handle, &alignment);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbPayloadSizeGet(ihandle->handle, &payloadSize);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    eprintlf("Payload size: %u", payloadSize);
    if (ihandle->framebuf != NULL) // previously allocated frame buffer
    {
        err = allied_stop_capture(handle);
        if (err != VmbErrorSuccess)
        {
            return err;
        }
        err = allied_free_framebuf(&(ihandle->framebuf), ihandle->num_frames);
        if (err != VmbErrorSuccess)
        {
            return err;
        }
    }
    VmbFrame_t *iframebuf = (VmbFrame_t *)malloc(num_frames * sizeof(VmbFrame_t));
    if (iframebuf == NULL)
    {
        return VmbErrorResources;
    }
    // memset the buffer ptrs to NULL
    for (VmbUint32_t i = 0; i < num_frames; i++)
    {
        memset(&iframebuf[i], 0, sizeof(VmbFrame_t));
    }
    // allocate the buffers
    for (VmbUint32_t i = 0; i < num_frames; i++)
    {
        iframebuf[i].buffer = (VmbUchar_t *)aligned_alloc(alignment, payloadSize);
        if (iframebuf[i].buffer == NULL)
        {
            goto err_alloc;
        }
        iframebuf[i].bufferSize = payloadSize;
        iframebuf[i].context[CONTEXT_IDX_HANDLE] = ihandle; // store the handle in the context
    }
    // announce the buffers
    for (VmbUint32_t i = 0; i < num_frames; i++)
    {
        err = VmbFrameAnnounce(ihandle->handle, &iframebuf[i], sizeof(VmbFrame_t));
        if (err != VmbErrorSuccess)
        {
            goto err_alloc;
        }
    }
    ihandle->framebuf = iframebuf;
    ihandle->num_frames = num_frames;
    return VmbErrorSuccess;
err_alloc:
    for (VmbUint32_t i = 0; i < num_frames; i++)
    {
        if (iframebuf[i].buffer != NULL)
        {
            free(iframebuf[i].buffer);
        }
    }
    free(iframebuf);
    return VmbErrorResources;
}

static void FrameCaptureCallback(const VmbHandle_t handle, const VmbHandle_t stream, VmbFrame_t *frame)
{
    // extract user data
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)frame->context[CONTEXT_IDX_HANDLE];
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
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    eprintlf("Starting capture: %p", handle);
    eprintlf("Camera handle: %p", ((_AlliedCameraHandle_t *)handle)->handle);
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    if (ihandle->framebuf == NULL)
    {
        return VmbErrorResources;
    }
    // start the capture engine
    err = VmbCaptureStart(ihandle->handle);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    eprintlf("Started capture");
    ihandle->streaming = true;
    // store callback ptr
    for (VmbUint32_t i = 0; i < ihandle->num_frames; i++)
    {
        ihandle->framebuf[i].context[CONTEXT_CB_HANDLE] = callback;
        ihandle->framebuf[i].context[CONTEXT_DATA_HANDLE] = user_data;
    }
    // queue up the frames
    for (VmbUint32_t i = 0; i < ihandle->num_frames; i++)
    {
        err = VmbCaptureFrameQueue(ihandle->handle, &(ihandle->framebuf[i]), &FrameCaptureCallback);
        if (err != VmbErrorSuccess)
        {
            break;
        }
        eprintlf("Queued frame %d", i);
    }

    if (err == VmbErrorSuccess)
    {
        err = VmbFeatureCommandRun(ihandle->handle, "AcquisitionStart");
        if (err == VmbErrorSuccess)
        {
            ihandle->acquiring = true;
            eprintlf("Started acquisition");
            return VmbErrorSuccess;
        }
    }
    // if we reach here, something went wrong
    allied_stop_capture(handle);
    return VmbErrorInternalFault;
}

VmbError_t allied_stop_capture(AlliedCameraHandle_t handle)
{
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    assert(handle);
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    if (ihandle->acquiring)
    {
        err = VmbFeatureCommandRun(ihandle->handle, "AcquisitionStop");
        if (err != VmbErrorSuccess)
        {
            return err;
        }
        ihandle->acquiring = false;
    }
    if (ihandle->streaming)
    {
        err = VmbCaptureEnd(ihandle->handle);
        if (err != VmbErrorSuccess)
        {
            return err;
        }
        ihandle->streaming = false;
    }
    VmbCaptureQueueFlush(ihandle->handle);
    while (VmbErrorSuccess != VmbFrameRevokeAll(ihandle->handle))
    {
    }
    return VmbErrorSuccess;
}

static VmbError_t allied_free_framebuf(VmbFrame_t **framebuf, VmbUint32_t num_frames)
{
    assert(framebuf);
    VmbFrame_t *frames = *framebuf;
    if (frames == NULL)
    {
        return VmbErrorSuccess;
    }
    for (VmbUint32_t i = 0; i < num_frames; i++)
    {
        if (frames[i].buffer != NULL)
        {
            free(frames[i].buffer);
            frames[i].buffer = NULL;
        }
    }
    free(frames);
    *framebuf = NULL;
    return VmbErrorSuccess;
}

VmbError_t allied_close_camera(AlliedCameraHandle_t *handle)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)(* handle);
    err = allied_stop_capture(*handle);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = allied_free_framebuf(&(ihandle->framebuf), ihandle->num_frames);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbCameraClose(ihandle->handle);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    free(ihandle);
    *handle = NULL;
    return VmbErrorSuccess;
}

VmbError_t allied_set_temperature_src(AlliedCameraHandle_t handle, const char *src)
{
    assert(handle);
    assert(src);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureEnumSet(ihandle->handle, "DeviceTemperatureSelector", src);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    return VmbErrorSuccess;
}

VmbError_t allied_get_temperature_src(AlliedCameraHandle_t handle, const char **src)
{
    assert(handle);
    assert(src);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureEnumGet(ihandle->handle, "DeviceTemperatureSelector", src);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    return VmbErrorSuccess;
}

VmbError_t allied_get_temperature(AlliedCameraHandle_t handle, double *temp)
{
    assert(handle);
    assert(temp);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureFloatGet(ihandle->handle, "DeviceTemperature", temp);
    return err;
}

VmbError_t allied_get_sensor_size(AlliedCameraHandle_t handle, VmbInt64_t *width, VmbInt64_t *height)
{
    assert(handle);
    assert(width);
    assert(height);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureIntGet(ihandle->handle, "SensorWidth", width);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntGet(ihandle->handle, "SensorHeight", height);
    return err;
}

VmbError_t allied_get_image_size(AlliedCameraHandle_t handle, VmbInt64_t *width, VmbInt64_t *height)
{
    assert(handle);
    assert(width);
    assert(height);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *width = 0;
    *height = 0;
    err = VmbFeatureIntGet(ihandle->handle, "Width", width);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntGet(ihandle->handle, "Height", height);
    return err;
}

VmbError_t allied_set_image_size(AlliedCameraHandle_t handle, VmbUint32_t width, VmbUint32_t height)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    if (width == 0 || height == 0)
    {
        return VmbErrorBadParameter;
    }
    if (ihandle->acquiring || ihandle->streaming)
    {
        return VmbErrorBusy;
    }
    err = VmbFeatureIntSet(ihandle->handle, "Width", width);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntSet(ihandle->handle, "Height", height);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = allied_alloc_framebuffer(handle, ihandle->num_frames);
    return err;
}

VmbError_t allied_get_image_ofst(AlliedCameraHandle_t handle, VmbInt64_t *ofst_x, VmbInt64_t *ofst_y)
{
    assert(handle);
    assert(ofst_x);
    assert(ofst_y);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *ofst_x = 0;
    *ofst_y = 0;
    err = VmbFeatureIntGet(ihandle->handle, "OffsetX", ofst_x);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntGet(ihandle->handle, "OffsetY", ofst_y);
    return err;
}

VmbError_t allied_set_image_ofst(AlliedCameraHandle_t handle, VmbUint32_t ofst_x, VmbUint32_t ofst_y)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureIntSet(ihandle->handle, "OffsetX", ofst_x);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntSet(ihandle->handle, "OffsetY", ofst_y);
    return err;
}

VmbError_t allied_set_binning_mode(AlliedCameraHandle_t handle, const char *mode)
{
    assert(handle);
    assert(mode);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureEnumSet(ihandle->handle, "BinningHorizontalMode", mode);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureEnumSet(ihandle->handle, "BinningVerticalMode", mode);
    return err;
}

VmbError_t allied_get_binning_mode(AlliedCameraHandle_t handle, const char **mode)
{
    assert(handle);
    assert(mode);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureEnumGet(ihandle->handle, "BinningHorizontalMode", mode);
    return err;
}

VmbError_t allied_set_binning_factor(AlliedCameraHandle_t handle, VmbUint32_t factor)
{
    assert(handle);
    assert(factor > 0);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    if (ihandle->streaming || ihandle->acquiring)
    {
        return VmbErrorBusy;
    }
    err = VmbFeatureIntSet(ihandle->handle, "BinningHorizontal", factor);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntSet(ihandle->handle, "BinningVertical", factor);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = allied_alloc_framebuffer(handle, ihandle->num_frames);
    return err;
}

VmbError_t allied_get_binning_factor(AlliedCameraHandle_t handle, VmbInt64_t *factor)
{
    assert(handle);
    assert(factor);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *factor = 0;
    VmbInt64_t hx = 0, hy = 0;
    err = VmbFeatureIntGet(ihandle->handle, "BinningHorizontal", &hx);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntGet(ihandle->handle, "BinningVertical", &hy);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
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
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *flipx = VmbBoolFalse;
    *flipy = VmbBoolFalse;
    err = VmbFeatureBoolGet(ihandle->handle, "ReverseX", flipx);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureBoolGet(ihandle->handle, "ReverseY", flipy);
    return err;
}

VmbError_t allied_set_image_flip(AlliedCameraHandle_t handle, VmbBool_t flipx, VmbBool_t flipy)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureBoolSet(ihandle->handle, "ReverseX", flipx);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureBoolSet(ihandle->handle, "ReverseY", flipy);
    return err;
}

VmbError_t allied_set_image_format(AlliedCameraHandle_t handle, const char *format)
{
    assert(handle);
    assert(format);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureEnumSet(ihandle->handle, "PixelFormat", format);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = allied_alloc_framebuffer(handle, ihandle->num_frames);
    return err;
}

VmbError_t allied_get_image_format(AlliedCameraHandle_t handle, const char **format)
{
    assert(handle);
    assert(format);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureEnumGet(ihandle->handle, "PixelFormat", format);
    return err;
}

VmbError_t allied_get_enum_list(AlliedCameraHandle_t handle, const char *name, char ***formats, VmbBool_t **available, VmbUint32_t *count)
{
    VmbError_t err;
    VmbUint32_t list_len = 0;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    VmbHandle_t moduleHandle = ihandle->handle;
    err = VmbFeatureEnumRangeQuery(moduleHandle, name, NULL, 0, &list_len);
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
        err = VmbFeatureEnumRangeQuery(moduleHandle, name, (const char **)_formats, list_len, NULL);
        if (err == VmbErrorSuccess && formats != NULL)
        {
            for (int i = 0; i < list_len; i++)
            {
                err = VmbFeatureEnumIsAvailable(moduleHandle, name, _formats[i], &(_available[i]));
            }
        }
        else
        {
            free(_formats);
            *formats = NULL;
            *available = NULL;
        }
        *formats = _formats;
        *available = _available;
    }
    *count = list_len;
    return VmbErrorSuccess;
}

VmbError_t allied_get_image_format_list(AlliedCameraHandle_t handle, char ***formats, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    assert(formats);
    assert(available);
    assert(count);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_enum_list(handle, "PixelFormat", formats, available, count);
}

VmbError_t allied_get_sensor_bit_depth_list(AlliedCameraHandle_t handle, char ***depths, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    assert(depths);
    assert(available);
    assert(count);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_enum_list(handle, "SensorBitDepth", depths, available, count);
}

VmbError_t allied_get_temperature_src_list(AlliedCameraHandle_t handle, char ***srcs, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    assert(srcs);
    assert(available);
    assert(count);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_enum_list(handle, "DeviceTemperatureSelector", srcs, available, count);
}

VmbError_t allied_get_indicator_mode_list(AlliedCameraHandle_t handle, char ***modes, VmbBool_t **available, VmbUint32_t *count)
{
    assert(handle);
    assert(modes);
    assert(available);
    assert(count);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    return allied_get_enum_list(handle, "DeviceIndicatorMode", modes, available, count);
}

VmbError_t allied_get_indicator_mode(AlliedCameraHandle_t handle, const char **mode)
{
    assert(handle);
    assert(mode);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    err = VmbFeatureEnumGet(ihandle->handle, "DeviceIndicatorMode", mode);

    return err;
}

VmbError_t allied_set_indicator_mode(AlliedCameraHandle_t handle, const char *mode)
{
    assert(handle);
    assert(mode);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureEnumSet(ihandle->handle, "DeviceIndicatorMode", mode);
    return err;
}

VmbError_t allied_get_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t *luma)
{
    assert(handle);
    assert(luma);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureIntGet(ihandle->handle, "DeviceIndicatorLuminance", luma);
    return err;
}

VmbError_t allied_set_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t luma)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureIntSet(ihandle->handle, "DeviceIndicatorLuminance", luma);
    return err;
}

VmbError_t allied_get_indicator_luma_range(AlliedCameraHandle_t handle, VmbInt64_t *minval, VmbInt64_t *maxval, VmbInt64_t *step)
{
    assert(handle);
    assert(minval);
    assert(maxval);
    assert(step);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *minval = 0;
    *maxval = 0;
    *step = 0;

    err = VmbFeatureIntRangeQuery(ihandle->handle, "DeviceIndicatorLuminance", minval, maxval);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureIntIncrementQuery(ihandle->handle, "DeviceIndicatorLuminance", step);
    return err;
}

VmbError_t allied_set_sensor_bit_depth(AlliedCameraHandle_t handle, const char *depth)
{
    assert(handle);
    assert(depth);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    if (ihandle->streaming || ihandle->acquiring)
    {
        return VmbErrorBusy;
    }
    err = VmbFeatureEnumSet(ihandle->handle, "SensorBitDepth", depth);
    return err;
}

VmbError_t allied_get_sensor_bit_depth(AlliedCameraHandle_t handle, const char **depth)
{
    assert(handle);
    assert(depth);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;

    err = VmbFeatureEnumGet(ihandle->handle, "SensorBitDepth", depth);
    return err;
}

VmbError_t allied_get_exposure_range_us(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    assert(handle);
    assert(minval);
    assert(maxval);
    assert(step);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    VmbBool_t _unused;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *minval = 0.0;
    *maxval = 0.0;
    *step = 0.0;

    err = VmbFeatureFloatRangeQuery(ihandle->handle, "ExposureTime", minval, maxval);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureFloatIncrementQuery(ihandle->handle, "ExposureTime", &_unused, step);
    return err;
}

VmbError_t allied_set_exposure_us(AlliedCameraHandle_t handle, double value)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    if (value <= 0.0)
    {
        return VmbErrorInvalidValue;
    }
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    return VmbFeatureFloatSet(ihandle->handle, "ExposureTime", value);
}

VmbError_t allied_get_exposure_us(AlliedCameraHandle_t handle, double *value)
{
    assert(handle);
    assert(value);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *value = 0;
    return VmbFeatureFloatGet(ihandle->handle, "ExposureTime", value);
}

VmbError_t allied_get_gain_range(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    assert(handle);
    assert(minval);
    assert(maxval);
    assert(step);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    VmbBool_t _unused;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *minval = 0.0;
    *maxval = 0.0;
    *step = 0.0;

    err = VmbFeatureFloatRangeQuery(ihandle->handle, "Gain", minval, maxval);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureFloatIncrementQuery(ihandle->handle, "Gain", &_unused, step);
    return err;
}

VmbError_t allied_get_gain(AlliedCameraHandle_t handle, double *value)
{
    assert(handle);
    assert(value);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *value = 0;
    return VmbFeatureFloatGet(ihandle->handle, "Gain", value);
}

VmbError_t allied_set_gain(AlliedCameraHandle_t handle, double value)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    if (value <= 0.0)
    {
        return VmbErrorInvalidValue;
    }
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    return VmbFeatureFloatSet(ihandle->handle, "Gain", value);
}

VmbError_t allied_get_acq_framerate(AlliedCameraHandle_t handle, double * framerate)
{
    assert(handle);
    assert(framerate);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *framerate = 0;
    return VmbFeatureFloatGet(ihandle->handle, "AcquisitionFrameRate", framerate);  
}

VmbError_t allied_set_acq_framerate(AlliedCameraHandle_t handle, double framerate)
{
    assert(handle);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    if (framerate <= 0.0)
    {
        return VmbErrorInvalidValue;
    }
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    return VmbFeatureFloatSet(ihandle->handle, "AcquisitionFrameRate", framerate);  
}

VmbError_t allied_get_acq_framerate_range(AlliedCameraHandle_t handle, double *minval, double *maxval, double *step)
{
    assert(handle);
    assert(minval);
    assert(maxval);
    assert(step);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    VmbBool_t _unused;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    *minval = 0.0;
    *maxval = 0.0;
    *step = 0.0;

    err = VmbFeatureFloatRangeQuery(ihandle->handle, "AcquisitionFrameRate", minval, maxval);
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    err = VmbFeatureFloatIncrementQuery(ihandle->handle, "AcquisitionFrameRate", &_unused, step);
    return err;
}

VmbError_t allied_get_camera_id(AlliedCameraHandle_t handle, char **id)
{
    assert(handle);
    assert(id);
    if (!is_init)
    {
        return VmbErrorNotInitialized;
    }
    VmbError_t err;
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    VmbCameraInfo_t info;
    err = VmbCameraInfoQueryByHandle(ihandle->handle, &info, sizeof(info));
    if (err != VmbErrorSuccess)
    {
        return err;
    }
    *id = strdup(info.cameraIdString);
    return VmbErrorSuccess;
}

bool allied_camera_streaming(AlliedCameraHandle_t handle)
{
    assert(handle);
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    return ihandle->streaming;
}

bool allied_camera_acquiring(AlliedCameraHandle_t handle)
{
    assert(handle);
    _AlliedCameraHandle_t *ihandle = (_AlliedCameraHandle_t *)handle;
    return ihandle->acquiring;
}

const char *allied_strerr(VmbError_t status)
{

    switch (status)
    {
    case VmbErrorSuccess:
        return "Success.";
    case VmbErrorInternalFault:
        return "Unexpected fault in VmbApi or driver.";
    case VmbErrorApiNotStarted:
        return "API not started.";
    case VmbErrorNotFound:
        return "Not found.";
    case VmbErrorBadHandle:
        return "Invalid handle.";
    case VmbErrorDeviceNotOpen:
        return "Device not open.";
    case VmbErrorInvalidAccess:
        return "Invalid access.";
    case VmbErrorBadParameter:
        return "Bad parameter.";
    case VmbErrorStructSize:
        return "Wrong DLL version.";
    case VmbErrorMoreData:
        return "More data is available.";
    case VmbErrorWrongType:
        return "Wrong type.";
    case VmbErrorInvalidValue:
        return "Invalid value.";
    case VmbErrorTimeout:
        return "Timeout.";
    case VmbErrorOther:
        return "TL error.";
    case VmbErrorResources:
        return "Resource not available.";
    case VmbErrorInvalidCall:
        return "Invalid call.";
    case VmbErrorNoTL:
        return "No TL loaded.";
    case VmbErrorNotImplemented:
        return "Not implemented.";
    case VmbErrorNotSupported:
        return "Not supported.";
    case VmbErrorIncomplete:
        return "Operation is not complete.";
    case VmbErrorIO:
        return "IO error.";
    case VmbErrorValidValueSetNotPresent:
        return "No valid value set available.";
    case VmbErrorGenTLUnspecified:
        return "Unspecified GenTL runtime error.";
    case VmbErrorUnspecified:
        return "Unspecified runtime error.";
    case VmbErrorBusy:
        return "The responsible module/entity is busy executing actions.";
    case VmbErrorNoData:
        return "The function has no data to work on.";
    case VmbErrorParsingChunkData:
        return "An error occurred parsing a buffer containing chunk data.";
    case VmbErrorInUse:
        return "Already in use.";
    case VmbErrorUnknown:
        return "Unknown error condition.";
    case VmbErrorXml:
        return "Error parsing xml.";
    case VmbErrorNotAvailable:
        return "Something is not available.";
    case VmbErrorNotInitialized:
        return "Something is not initialized.";
    case VmbErrorInvalidAddress:
        return "The given address is out of range or invalid for internal reasons.";
    case VmbErrorAlready:
        return "Something has already been done.";
    case VmbErrorNoChunkData:
        return "A frame expected to contain chunk data does not contain chunk data.";
    case VmbErrorUserCallbackException:
        return "A callback provided by the user threw an exception.";
    case VmbErrorFeaturesUnavailable:
        return "Feature unavailable for a module.";
    case VmbErrorTLNotFound:
        return "A required transport layer could not be found or loaded.";
    case VmbErrorAmbiguous:
        return "Entity cannot be uniquely identified based on the information provided.";
    case VmbErrorRetriesExceeded:
        return "Allowed retries exceeded without successfully completing the operation.";
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