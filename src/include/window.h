#pragma once
#include <SDL3/SDL.h>
#include <vector>

// 自定义像素缓冲区
class PixelBuffer {
private:
    std::vector<Uint32> buffer;
    int width;
    int height;
    
public:
    PixelBuffer(int w, int h) : width(w), height(h) {
        buffer.resize(w * h, 0); // 初始化为黑色
    }
    
    void setPixel(int x, int y, Uint32 color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y * width + x] = color;
        }
    }
    
    void clear(Uint32 color) {
        std::fill(buffer.begin(), buffer.end(), color);
    }
    
    void draw(SDL_Renderer* renderer) {
        // 创建表面
        SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA8888);
        memcpy(surface->pixels, buffer.data(), buffer.size() * sizeof(Uint32));
        
        // 转换为纹理并渲染
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        
        SDL_DestroyTexture(texture);
        SDL_DestroySurface(surface);
    }
};

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_Window* window = SDL_CreateWindow("Pixel Buffer", 800, 600, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    
    PixelBuffer pixelBuffer(800, 600);
    
    // 绘制一些像素点
    Uint32 red = SDL_MapRGBA(SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888), 
                             nullptr, 255, 0, 0, 255);
    Uint32 green = SDL_MapRGBA(SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888), 
                               nullptr, 0, 255, 0, 255);
    
    // 绘制一条线
    for (int i = 0; i < 100; i++) {
        pixelBuffer.setPixel(100 + i, 300, red);
    }
    
    // 绘制一个矩形区域
    for (int y = 200; y < 250; y++) {
        for (int x = 400; x < 450; x++) {
            pixelBuffer.setPixel(x, y, green);
        }
    }
    
    bool running = true;
    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        SDL_RenderClear(renderer);
        
        // 绘制像素缓冲区
        pixelBuffer.draw(renderer);
        
        SDL_RenderPresent(renderer);
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}