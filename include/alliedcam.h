// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file alliedcam.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief AlliedVision Camera Simplified API
 * @version Check Readme file for version info.
 * @date 2023-10-17
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ALLIEDCAM_H_
#define ALLIEDCAM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _Nonnull
/**
 * @brief Indicates that variable must not be NULL.
 *
 */
#define _Nonnull
#endif

#ifndef _Nullable
/**
 * @brief Indicates the variable can be NULL.
 *
 */
#define _Nullable
#endif

#include <stdbool.h>
#include <VmbC/VmbC.h>

/**
 * @brief Handle to an Allied Vision camera.
 *
 */
typedef void *AlliedCameraHandle_t;

/**
 * @brief Callback function for camera image capture events.
 *
 * @details This function is called when an image is captured by the camera. The user must copy the image data from the frame buffer to a separate buffer if the image data is to be used after the callback returns. DO NOT modify the frame->context pointers as they are used internally by the library.
 *
 * @param handle Handle to the camera.
 * @param stream Handle to capture stream.
 * @param frame Image frame containing data.
 * @param user_data User data passed to the callback.
 */
typedef void (*AlliedCaptureCallback)(const AlliedCameraHandle_t handle, const VmbHandle_t stream, VmbFrame_t *frame, void *_Nullable user_data);

