#include <stdio.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}__attribute__((packed, aligned(1))) pixel;

typedef struct
{
    int r, g, b;
}color_data_t;

#define THREAD_COUNT 4000

typedef pixel (*get_color_func_t)(int *fixed_data, void *custom_data);

typedef struct
{
    double max_x, min_x, max_y, min_y;
    int screen_width, screen_height;
    int screen_height_stop, screen_height_start;
    int max_iter;
    // can be zero
    void *custom_data;
    get_color_func_t get_color_func;
}mand_parameters_t;

typedef struct
{
    mand_parameters_t p;
    pixel *buf;
}thread_data_t;

void *calc_mandelbrot(void *data)
{
    mand_parameters_t *p = &((thread_data_t *)data)->p;
    for (int screen_x = 0; screen_x < p->screen_width; screen_x++)
    {
        double real = p->min_x + ((p->max_x - p->min_x) / (double)(p->screen_width  - 1) * screen_x);
        
        for (int screen_y = p->screen_height_start; screen_y < p->screen_height_stop; screen_y++)
        {
            double imag = p->max_y - ((p->max_y - p->min_y) / (double)(p->screen_height - 1) * screen_y);
            
            double z_real = 0.0f;
            double z_imag = 0.0f;

            int current_iter = 0;

            while (current_iter < p->max_iter && z_real * z_real + z_imag * z_imag <= 4.0)
            {
                // (a+bi) * (c+di) = ac - bd + (ad + bc) * i
                double z_real_temp = z_real * z_real - z_imag * z_imag + real;
                z_imag = 2 * z_imag * z_real + imag; 

                z_real = z_real_temp;

                current_iter++;
            }

            int fixed_data[2] = {current_iter, p->max_iter};
            pixel pix = p->get_color_func(fixed_data, p->custom_data);

            ((thread_data_t *)data)->buf[screen_y * p->screen_width + screen_x] = pix;
        }
    }
    
    return NULL;
}

pixel *get_mand_buf(mand_parameters_t *param)
{
    // if THREAD_COUNT is higher than screen_height -> segfault/memory corruption
    pixel *mand_buf = (pixel *)malloc(param->screen_width * param->screen_height * sizeof(pixel));
    
    pthread_t threads[THREAD_COUNT];
    thread_data_t thread_data [THREAD_COUNT];

    // divide up on the y axis
    /*
                 ----------------
    thread 0--> |                |
                |                |
    thread 1--> |    mand_buf    |
                |                |
    thread 2--> |                |
                 ----------------
    ...
    */

    int curr_y = 0;
    int step_y = param->screen_height / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        mand_parameters_t curr_param = *param;

        curr_param.screen_height_start = i * step_y;
        curr_param.screen_height_stop  = (i == THREAD_COUNT - 1) ? param->screen_height : i * step_y + step_y;

        curr_y += step_y;

        thread_data_t curr_data = 
        {
            .p = curr_param,
            .buf = mand_buf,
        };

        thread_data[i] = curr_data;
        pthread_create(&threads[i], NULL, calc_mandelbrot, &thread_data[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        pthread_join(threads[i], NULL);
    }

    return mand_buf;
}

// pixel get_color(int *fixed_data, void *custom_data)
// {
//     pixel pix;

//     if (fixed_data[0] == fixed_data[1])
//     {
//         pix = (pixel){255, 255, 255};
//     }else
//     {
//         pix = (pixel){0, 0, 0};
//     }

//     return pix;
// }

pixel get_color(int *fixed_data, void *custom_data)
{
    int iter = fixed_data[0];
    int max_iter = fixed_data[1];

    pixel pix;

    if(iter >= max_iter) {
        pix.r = 0;
        pix.g = 0;
        pix.b = 0;
    } else {
        float t = (float)iter / (float)max_iter;

        pix.r = (uint8_t)(9*(1-t)*t*t*t*255);
        pix.g = (uint8_t)(15*(1-t)*(1-t)*t*t*255);
        pix.b = (uint8_t)(8.5*(1-t)*(1-t)*(1-t)*t*255);
    }

    return pix;
}

typedef enum
{
    ITER, R, G, B,
} change_state;

#define MAX_ITER_START 10000

#define WIDTH 7680
#define HEIGHT 4320

#define FILENAME "mandelbrot.png"

int main()
{
    mand_parameters_t mand_parameters = 
    {
        .max_x = 1.3,
        .min_x = -2,
        .max_y = 1.2,
        .min_y = -1.2,

        .screen_width = WIDTH,
        .screen_height = HEIGHT,

        .max_iter = MAX_ITER_START,

        .get_color_func = get_color,
        .custom_data = NULL
    };

    pixel *mand_buf = get_mand_buf(&mand_parameters);
    
    Image img = {
        .data = mand_buf,
        .width = WIDTH,
        .height = HEIGHT,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8
    };

    ExportImage(img, FILENAME);

    free(mand_buf);

    return 0;
}