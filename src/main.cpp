#include <SDL3/SDL.h>
// #include <SDL3/SDL_main.h>

#include <iostream>

#include <app.h>


int main(int argc, char *argv[]) {
    Application app;
    try {
        app.start(argc, argv);
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
    // gui_test();
    return 0;
}