/**
 * @brief List available Allied Vision (and other GenICam) cameras.
 *
 * @param cameras Reference to a pointer to store the camera list. Note: Memory is allocated by the function and must be freed by the caller.
 * @param count Reference to a variable to store the number of cameras found.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_list_cameras(VmbCameraInfo_t **_Nonnull cameras, VmbUint32_t *_Nonnull count);

/**
 * @brief Open an Allied Vision Camera by ID.
 *
 * @param handle Pointer to store the camera handle.
 * @param id Camera ID string. If NULL, the first camera found is opened.
 * @param mode Camera access mode. Can be of `VmbAccessModeFull`, `VmbAccessModeRead` or `VmbAccessModeExclusive`.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_open_camera_generic(AlliedCameraHandle_t *_Nonnull handle, const char *_Nullable id, VmbAccessMode_t mode);

/**
 * @brief Open an Allied Vision Camera by ID in exclusive mode.
 *
 * @param handle Pointer to store the camera handle.
 * @param id Camera ID string. If NULL, the first camera found is opened.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
static inline VmbError_t allied_open_camera(AlliedCameraHandle_t *_Nonnull handle, const char *_Nullable id)
{
    return allied_open_camera_generic(handle, id, VmbAccessModeExclusive);
}

/**
 * @brief Allocate and queue a frame buffer for image capture.
 *
 * @param handle Handle to Allied Vision camera.
 * @param num_frames Number of frames to allocate.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_alloc_framebuffer(AlliedCameraHandle_t handle, VmbUint32_t num_frames);

/**
 * @brief Start image acquisition.
 *
 * @param handle Handle to Allied Vision camera.
 * @param callback A callback function to be called when an image is captured.
 * @param user_data Pointer to custom user data to be passed to the callback.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_start_capture(AlliedCameraHandle_t handle, AlliedCaptureCallback callback, void *user_data);

/**
 * @brief Stop image acquisition.
 *
 * @param handle Handle to Allied Vision camera.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_stop_capture(AlliedCameraHandle_t handle);

/**
 * @brief
 *
 * @param handle
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_close_camera(AlliedCameraHandle_t handle);

/**
 * @brief Select the camera temperature source, measured using {@link allied_get_temperature}.
 *
 * @param handle Handle to Allied Vision camera.
 * @param src "Sensor" for sensor temperature, "Mainboard" for FPGA temperature.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_temperature_src(AlliedCameraHandle_t handle, const char *src);

/**
 * @brief Get the selected camera temperature source.
 *
 * @param handle Handle to Allied Vision camera.
 * @param src Pointer to store the temperature source.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_temperature_src(AlliedCameraHandle_t handle, const char **_Nonnull src);

/**
 * @brief Get the list of available temperature sources. User has to free the memory allocated for the lists.
 * 
 * @param handle Handle to Allied Vision camera.
 * @param srcs Pointer to store the list of temperature source strings.
 * @param available Pointer to store the list of booleans indicating whether the temperature source is available.
 * @param count Pointer to store the number of temperature sources.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_temperature_src_list(AlliedCameraHandle_t handle, char ***_Nonnull srcs, VmbBool_t **_Nonnull available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the camera temperature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param temp Pointer to store the temperature.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_temperature(AlliedCameraHandle_t handle, double *_Nonnull temp);

/**
 * @brief Get the camera sensor size.
 *
 * @param handle Handle to Allied Vision camera.
 * @param width Pointer to store the sensor width.
 * @param height Pointer to store the sensor height.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_sensor_size(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull width, VmbInt64_t *_Nonnull height);

/**
 * @brief Set the sensor gain.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Gain value to set.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_gain(AlliedCameraHandle_t handle, double value);

/**
 * @brief Get the sensor gain.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Pointer to store the gain value.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_gain(AlliedCameraHandle_t handle, double *_Nonnull value);

/**
 * @brief Get the sensor gain range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum gain value.
 * @param maxval Pointer to store the maximum gain value.
 * @param step Pointer to store the gain step size.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_gain_range(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nonnull step);

/**
 * @brief Set the camera exposure time in microseconds.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Exposure time in microseconds.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_exposure_us(AlliedCameraHandle_t handle, double value);

/**
 * @brief Get the camera exposure time in microseconds.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Pointer to store the exposure time in microseconds.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_exposure_us(AlliedCameraHandle_t handle, double *_Nonnull value);

/**
 * @brief Get the camera exposure time range in microseconds.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum exposure time in microseconds.
 * @param maxval Pointer to store the maximum exposure time in microseconds.
 * @param step Pointer to store the exposure time step size in microseconds.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_exposure_range_us(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nonnull step);

/**
 * @brief Get the sensor bit depth.
 *
 * @param handle Handle to Allied Vision camera.
 * @param depth Pointer to store the bit depth string.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_sensor_bit_depth(AlliedCameraHandle_t handle, const char **_Nonnull depth);

/**
 * @brief Set the sensor bit depth. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param depth Bit depth string.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_sensor_bit_depth(AlliedCameraHandle_t handle, const char *_Nonnull depth);

/**
 * @brief Get the list of available sensor bit depths. User has to free the memory allocated for the lists.
 *
 * @param handle Handle to Allied Vision camera.
 * @param depths Pointer to store the list of bit depth strings.
 * @param available Pointer to store the list of booleans indicating whether the bit depth is available.
 * @param count Pointer to store the number of bit depths.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_sensor_bit_depth_list(AlliedCameraHandle_t handle, char ***_Nonnull depths, VmbBool_t **_Nonnull available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the camera pixel format.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pixel format string.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_image_format(AlliedCameraHandle_t handle, const char **_Nonnull format);

/**
 * @brief Set the camera pixel format. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pixel format string.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_image_format(AlliedCameraHandle_t handle, const char *_Nonnull format);

/**
 * @brief Get the list of available pixel formats. User has to free the memory allocated for the lists.
 *
 * @param handle Handle to Allied Vision camera.
 * @param formats Pointer to store the list of pixel format strings.
 * @param available Pointer to store the list of booleans indicating whether the pixel format is available.
 * @param count Pointer to store the number of pixel formats.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_image_format_list(AlliedCameraHandle_t handle, char ***_Nonnull formats, VmbBool_t **_Nonnull available, VmbUint32_t *_Nonnull count);

/**
 * @brief Flip the image on the camera.
 *
 * @param handle Handle to Allied Vision camera.
 * @param flipx Flip image horizontally.
 * @param flipy Flip image vertically.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_image_flip(AlliedCameraHandle_t handle, VmbBool_t flipx, VmbBool_t flipy);

/**
 * @brief Get the image flip settings.
 *
 * @param handle Handle to Allied Vision camera.
 * @param flipx Pointer to store the horizontal flip setting.
 * @param flipy Pointer to store the vertical flip setting.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_image_flip(AlliedCameraHandle_t handle, VmbBool_t *_Nonnull flipx, VmbBool_t *_Nonnull flipy);

/**
 * @brief Set the binning factor. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Binning factor.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_binning_factor(AlliedCameraHandle_t handle, VmbUint32_t factor);

/**
 * @brief Get the binning factor.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pointer to store the binning factor.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_binning_factor(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull factor);

/**
 * @brief Get the current binning mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Binning mode string ("Sum" or "Average").
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_binning_mode(AlliedCameraHandle_t handle, const char **_Nonnull mode);

/**
 * @brief Set the binning mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Binning mode string ("Sum" or "Average").
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_binning_mode(AlliedCameraHandle_t handle, const char *_Nonnull mode);

/**
 * @brief Set the image X-Y offset. The offsets are applied after binning.
 *
 * @param handle Handle to Allied Vision camera.
 * @param x Image X offset.
 * @param y Image Y offset.
 * @return VmbError_t
 */
