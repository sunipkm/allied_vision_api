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

#ifndef ALLIED_DEBUG
/**
 * @brief Debug level for the Allied Vision Camera API.
 * Set bit 0 to print specific debug messages.
 * Set bit 1 to print error messages on Vimba API call errors.
 *
 */
#define ALLIED_DEBUG 0
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

#ifndef _Const
/**
 * @brief Indicates that the variable/pointer is constant.
 *
 */
#define _Const
#endif

#ifndef KIB
/**
 * @brief Convert KiB to bytes.
 *
 */
#define KIB(x) (x * 1024)
#endif // !KIB

#ifndef MIB
/**
 * @brief Convert MiB to bytes.
 *
 */
#define MIB(x) (x * 1024 * 1024)
#endif // !MIB

#include <stdbool.h>
#include <VmbC/VmbCTypeDefinitions.h>

/**
 * @brief Handle to an Allied Vision camera.
 *
 */
typedef void *AlliedCameraHandle_t;

/**
 * @brief Callback function for camera image capture events.
 *
 * @details This function is called when an image is captured by the camera. The user must copy the image data from the frame buffer to a separate buffer if the image data is to be used after the callback returns. DO NOT modify the `frame->context` pointers as they are used internally by the library.
 *
 * @param handle Handle to the camera.
 * @param stream Handle to capture stream.
 * @param frame Image frame containing data.
 * @param user_data User data passed to the callback.
 */
typedef void (*AlliedCaptureCallback)(const AlliedCameraHandle_t, const VmbHandle_t, VmbFrame_t *_Nonnull, void *_Nullable);

/**
 * @brief Start the Allied Vision Camera API. This function MUST be called before any other function in this library.
 * This function registers an {@link atexit} handler to stop the API when the program exits.
 *
 * @param config_path A string containing a semicolon (Windows) or colon (other os) separated list of paths. The paths contain directories to search for .cti files, paths to .cti files and optionally the path to a configuration xml file. If null is passed the parameter is the cti files found in the paths the GENICAM_GENTL{32|64}_PATH environment variable are considered.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_init_api(const char *_Nullable config_path);

/**
 * @brief List available Allied Vision (and other GenICam) cameras. {@link allied_init_api} MUST be called before calling this function.
 *
 * @param cameras Reference to a pointer to store the camera list. Note: Memory is allocated by the function and must be freed by the caller.
 * @param count Reference to a variable to store the number of cameras found.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_list_cameras(VmbCameraInfo_t **_Nonnull cameras, VmbUint32_t *_Nonnull count);

/**
 * @brief Open an Allied Vision Camera by ID.
 *
 * @param handle Pointer to store the camera handle.
 * @param id Camera ID string. If NULL, the first camera found is opened.
 * @param bufsize Size of the frame buffer for this camera in bytes. Must be greater than 0.
 * @param mode Camera access mode. Can be of `VmbAccessModeFull`, `VmbAccessModeRead` or `VmbAccessModeExclusive`.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_open_camera_generic(AlliedCameraHandle_t *_Nonnull handle, const char *_Nullable id, uint32_t bufsize, VmbAccessMode_t mode);

/**
 * @brief Open an Allied Vision Camera by ID in exclusive mode.
 *
 * @param handle Pointer to store the camera handle.
 * @param id Camera ID string. If NULL, the first camera found is opened.
 * @param bufsize Size of the frame buffer for this camera in bytes. Must be greater than 0.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
static inline VmbError_t allied_open_camera(AlliedCameraHandle_t *_Nonnull handle, const char *_Nullable id, uint32_t bufsize)
{
    return allied_open_camera_generic(handle, id, bufsize, VmbAccessModeExclusive);
}

/**
 * @brief Get the size of images in bytes.
 *
 * @param handle Handle to Allied Vision camera.
 * @return Image size in bytes.
 */
uint32_t allied_get_frame_size(AlliedCameraHandle_t handle);

