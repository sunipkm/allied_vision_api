# Allied Vision Camera Simplified API
This repository is a thin wrapper around the [VimbaX SDK](https://www.alliedvision.com/en/products/software/vimba-x-sdk/), and breaks out some of the features available in the cameras to individual functions for ease of programming.

## Testing
1. Clone this repository, and go into the cloned directory:
```sh
$ git clone https://github.com/sunipkm/allied_vision_api alliedcam
$ cd alliedcam
```
2. Install the USB and/or GigE backend:
```sh
$ cd cti
$ sudo ./VimbaUSBTL_Install.sh
$ sudo ./VimbaGigETL_Install.sh
$ cd ..
```
3. Execute `make` to build and run the test executable.

## Installation
Note that the `Makefile` appends the `lib` directory inside the repository to `LD_LIBRARY_PATH` environment variable. The installation of the backend also sets an environment variable to the location of the `cti` directory inside the repository. In order to link and run other programs, care must be taken in this regard.

## Minimal Example
```c
#include <alliedcam.h>

void Callback(const AlliedCameraHandle_t handle, const VmbHandle_t stream, VmbFrame_t *frame, void *user_data)
{
    // do stuff with the frame and user data here
}

int main()
{
    VmbError_t err;
    err = allied_init_api(NULL); // start the API
    if (err != VmbErrorSuccess)
    {
        // do error handling
    }
    AlliedCameraHandle_t handle;
    err = allied_open_camera(&handle, NULL, 2); // open the first available camera with two frames for framebuffer
    if (err != VmbErrorSuccess)
    {
        // do error handling
    }
    err = allied_start_capture(handle, &Callback, NULL); // start capturing, pass in the capture callback and any user data
    if (err != VmbErrorSuccess)
    {
        // do error handling
    }
    // wait or do other things while images are captured

    err = allied_stop_capture(handle); // stop capturing
    if (err != VmbErrorSuccess)
    {
        // do error handling
    }
    err = allied_close_camera(&handle); // close the camera and free all associated memory
    if (err != VmbErrorSuccess)
    {
        // do error handling
    }
    return 0;
}
```