VmbError_t allied_set_image_ofst(AlliedCameraHandle_t handle, VmbUint32_t x, VmbUint32_t y);

/**
 * @brief Get the image X-Y offset after binning.
 *
 * @param handle Handle to Allied Vision camera.
 * @param x Pointer to store the image X offset.
 * @param y Pointer to store the image Y offset.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_image_ofst(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull x, VmbInt64_t *_Nonnull y);

/**
 * @brief Set the image size. The image size is applicable after binning. The image size must be smaller than the sensor size. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param width Image width.
 * @param height Image height.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_image_size(AlliedCameraHandle_t handle, VmbUint32_t width, VmbUint32_t height);

/**
 * @brief Get the image size. The image size is applicable after binning.
 *
 * @param handle Handle to Allied Vision camera.
 * @param width Pointer to store the image width.
 * @param height Pointer to store the image height.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_image_size(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull width, VmbInt64_t *_Nonnull height);

/**
 * @brief Get the camera frame rate.
 *
 * @param handle Handle to Allied Vision camera.
 * @param framerate Pointer to store frame rate in Hz.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_acq_framerate(AlliedCameraHandle_t handle, double *_Nonnull framerate);

/**
 * @brief Set the camera frame rate.
 *
 * @param handle Handle to Allied Vision camera.
 * @param framerate Frame rate in Hz.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_acq_framerate(AlliedCameraHandle_t handle, double framerate);

/**
 * @brief Get the camera frame rate range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum frame rate in Hz.
 * @param maxval Pointer to store the maximum frame rate in Hz.
 * @param step Pointer to store the frame rate step size in Hz.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_acq_framerate_range(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nonnull step);

/**
 * @brief Get the camera indicator LED mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pointer to store indicator mode string.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_mode(AlliedCameraHandle_t handle, const char **_Nonnull mode);

/**
 * @brief Set the camera indicator LED mode.
 * 
 * @param handle Handle to Allied Vision camera.
 * @param mode Indicator mode string.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_indicator_mode(AlliedCameraHandle_t handle, const char *_Nonnull mode);

/**
 * @brief Get the list of available indicator modes. User has to free the memory allocated for the lists.
 * 
 * @param handle Handle to Allied Vision camera.
 * @param modes Pointer to store the list of indicator mode strings.
 * @param available Pointer to store the list of booleans indicating whether the indicator mode is available.
 * @param count Pointer to store the number of indicator modes.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_mode_list(AlliedCameraHandle_t handle, char ***_Nonnull modes, VmbBool_t **_Nonnull available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the camera indicator LED brightness.
 *
 * @param handle Handle to Allied Vision camera.
 * @param luma Pointer to store the indicator brightness.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull luma);

/**
 * @brief Set the camera indicator LED brightness.
 *
 * @param handle Handle to Allied Vision camera.
 * @param luma Indicator brightness.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_set_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t luma);

/**
 * @brief Get the camera indicator LED brightness range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum indicator brightness.
 * @param maxval Pointer to store the maximum indicator brightness.
 * @param step Pointer to store the indicator brightness step size.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_luma_range(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull minval, VmbInt64_t *_Nonnull maxval, VmbInt64_t *_Nonnull step);

/**
 * @brief Get the camera ID string.
 *
 * @param handle Handle to Allied Vision camera.
 * @param id Pointer to store the camera ID string. The memory is allocated by the function and must be freed by the caller.
 * @return VmbError_t VmbErrorSuccess if successful, otherwise an error code.
 */
VmbError_t allied_get_camera_id(AlliedCameraHandle_t handle, char **_Nonnull id);

/**
 * @brief Check if the camera is currently streaming.
 *
 * @param handle Handle to Allied Vision camera.
 * @return true
 * @return false
 */
bool allied_camera_streaming(AlliedCameraHandle_t handle);

/**
 * @brief Check if the camera is currently acquiring images.
 *
 * @param handle Handle to Allied Vision camera.
 * @return true
 * @return false
 */
bool allied_camera_acquiring(AlliedCameraHandle_t handle);

/**
 * @brief Get the message string corresponding to a {@link VmbError_t} status code.
 *
 * @param status Status code.
 * @return const char* Message string.
 */
const char *allied_strerr(VmbError_t status);

#ifdef __cplusplus
}
#endif

#endif /* ALLIEDCAM_H_ */