/**
 * @brief Start image acquisition.
 *
 * @param handle Handle to Allied Vision camera.
 * @param callback A callback function to be called when an image is captured.
 * @param user_data Pointer to custom user data to be passed to the callback.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_start_capture(AlliedCameraHandle_t handle, AlliedCaptureCallback _Nonnull callback, void *user_data);

/**
 * @brief Stop image acquisition.
 *
 * @param handle Handle to Allied Vision camera.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_stop_capture(AlliedCameraHandle_t handle);

/**
 * @brief Close the camera and free all associated memory allocations.
 * Reuse of the camera handle without calling {@link allied_open_camera} again will cause your program to crash.
 *
 * @param handle Pointer to the camera handle.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_close_camera(AlliedCameraHandle_t *handle);

/**
 * @brief Reset the camera. This is a soft-reset, and this operation closes the camera handle. The camera must be reopened after this operation.
 *
 * @param handle Pointer to the camera handle.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_reset_camera(AlliedCameraHandle_t *handle);

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
 * @brief Select the camera temperature source, measured using {@link allied_get_temperature}.
 *
 * @param handle Handle to Allied Vision camera.
 * @param src "Sensor" for sensor temperature, "Mainboard" for FPGA temperature.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_temperature_src(AlliedCameraHandle_t handle, const char *src);

/**
 * @brief Get the selected camera temperature source.
 *
 * @param handle Handle to Allied Vision camera.
 * @param src Pointer to store the temperature source.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_temperature_src(AlliedCameraHandle_t handle, const char **_Nonnull src);

/**
 * @brief Get the list of available temperature sources. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param srcs Pointer to store the list of temperature source strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the temperature source is available. Pass NULL if not required.
 * @param count Pointer to store the number of temperature sources.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_temperature_src_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *srcs, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the camera temperature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param temp Pointer to store the temperature.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_temperature(AlliedCameraHandle_t handle, double *_Nonnull temp);

/**
 * @brief Get the camera sensor size.
 *
 * @param handle Handle to Allied Vision camera.
 * @param width Pointer to store the sensor width.
 * @param height Pointer to store the sensor height.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_sensor_size(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull width, VmbInt64_t *_Nonnull height);

/**
 * @brief Set the sensor gain.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Gain value to set.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_gain(AlliedCameraHandle_t handle, double value);

/**
 * @brief Get the sensor gain.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Pointer to store the gain value.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_gain(AlliedCameraHandle_t handle, double *_Nonnull value);

/**
 * @brief Get the sensor gain range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum gain value.
 * @param maxval Pointer to store the maximum gain value.
 * @param step Pointer to store the gain step size.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_gain_range(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nullable step);

/**
 * @brief Set the camera exposure time in microseconds.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Exposure time in microseconds.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_exposure_us(AlliedCameraHandle_t handle, double value);

/**
 * @brief Get the camera exposure time in microseconds.
 *
 * @param handle Handle to Allied Vision camera.
 * @param value Pointer to store the exposure time in microseconds.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_exposure_us(AlliedCameraHandle_t handle, double *_Nonnull value);

/**
 * @brief Get the camera exposure time range in microseconds.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum exposure time in microseconds.
 * @param maxval Pointer to store the maximum exposure time in microseconds.
 * @param step Pointer to store the exposure time step size in microseconds.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_exposure_range_us(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nullable step);

/**
 * @brief Get the sensor bit depth.
 *
 * @param handle Handle to Allied Vision camera.
 * @param depth Pointer to store the bit depth string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_sensor_bit_depth(AlliedCameraHandle_t handle, const char **_Nonnull depth);

/**
 * @brief Set the sensor bit depth. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param depth Bit depth string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_sensor_bit_depth(AlliedCameraHandle_t handle, const char *_Nonnull depth);

/**
 * @brief Get the list of available sensor bit depths. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param depths Pointer to store the list of bit depth strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the bit depth is available. Pass NULL if not required.
 * @param count Pointer to store the number of bit depths.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_sensor_bit_depth_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *depths, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the camera pixel format.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pixel format string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_image_format(AlliedCameraHandle_t handle, const char **_Nonnull format);

/**
 * @brief Set the camera pixel format. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pixel format string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_image_format(AlliedCameraHandle_t handle, const char *_Nonnull format);

/**
 * @brief Get the list of available pixel formats. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param formats Pointer to store the list of pixel format strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the pixel format is available. Pass NULL if not required.
 * @param count Pointer to store the number of pixel formats.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_image_format_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *formats, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Flip the image on the camera.
 *
 * @param handle Handle to Allied Vision camera.
 * @param flipx Flip image horizontally.
 * @param flipy Flip image vertically.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_image_flip(AlliedCameraHandle_t handle, VmbBool_t flipx, VmbBool_t flipy);

/**
 * @brief Get the image flip settings.
 *
 * @param handle Handle to Allied Vision camera.
 * @param flipx Pointer to store the horizontal flip setting.
 * @param flipy Pointer to store the vertical flip setting.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_image_flip(AlliedCameraHandle_t handle, VmbBool_t *_Nonnull flipx, VmbBool_t *_Nonnull flipy);

/**
 * @brief Set the binning factor. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Binning factor.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_binning_factor(AlliedCameraHandle_t handle, VmbUint32_t factor);

/**
 * @brief Get the binning factor.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pointer to store the binning factor.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_binning_factor(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull factor);

/**
 * @brief Get the current binning mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Binning mode string ("Sum" or "Average").
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_binning_mode(AlliedCameraHandle_t handle, const char **_Nonnull mode);

/**
 * @brief Set the binning mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Binning mode string ("Sum" or "Average").
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
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
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_image_ofst(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull x, VmbInt64_t *_Nonnull y);

/**
 * @brief Set the image size. The image size is applicable after binning. The image size must be smaller than the sensor size. The camera must not be capturing when this function is called.
 *
 * @param handle Handle to Allied Vision camera.
 * @param width Image width.
 * @param height Image height.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_image_size(AlliedCameraHandle_t handle, VmbUint32_t width, VmbUint32_t height);

/**
 * @brief Get the image size. The image size is applicable after binning.
 *
 * @param handle Handle to Allied Vision camera.
 * @param width Pointer to store the image width.
 * @param height Pointer to store the image height.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_image_size(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull width, VmbInt64_t *_Nonnull height);

/**
 * @brief Check automatic frame rate control status.
 *
 * @param handle Handle to Allied Vision camera.
 * @param auto_on Pointer to store automatic frame rate control status.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_acq_framerate_auto(AlliedCameraHandle_t handle, bool *_Nonnull auto_on);

/**
 * @brief Enable/disable automatic frame rate control.
 *
 * @param handle Handle to Allied Vision camera.
 * @param auto_on Enable/disable automatic frame rate control.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_acq_framerate_auto(AlliedCameraHandle_t handle, bool auto_on);

/**
 * @brief Get the camera frame rate.
 *
 * @param handle Handle to Allied Vision camera.
 * @param framerate Pointer to store frame rate in Hz.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_acq_framerate(AlliedCameraHandle_t handle, double *_Nonnull framerate);

/**
 * @brief Set the camera frame rate. Automatic frame rate control must be disabled using {@link allied_set_acq_framerate_auto} for this function to work.
 *
 * @param handle Handle to Allied Vision camera.
 * @param framerate Frame rate in Hz.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_acq_framerate(AlliedCameraHandle_t handle, double framerate);

/**
 * @brief Get the camera frame rate range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum frame rate in Hz.
 * @param maxval Pointer to store the maximum frame rate in Hz.
 * @param step Pointer to store the frame rate step size in Hz.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_acq_framerate_range(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nullable step);

/**
 * @brief Get the camera indicator LED mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pointer to store indicator mode string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_mode(AlliedCameraHandle_t handle, const char **_Nonnull mode);

/**
 * @brief Set the camera indicator LED mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Indicator mode string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_indicator_mode(AlliedCameraHandle_t handle, const char *_Nonnull mode);

/**
 * @brief Get the list of available indicator modes. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param modes Pointer to store the list of indicator mode strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the indicator mode is available. Pass NULL if not required.
 * @param count Pointer to store the number of indicator modes.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_mode_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *modes, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the camera indicator LED brightness.
 *
 * @param handle Handle to Allied Vision camera.
 * @param luma Pointer to store the indicator brightness.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull luma);

/**
 * @brief Set the camera indicator LED brightness.
 *
 * @param handle Handle to Allied Vision camera.
 * @param luma Indicator brightness.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_indicator_luma(AlliedCameraHandle_t handle, VmbInt64_t luma);

/**
 * @brief Get the camera indicator LED brightness range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum indicator brightness.
 * @param maxval Pointer to store the maximum indicator brightness.
 * @param step Pointer to store the indicator brightness step size.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_indicator_luma_range(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull minval, VmbInt64_t *_Nonnull maxval, VmbInt64_t *_Nullable step);

/**
 * @brief Get the list of available trigger lines. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param lines Pointer to store the available trigger lines list. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the trigger line is available. Pass NULL if not required.
 * @param count Number of trigger lines.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_triglines_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *lines, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the trigger line currently selected for configuration.
 *
 * @param handle Handle to Allied Vision camera.
 * @param line Pointer to store the trigger line name.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline(AlliedCameraHandle_t handle, const char **_Nonnull line);

/**
 * @brief Set the trigger line to configure.
 *
 * @param handle Handle to Allied Vision camera.
 * @param line Trigger line name.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_trigline(AlliedCameraHandle_t handle, const char *_Nonnull line);

/**
 * @brief Get the trigger line mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Pointer to store the trigger line mode string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_mode(AlliedCameraHandle_t handle, const char **_Nonnull mode);

/**
 * @brief Set the trigger line mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param mode Trigger line mode string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_trigline_mode(AlliedCameraHandle_t handle, const char *_Nonnull mode);

/**
 * @brief Get the list of available trigger line modes. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param modes Pointer to store the list of trigger line mode strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the trigger line mode is available. Pass NULL if not required.
 * @param count Pointer to store the number of trigger line modes.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_mode_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *modes, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the trigger line source.
 * NOTE: The trigger line must be in "Output" mode to get the source.
 *
 * @param handle Handle to Allied Vision camera.
 * @param src Pointer to store the trigger line source string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_src(AlliedCameraHandle_t handle, const char **_Nonnull src);

/**
 * @brief Set the trigger line source.
 * NOTE: The trigger line must be in "Output" mode to set the source.
 *
 * @param handle Handle to Allied Vision camera.
 * @param src Trigger line source string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_trigline_src(AlliedCameraHandle_t handle, const char *_Nonnull src);

/**
 * @brief Get the list of available trigger line sources. User has to free the memory allocated for the set using the {@allied_free_list} function.
 * NOTE: The trigger line must be in "Output" mode to get the source list.
 *
 * @param handle Handle to Allied Vision camera.
 * @param srcs Pointer to store the list of trigger line source strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the trigger line source is available. Pass NULL if not required.
 * @param count Pointer to store the number of trigger line sources.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_src_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *srcs, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Free the memory allocated for by `allied_get_*_list` functions.
 *
 * @param list Pointer to the list.
 */
