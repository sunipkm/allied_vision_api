#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>

#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <math.h>

#include <alliedcam.h>

#define MS_TO_US(ms) ((ms) * 1000)                    // ms to us
#define US_TO_SECS(us) ((us) / 1000000.0)             // us to ms
#define CYCLE_TIME MS_TO_US(1000)                     // 100 ms
#define NUM_CYCLES 10                                 // 100 cycles
#define TOTAL_SECS US_TO_SECS(CYCLE_TIME *NUM_CYCLES) // 10 s

#define error_message(fmt, ...) \
    printf(fmt "\n", ##__VA_ARGS__)

// serial functions
int set_interface_attribs(int fd, int speed, int parity);
void set_blocking(int fd, int should_block);

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
    int fd;
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
    if (stat->fd > STDERR_FILENO)
    {
        write(stat->fd, "1", 1);
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

static void ReadAndPrintTemperatures(const AlliedCameraHandle_t handle, const char **srcs, VmbUint32_t count)
{
    VmbError_t err;
    double temp;
    VmbUint32_t i;
    printf("Temperatures: ");
    for (i = 0; i < count; i++)
    {
        err = allied_set_temperature_src(handle, srcs[i]);
        if (err != VmbErrorSuccess)
        {
            temp = -280;
            goto print;
        }
        err = allied_get_temperature(handle, &temp);
        if (err != VmbErrorSuccess)
        {
            temp = -280;
        }
    print:
        printf("%.2lf C [%s]\t", temp, srcs[i]);
    }
    fflush(stdout);
    printf("\r");
}

int main(int argc, char *argv[])
{
    VmbCameraInfo_t *cameras;
    VmbUint32_t count;
    VmbError_t err;
    AlliedCameraHandle_t handle;
    VmbUint32_t i;

    frame_stat_t stat;

    stat.fd = -1;
    char *serdev = "/dev/ttyACM1";
    if (argc == 2)
        serdev = argv[1];
    int ser = open(serdev, O_RDWR | O_NOCTTY | O_SYNC);
    if (ser < 0)
    {
        printf("Error opening serial port\n");
    } else {
        printf("Opened serial port: %d\n", ser);
        set_interface_attribs(ser, B921600, 0);
        set_blocking(ser, 0);
        stat.fd = ser;
    }

    double exposure;
    double framerate;
    double fps_min, fps_max, fps_step;
    VmbInt64_t width, height, swidth, sheight;
    char *camera_id;
    char **fmt;
    VmbBool_t *avail;

    err = allied_init_api(NULL);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error initializing API: %d\n", err);
        return EXIT_FAILURE;
    }

    err = allied_list_cameras(&cameras, &count);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error listing cameras: %d\n", err);
        return EXIT_FAILURE;
    }

    printf("Found %d cameras\n", count);
    for (i = 0; i < count; i++)
    {
        printf("Available camera %d: %s\n", i, cameras[i].cameraIdString);
    }

    free(cameras);

    err = allied_open_camera(&handle, NULL, 2);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error opening camera: %d\n", err);
        return EXIT_FAILURE;
    }

    err = allied_get_camera_id(handle, &camera_id);

    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting camera ID: %d\n", err);
        goto cleanup;
    }
    else
    {
        printf("Opened: %s\n", camera_id);
    }
    free((void *)camera_id);

    err = allied_get_temperature_src_list(handle, &fmt, NULL, &count);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting temperature source list: %d\n", err);
        goto cleanup;
    }
    else
    {
        printf("Available temperature sources:\n");
        for (i = 0; i < count; i++)
        {
            printf("%s\n", fmt[i]);
        }
    }

    err = allied_get_sensor_size(handle, &swidth, &sheight);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting sensor size: %d\n", err);
        goto cleanup;
    }
    else
    {
        printf("Sensor size: %lld x %lld\n", swidth, sheight);
    }

    err = allied_set_sensor_bit_depth(handle, "Mono12");
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting sensor bit depth: %d\n", err);
    }
    else
    {
        printf("Sensor bit depth set to Mono12\n");
    }

    err = allied_get_sensor_bit_depth(handle, &camera_id);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting sensor bit depth: %d\n", err);
    }
    else
    {
        printf("Sensor bit depth: %s\n", camera_id);
    }

    err = allied_set_image_size(handle, 128, 128);
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

    err = allied_set_exposure_us(handle, 100);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting exposure: %d\n", err);
    }
    else
    {
        err = allied_get_exposure_us(handle, &exposure);
        printf("Exposure set to %lf us\n", exposure);
    }

    err = allied_set_indicator_luma(handle, 0);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting indicator luma: %d\n", err);
    }

    err = allied_set_trigline(handle, "Line0");
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting trigger line: %d\n", err);
    }
    else
    {
        err = allied_set_trigline_mode(handle, "Output");
        if (err != VmbErrorSuccess)
        {
            fprintf(stderr, "Error setting trigger line mode: %d\n", err);
        }
        else
        {
            err = allied_set_trigline_src(handle, "ExposureActive");
            if (err != VmbErrorSuccess)
            {
                fprintf(stderr, "Error setting trigger line source: %d\n", err);
            }
            else
            {
                printf("Trigger line set to Line0: ExposureActive\n");
            }
        }
    }

    printf("\n\nPress Enter to start capture\n");
    getchar();

    err = allied_start_capture(handle, &Callback, (void *)&stat);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error starting capture: %s\n", allied_strerr(err));
        goto cleanup;
    }

    printf("Capturing for %.2lf s\n", TOTAL_SECS);

    for (int i = NUM_CYCLES; i > 0; i--)
    {
        ReadAndPrintTemperatures(handle, (const char **)fmt, count);
        usleep(CYCLE_TIME);
    }

    printf("\n");

    allied_free_list(&fmt);
    // free(avail);

    err = allied_stop_capture(handle);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error stopping capture: %d\n", err);
        goto cleanup;
    }

    printf("Captured %d frames\n", stat.n);
    printf("Average frame time: %lf us (%.5lf FPS)\n", stat.avg * 1e6, 1.0 / stat.avg);
    printf("Frame time std: %lf us\n", sqrt(stat.avg2 - stat.avg * stat.avg) * 1e6);

    err = allied_set_indicator_luma(handle, 10);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error setting indicator luma: %d\n", err);
    }

    err = allied_get_image_format_list(handle, &fmt, &avail, &count);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error getting image format list: %d\n", err);
        goto cleanup;
    }
    else
    {
        printf("Available image formats:\n");
        for (i = 0; i < count; i++)
        {
            printf("%s: %s\n", fmt[i], avail[i] ? "available" : "not available");
        }
    }
    allied_free_list(&fmt);
    free(avail);

    err = allied_stop_capture(handle);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error stopping capture: %d\n", err);
        goto cleanup;
    }

cleanup:
    err = allied_close_camera(&handle);
    if (err != VmbErrorSuccess)
    {
        fprintf(stderr, "Error closing camera: %d\n", err);
        return EXIT_FAILURE;
    }
    printf("Closed camera: %p\n", handle);
    if (ser > STDERR_FILENO)
    {
        close(ser);
    }
    return EXIT_SUCCESS;
}

int set_interface_attribs(int fd, int speed, int parity)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0)
    {
        error_message("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK; // disable break processing
    tty.c_lflag = 0;        // no signaling chars, no echo,
                            // no canonical processing
    tty.c_oflag = 0;        // no remapping, no delays
    tty.c_cc[VMIN] = 0;     // read doesn't block
    tty.c_cc[VTIME] = 5;    // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                       // enable reading
    tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        error_message("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}

void set_blocking(int fd, int should_block)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0)
    {
        error_message("error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN] = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
        error_message("error %d setting term attributes", errno);
}
