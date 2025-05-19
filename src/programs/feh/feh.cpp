// I am aware that this code is ugly, especially the syscall interface used, but I just wanted to have a way to display
// images on the screen as it's cool, but this doesn't align with the way the framebuffer works (with characters),
// so I had to find ugly workarounds.

// Also, this is painfully slow, especially downsizing. I don't understand why. Maybe SIMD support will make it run at a
// decent speed.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ksyscalls.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize2.h"

enum class ERROR_TYPE : int
{
    NO_ERROR,
    FILE_LOAD,
    FILE_FORMAT,
    FILE_SANITY
};

/**
 * Make dimensions fit into the screen by keeping the aspect ratio
 * @param x input width
 * @param y input height
 * @param new_x OUTPUT - resized width
 * @param new_y OUTPUT - resized height
 */
void size_screen_fit(int x, int y, int* new_x, int* new_y)
{
    // Get screen dimensions
    unsigned int SCREEN_WIDTH = 0, SCREEN_HEIGHT = 0;
    get_screen_dimensions(&SCREEN_WIDTH, &SCREEN_HEIGHT);

    // Compute resized dimensions
    double x_ratio = (double)x / SCREEN_WIDTH;
    double y_ratio = (double)y / SCREEN_HEIGHT;
    if ((uint)x > SCREEN_WIDTH)
    {
        if ((uint)y > SCREEN_HEIGHT)
        {
            if (x_ratio > y_ratio)
            {
                *new_x = SCREEN_WIDTH;
                *new_y = y / x_ratio;
            }
            else
            {
                *new_x = x / y_ratio;
                *new_y = SCREEN_HEIGHT;
            }
        }
        else
        {
            *new_x = SCREEN_WIDTH;
            *new_y = y / x_ratio;
        }
    }
    else if ((uint)y > SCREEN_HEIGHT)
    {
        *new_x = x * (double)SCREEN_HEIGHT / y;
        *new_y = SCREEN_HEIGHT;
    }
    else
    {
        *new_x = x;
        *new_y = y;
    }
}

ERROR_TYPE parse_and_display_image(unsigned char* raw_img, uint size)
{
    printf("Read image metadata...\n");
    int x, y, n;
    if (!stbi_info_from_memory(raw_img, size, &x, &y, &n))
        return ERROR_TYPE::FILE_FORMAT;

    printf("Decoding image...\n");
    auto data = stbi_load_from_memory(raw_img, size, &x, &y, &n, 0);
    if (!data)
    {
        fprintf(stderr, "%s", stbi_failure_reason());
        return ERROR_TYPE::FILE_SANITY;
    }

    int new_x, new_y;
    size_screen_fit(x, y, &new_x, &new_y);

    if (new_x != x || new_y != y)
    {
        printf("Downsizing image...\n");
        unsigned char* res = stbir_resize_uint8_linear(
        data, x, y, 0,  nullptr, new_x, new_y,
        0, (stbir_pixel_layout)n);
        stbi_image_free(data);
        data = res;
        if (!res)
            return ERROR_TYPE::FILE_SANITY;
    }

    lock_framebuffer_flush();
    __asm__ volatile("int $0x80" : : "a"(21), "D"(data), "S"(new_x), "d"(new_y)); // Display image
    unlock_framebuffer_flush();

    free(data);

    // Clear screen
    pid_t child_pid = fork();
    if (child_pid == -1)
    {
        fprintf(stderr, "Fork failed\n");
        _exit(1);
    }
    if (child_pid == 0)
    {
        const char* argv[] = {"cls", nullptr};
        if (execve(argv[0], (char**)argv, nullptr) == -1)
        {
            fprintf(stderr, "Exec failed\n");
            _exit(1);
        }

        __builtin_unreachable();
    }
    [[maybe_unused]] int wstatus = 0;
    if (waitpid(child_pid, &wstatus, 0) == -1)
    {
        fprintf(stderr, "Waitpid failed\n");
        _exit(1);
    }

    return ERROR_TYPE::NO_ERROR;
}

void show_usage()
{
    fprintf(stderr, "Usage: feh <image_path>\n");
    fprintf(stderr, "Display an image on the screen.\n");
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        show_usage();
        return 1;
    }

    char* buf = NULL;
    uint size = get_file_size(argv[1]);
    printf("Loading image...\n");
    if (size != (uint)-1)
        buf = (char*)malloc(size);

    if (!buf || !load_file(argv[1], buf))
    {
        fprintf(stderr, "Failed to load image\n");
        return (int)ERROR_TYPE::FILE_LOAD;
    }

    ERROR_TYPE e = parse_and_display_image((unsigned char*)buf, size);
    if (e == ERROR_TYPE::FILE_FORMAT || e == ERROR_TYPE::FILE_SANITY)
        fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());

    return (int)e;
}