void allied_free_list(char *_Nonnull *_Const *list);

/**
 * @brief Get the trigger line polarity.
 *
 * @param handle Handle to Allied Vision camera.
 * @param polarity Pointer to store the trigger line polarity.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_polarity(AlliedCameraHandle_t handle, VmbBool_t *inverted);

/**
 * @brief Set the trigger line polarity.
 *
 * @param handle Handle to Allied Vision camera.
 * @param polarity Trigger line polarity.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_trigline_polarity(AlliedCameraHandle_t handle, VmbBool_t inverted);

/**
 * @brief Get the trigger line debounce mode.
 * NOTE: The trigger line must be in "Input" mode to get the debounce mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param time Pointer to store the trigger line debounce mode.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_debounce_mode(AlliedCameraHandle_t handle, char **_Nonnull mode);

/**
 * @brief Set the trigger line debounce mode.
 * NOTE: The trigger line must be in "Input" mode to set the debounce mode.
 *
 * @param handle Handle to Allied Vision camera.
 * @param time Trigger line debounce mode.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_trigline_debounce_mode(AlliedCameraHandle_t handle, const char *_Nonnull mode);

/**
 * @brief Get the list of available trigger line debounce modes. User has to free the memory allocated for the set using the {@allied_free_list} function.
 * NOTE: The trigger line must be in "Input" mode to get the debounce mode list.
 *
 * @param handle Handle to Allied Vision camera.
 * @param modes Pointer to store the list of trigger line debounce mode strings. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the trigger line debounce mode is available. Pass NULL if not required.
 * @param count Pointer to store the number of trigger line debounce modes.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_debounce_mode_list(AlliedCameraHandle_t handle, char *_Nonnull *_Const *modes, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the trigger line debounce time.
 * NOTE: The trigger line must be in "Input" mode to get the debounce time.
 *
 * @param handle Handle to Allied Vision camera.
 * @param time Pointer to store the trigger line debounce time.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_debounce_time(AlliedCameraHandle_t handle, double *_Nonnull time);

/**
 * @brief Set the trigger line debounce time.
 * NOTE: The trigger line must be in "Input" mode to set the debounce time.
 *
 * @param handle Handle to Allied Vision camera.
 * @param time Trigger line debounce time.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_trigline_debounce_time(AlliedCameraHandle_t handle, double time);

/**
 * @brief Get the trigger line debounce time range.
 * NOTE: The trigger line must be in "Input" mode to get the debounce time range.
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum trigger line debounce time.
 * @param maxval Pointer to store the maximum trigger line debounce time.
 * @param step Pointer to store the trigger line debounce time step size.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_trigline_debounce_time_range(AlliedCameraHandle_t handle, double *_Nonnull minval, double *_Nonnull maxval, double *_Nullable step);

/**
 * @brief Get the camera ID string.
 *
 * @param handle Handle to Allied Vision camera.
 * @param id Pointer to store the camera ID string. The memory is allocated by the function and must be freed by the caller.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_camera_id(AlliedCameraHandle_t handle, char **_Nonnull id);

/**
 * @brief Get the camera link speed (MB/s).
 *
 * @param handle Handle to Allied Vision camera.
 * @param speed Pointer to store the camera link speed.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_link_speed(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull speed);

/**
 * @brief Get the camera link speed range (MB/s).
 *
 * @param handle Handle to Allied Vision camera.
 * @param minval Pointer to store the minimum camera link speed.
 * @param maxval Pointer to store the maximum camera link speed.
 * @param step Pointer to store the camera link speed step size. Can be NULL.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_throughput_limit_range(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull minval, VmbInt64_t *_Nonnull maxval, VmbInt64_t *_Nullable step);

/**
 * @brief Get the camera link throughput limit (MB/s).
 *
 * @param handle Handle to Allied Vision camera.
 * @param limit Pointer to store the camera link throughput limit.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_throughput_limit(AlliedCameraHandle_t handle, VmbInt64_t *_Nonnull limit);

/**
 * @brief Set the camera link throughput limit (MB/s).
 *
 * @param handle Handle to Allied Vision camera.
 * @param limit Camera link throughput limit.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_throughput_limit(AlliedCameraHandle_t handle, VmbInt64_t limit);

/**
 * @brief Get a list of avaliable features and associated information. User has to free the memory allocated for the list.
 *
 * @param handle Handle to Allied Vision camera.
 * @param features Pointer to store the list of feature info.
 * @param count Number of features.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_features_list(AlliedCameraHandle_t handle, VmbFeatureInfo_t **_Nonnull features, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the feature information for a given feature name.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param info Pointer to store the feature information.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_info(AlliedCameraHandle_t handle, const char *_Nonnull name, VmbFeatureInfo_t *_Nonnull info);

/**
 * @brief Get the integer value of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param value Pointer to store the feature value.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_int(AlliedCameraHandle_t handle, const char *_Nonnull name, VmbInt64_t *_Nonnull value);

/**
 * @brief Set the integer value of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param value Feature value.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_feature_int(AlliedCameraHandle_t handle, const char *_Nonnull name, VmbInt64_t value);

/**
 * @brief Get the integer value range of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param minval Pointer to store the minimum feature value.
 * @param maxval Pointer to store the maximum feature value.
 * @param step Pointer to store the feature value step size.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_int_range(AlliedCameraHandle_t handle, const char *_Nonnull name, VmbInt64_t *_Nonnull minval, VmbInt64_t *_Nonnull maxval, VmbInt64_t *_Nullable step);

/**
 * @brief Get the set of integer values of a feature. User has to free the memory allocated for the set.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param buffer Pointer to store the feature value set.
 * @param count Pointer to store the number of feature values.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_int_valset(AlliedCameraHandle_t handle, const char *_Nonnull name, VmbInt64_t **_Nonnull buffer, VmbUint32_t *_Nonnull count);

/**
 * @brief Get the float value of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param value Pointer to store the feature value.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_float(AlliedCameraHandle_t handle, const char *_Nonnull name, double *_Nonnull value);

/**
 * @brief Set the float value of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param value Feature value.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_feature_float(AlliedCameraHandle_t handle, const char *_Nonnull name, double value);

/**
 * @brief Get the float value range of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param minval Pointer to store the minimum feature value.
 * @param maxval Pointer to store the maximum feature value.
 * @param step Pointer to store the feature value step size. This pointer can be NULL.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_float_range(AlliedCameraHandle_t handle, const char *_Nonnull name, double *_Nonnull minval, double *_Nonnull maxval, double *_Nullable step);

/**
 * @brief Get the current string value of a feature. The user does not own the memory allocated for the string, and must not free it.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param value Pointer to store the feature value string. The user must not free this memory.
 * @return VmbError_t
 */
VmbError_t allied_get_feature_enum(AlliedCameraHandle_t handle, const char *_Nonnull name, char **_Nonnull const value);

/**
 * @brief Set the string value of a feature.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Feature name.
 * @param value Feature value string.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_set_feature_enum(AlliedCameraHandle_t handle, const char *_Nonnull name, const char *_Nonnull value);

/**
 * @brief Get the set of valid string values of a feature. User has to free the memory allocated for the set using the {@allied_free_list} function.
 *
 * @param handle Handle to Allied Vision camera.
 * @param name Name of the feature.
 * @param list Pointer to store the list of feature values. This is a pointer to an array of `const char *`.
 * @param available Pointer to store the list of booleans indicating whether the feature value is available. Pass NULL if not required.
 * @param count Pointer to store the number of feature values.
 * @return VmbError_t `VmbErrorSuccess` if successful, otherwise an error code.
 */
VmbError_t allied_get_feature_enum_list(AlliedCameraHandle_t handle, const char *_Nonnull name, char *_Nonnull *_Const *list, VmbBool_t **_Nullable available, VmbUint32_t *_Nonnull count);

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