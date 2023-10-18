#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include <alliedcam.h>

static inline void timespec_diff(struct timespec *start, struct timespec *end, struct timespec *diff)
{
    assert(start);
    assert(end);
    assert(diff);
    diff->tv_sec = end->tv_sec - start->tv_sec;
    diff->tv_nsec = end->tv_nsec - start->tv_nsec;
    if (diff->tv_nsec < 0)
    {
        diff->tv_sec--;
        diff->tv_nsec += 1000000000;
    }
}

static inline double timespec_to_double(struct timespec *ts)
{
    assert(ts);
    return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

static inline double rolling_average(double avg, double new, uint32_t n)
{
    return (n * avg + new) / (n + 1);
}

static inline double rolling_avg2(double avg2, double new, uint32_t n)
{
    return (n * avg2 + new *new) / (n + 1);
}

typedef struct
{
    double avg;
    double avg2;
    uint32_t n;
} frame_stat_t;

static void Callback(const AlliedCameraHandle_t handle, const VmbHandle_t stream, VmbFrame_t *frame, void *user_data)
{
    static struct timespec start, end, diff;
    static bool first_run = true;
    frame_stat_t *stat = (frame_stat_t *)user_data;
    if (first_run)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        first_run = false;
        stat->n = 0;
        stat->avg2 = 0;
        stat->avg = 0;
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    timespec_diff(&start, &end, &diff);
    start = end;
    double frame_time = timespec_to_double(&diff);
    stat->avg = rolling_average(stat->avg, frame_time, stat->n);
    stat->avg2 = rolling_avg2(stat->avg2, frame_time, stat->n);
    stat->n++;
    return;
}

int main()
{
    VmbCameraInfo_t *cameras;
    VmbUint32_t count;
    VmbError_t err;
    AlliedCameraHandle_t handle;
    VmbUint32_t i;

    frame_stat_t stat;

    double exposure;
    double framerate;
    double fps_min, fps_max, fps_step;
    VmbInt64_t width, height;

    err = allied_list_cameras(&cameras, &count);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error listing cameras: %d\n", err);
        return EXIT_FAILURE;
    }

    printf("Found %d cameras\n", count);
    for (i = 0; i < count; i++)
    {
        printf("Camera %d: %s\n", i, cameras[i].cameraIdString);
    }

    free(cameras);

    err = allied_open_camera(&handle, NULL);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error opening camera: %d\n", err);
        return EXIT_FAILURE;
    }

    err = allied_alloc_framebuffer(handle, 2);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error allocating frame buffer: %d\n", err);
        goto cleanup;
    }

    err = allied_set_image_size(handle, 64, 64);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting image size: %d\n", err);
        goto cleanup;
    }
    err = allied_get_image_size(handle, &width, &height);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting image size: %d\n", err);
        goto cleanup;
    }
    else
    {
        printf("Image size: %lld x %lld\n", width, height);
    }

    err = allied_get_acq_framerate(handle, &framerate);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting framerate: %d\n", err);
    }
    else
    {
        printf("Framerate: %.5lf fps\n", framerate);
    }

    err = allied_get_acq_framerate_range(handle, &fps_min, &fps_max, &fps_step);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting framerate range: %d\n", err);
    }
    else
    {
        printf("Framerate range: [%.5lf, %.5lf] fps, increment: %.5lf fps\n", fps_min, fps_max, fps_step);
    }

    err = allied_get_exposure_us(handle, &exposure);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting exposure: %d\n", err);
    }
    else
    {
        printf("Exposure: %lf us\n", exposure);
    }

    err = allied_set_exposure_us(handle, 10000);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting exposure: %d\n", err);
    }
    else
    {
        err = allied_get_exposure_us(handle, &exposure);
        printf("Exposure set to %lf us\n", exposure);
    }

    err = allied_start_capture(handle, &Callback, (void *)&stat);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error starting capture: %s\n", allied_strerr(err));
        goto cleanup;
    }

    sleep(2);

    err = allied_stop_capture(handle);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error stopping capture: %d\n", err);
        goto cleanup;
    }

    printf("Captured %d frames\n", stat.n);
    printf("Average frame time: %lf us\n", stat.avg * 1e6);
    printf("Frame time std: %lf us\n", sqrt(stat.avg2 - stat.avg * stat.avg) * 1e6);

cleanup:

    err = allied_close_camera(handle);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error closing camera: %d\n", err);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}