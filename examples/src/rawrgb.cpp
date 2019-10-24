#include <stdint.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <chrono>
#include <thread>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include "headers/MLX90640_API.h"

/*
 * This is a modification of the pimoroni rawrgb example where in place of streaming image bytes we just stream the float value bytes directly.
 *
 */


#define MLX_I2C_ADDR 0x33

#define IMAGE_SCALE 5

// Valid frame rates are 1, 2, 4, 8, 16, 32 and 64
// The i2c baudrate is set to 1mhz to support these
#define FPS 16
#define FRAME_TIME_MICROS (1000000/FPS)

// Despite the framerate being ostensibly FPS hz
// The frame is often not ready in time
// This offset is added to the FRAME_TIME_MICROS
// to account for this.
#define OFFSET_MICROS 850

#define PIXEL_SIZE_BYTES 3
#define IMAGE_SIZE 768*PIXEL_SIZE_BYTES

void put_pixel_false_colour(char *image, int x, int y, double v) {
    // Heatmap code borrowed from: http://www.andrewnoske.com/wiki/Code_-_heatmaps_and_color_gradients
    const int NUM_COLORS = 7;
    static float color[NUM_COLORS][3] = { {0,0,0}, {0,0,1}, {0,1,0}, {1,1,0}, {1,0,0}, {1,0,1}, {1,1,1} };
    int idx1, idx2;
    float fractBetween = 0;
    float vmin = 5.0;
    float vmax = 50.0;
    float vrange = vmax-vmin;
    int offset = (y*32+x) * PIXEL_SIZE_BYTES;

    v -= vmin;
    v /= vrange;
    if(v <= 0) {idx1=idx2=0;}
    else if(v >= 1) {idx1=idx2=NUM_COLORS-1;}
    else
    {
        v *= (NUM_COLORS-1);
        idx1 = floor(v);
        idx2 = idx1+1;
        fractBetween = v - float(idx1);
    }

    int ir, ig, ib;


    ir = (int)((((color[idx2][0] - color[idx1][0]) * fractBetween) + color[idx1][0]) * 255.0);
    ig = (int)((((color[idx2][1] - color[idx1][1]) * fractBetween) + color[idx1][1]) * 255.0);
    ib = (int)((((color[idx2][2] - color[idx1][2]) * fractBetween) + color[idx1][2]) * 255.0);

    //put calculated RGB values into image map
    image[offset] = ir;
    image[offset + 1] = ig;
    image[offset + 2] = ib;


}

int main(int argc, char *argv[]){

    static uint16_t eeMLX90640[832];
    float emissivity = 0.8;
    uint16_t frame[834];
    static char image[IMAGE_SIZE];
    static float mlx90640To[768];
    float eTa;
    static uint16_t data[768*sizeof(float)];
    static int fps = FPS;
    static long frame_time_micros = FRAME_TIME_MICROS;
    char *p;

    if(argc > 1){
        fps = strtol(argv[1], &p, 0);
        if (errno !=0 || *p != '\0') {
            fprintf(stderr, "Invalid framerate\n");
            return 1;
        }
        frame_time_micros = 1000000/fps;
    }

    auto frame_time = std::chrono::microseconds(frame_time_micros + OFFSET_MICROS);

    MLX90640_SetDeviceMode(MLX_I2C_ADDR, 0);
    MLX90640_SetSubPageRepeat(MLX_I2C_ADDR, 0);
    switch(fps){
        case 1:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b001);
            break;
        case 2:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b010);
            break;
        case 4:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b011);
            break;
        case 8:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b100);
            break;
        case 16:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b101);
            break;
        case 32:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b110);
            break;
        case 64:
            MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b111);
            break;
        default:
            fprintf(stderr, "Unsupported framerate: %d\n", fps);
            return 1;
    }
    MLX90640_SetChessMode(MLX_I2C_ADDR);

    paramsMLX90640 mlx90640;
    MLX90640_DumpEE(MLX_I2C_ADDR, eeMLX90640);
    MLX90640_SetResolution(MLX_I2C_ADDR, 0x03);
    MLX90640_ExtractParameters(eeMLX90640, &mlx90640);

    while (1){
        auto start = std::chrono::system_clock::now();
        MLX90640_GetFrameData(MLX_I2C_ADDR, frame);
        MLX90640_InterpolateOutliers(frame, eeMLX90640);

        eTa = MLX90640_GetTa(frame, &mlx90640); // Sensor ambient temprature
        MLX90640_CalculateTo(frame, &mlx90640, emissivity, eTa, mlx90640To); //calculate temprature of all pixels, base on emissivity of object

//        //Fill image array with false-colour data (raw RGB image with 24 x 32 x 24bit per pixel)
//        for(int y = 0; y < 24; y++){
//            for(int x = 0; x < 32; x++){
//                float val = mlx90640To[32 * (23-y) + x];
//                put_pixel_false_colour(image, x, y, val);
//            }
//        }

        //wite measurements to stdout
        fwrite(&mlx90640To, sizeof(float), sizeof(mlx90640To), stdout);
        fflush(stdout); // push to stdout now

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::this_thread::sleep_for(std::chrono::microseconds(frame_time - elapsed));
    }

    return 0;
}
