#pragma once

#include <fstream>
#include <vector>
#include <algorithm>
#include <vector>
#include <iostream>
#include <stdint.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

void writeMapPPM(float norm, int W, int H, vector<float> &height, float yMin, const string& filename) {
    const float invNorm = 255.0f / norm;
    
    vector<uint8_t> pixels;
    pixels.resize(static_cast<size_t>(W) * static_cast<size_t>(H) * 3u);

    for (int j = 0; j < H; j++) {
        for (int i = 0; i < W; i++) {
            const float h   = height[j * W + i];    // in [yMin, yMax] or background
            float       t   = (h - yMin) * invNorm; // map to [0,255] (float)
            if (t < 0.0f)   t = 0.0f;
            if (t > 255.0f) t = 255.0f;
            const uint8_t v = static_cast<uint8_t>(t + 0.5f);   

            const size_t idx = (static_cast<size_t>(j) * static_cast<size_t>(W) + static_cast<size_t>(i)) * 3u;
            pixels[idx + 0] = v;
            pixels[idx + 1] = v;
            pixels[idx + 2] = v;
        }
    }

    // write PPM 
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output file\n");
        return;
    }

    // header: P6\nW H\n255\n
    fprintf(fp, "P6\n%d %d\n255\n", W, H);
    // pixel data
    const size_t nbytes = pixels.size();
    const size_t wrote  = fwrite(pixels.data(), 1, nbytes, fp);
    if (wrote != nbytes) {
        fprintf(stderr, "Wrote %zu / %zu bytes\n", wrote, nbytes);
    }
    fclose(fp);
    
    cout << "Generated PPM.\n";
}

void writeMapPNG(float norm, int W, int H, const vector<float> &height,
                 float yMin, const string &filename)
{
    vector<uint8_t> pixels(W * H * 3);
    const float invNorm = 255.0f / norm;

    for (int j = 0; j < H; ++j) {
        for (int i = 0; i < W; ++i) {
            const float h = height[j * W + i];
            float t = (h - yMin) * invNorm;
            t = clamp(t, 0.0f, 255.0f);
            const uint8_t v = static_cast<uint8_t>(t + 0.5f);

            const size_t idx = (j * W + i) * 3;
            pixels[idx + 0] = v;
            pixels[idx + 1] = v;
            pixels[idx + 2] = v;
        }
    }

    // flip vertically if you want top-left origin
    stbi_write_png(filename.c_str(), W, H, 3, pixels.data(), W * 3);
    cout << "Generated PNG: " << filename << endl;
}