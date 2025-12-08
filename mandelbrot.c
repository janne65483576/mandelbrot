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

typedef pixel (*get_color_func_t)(int *fixed_data, void *custom_data);

typedef struct
{
    double max_x, min_x, max_y, min_y;
    int screen_width, screen_height;
    int max_iter;
    // can be zero
    void *custom_data;
    get_color_func_t get_color_func;
}mand_parameters_t;

typedef struct
{
    mand_parameters_t p;
    pixel *buf;
    int start_y, end_y;
}thread_data_t;

void *calc_mandelbrot(void *data)
{
    mand_parameters_t *p = &((thread_data_t *)data)->p;
    for (int screen_x = 0; screen_x < p->screen_width; screen_x++)
    {
        double real = p->min_x + ((p->max_x - p->min_x) / (double)(p->screen_width  - 1) * screen_x);
        
        for (int screen_y = ((thread_data_t *)data)->start_y; screen_y < ((thread_data_t *)data)->end_y; screen_y++)
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

Texture2D get_mand_tex(mand_parameters_t *param, int thread_count)
{
    if (param->screen_height < thread_count)
    {
        thread_count = param->screen_height;
        printf("reduce thread count to %d\n", param->screen_height);
    }

    printf("Running on %d threads\n", thread_count);

    pixel *mand_buf = (pixel *)malloc(param->screen_width * param->screen_height * sizeof(pixel));
    
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    thread_data_t *thread_data = (thread_data_t *)malloc(sizeof(thread_data_t) * thread_count);

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
    int step_y = param->screen_height / thread_count;

    for (int i = 0; i < thread_count; i++)
    {
        mand_parameters_t curr_param = *param;

        thread_data_t curr_data = 
        {
            .p = curr_param,
            .buf = mand_buf,
        };

        curr_data.start_y = curr_y;
        curr_data.end_y   = curr_y + step_y <= param->screen_height ? curr_y + step_y : param->screen_height;

        curr_y += step_y;

        thread_data[i] = curr_data;
        pthread_create(&threads[i], NULL, calc_mandelbrot, &thread_data[i]);
    }

    for (int i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(thread_data);
    free(threads);

    // create texture
    Image img = {
        .data = (void *)mand_buf,
        .width = param->screen_width,
        .height = param->screen_height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8
    };
    
    Texture2D tex = LoadTextureFromImage(img);

    UnloadImage(img);
    return tex;
}

// pixel get_color(int *fixed_data, void *custom_data)
// {
//     pixel pix;
    
//     if (fixed_data[0] == fixed_data[1])
//     {
//         pix = (pixel){255, 255, 255};
//     }else
//     {
//         pix = (pixel)
//         {
//             .r = (((Color *)custom_data)->r % 255) / (float)fixed_data[1] * (float)fixed_data[0],
//             .g = (((Color *)custom_data)->g % 255) / (float)fixed_data[1] * (float)fixed_data[0],
//             .b = (((Color *)custom_data)->b % 255) / (float)fixed_data[1] * (float)fixed_data[0],
//         };
//     }
    
//     return pix;
// }

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

#define MAX_ITER_START 400
#define THREAD_COUNT 50

int main()
{
    
    int screen_width  = 800;
    int screen_height = 400;

    int values[] = {MAX_ITER_START, 100, 100, 100};
    char *value_content[] = {"maximal iterations %d", "maximal red %d", "maximal green %d", "maximal blue %d"};
    change_state state = ITER;

    color_data_t color_data = {values[R], values[G], values[B]};

    mand_parameters_t mand_parameters = 
    {
        .max_x = 1.3,
        .min_x = -2,
        .max_y = 1.2,
        .min_y = -1.2,

        // .max_x = 0.5,
        // .min_x = -0.5,
        // .max_y = 0.5,
        // .min_y = -0.5,

        .screen_width = screen_width,
        .screen_height = screen_height,

        .max_iter = values[ITER],

        .get_color_func = get_color,
        .custom_data = (void *)&color_data
    };

    Texture2D mand_tex;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, "Mandelbrot");
    SetTargetFPS(60);

    while(!WindowShouldClose())
    {
        screen_height = GetScreenHeight();
        screen_width = GetScreenWidth();

        if(IsKeyPressed(KEY_LEFT))
        {
            state = (state + 3) % 4;
        }
        
        if(IsKeyPressed(KEY_RIGHT))
        {
            state = (state + 1) % 4;
        }

        if(IsKeyDown(KEY_UP))
        {
            values[state] += 1;
        }
        
        if(IsKeyDown(KEY_DOWN))
        {
            values[state] -= 1;
            if (values[state] < 0)
            {
                values[state] = 0;
            }
        }

        if(IsKeyPressed(KEY_Z))
        {
            mand_parameters.max_x -= 0.1;
            mand_parameters.max_y -= 0.1;
            mand_parameters.min_x += 0.1;
            mand_parameters.min_y += 0.1;
        }

        if(IsKeyPressed(KEY_U))
        {
            mand_parameters.max_x += 0.1;
            mand_parameters.max_y += 0.1;
            mand_parameters.min_x -= 0.1;
            mand_parameters.min_y -= 0.1;
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            if(IsKeyPressed(KEY_D))
            {
                UnloadTexture(mand_tex);
                // update parameters
                mand_parameters.max_iter = values[ITER];
                mand_parameters.screen_width = screen_width;
                mand_parameters.screen_height = screen_height;
                color_data.r = values[R];
                color_data.g = values[G];
                color_data.b = values[B];

                mand_tex = get_mand_tex(&mand_parameters, THREAD_COUNT);
            }

            DrawTexture(mand_tex, 0, 0, WHITE);

            DrawText(TextFormat(value_content[state], values[state]), 20, 20, 20, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}