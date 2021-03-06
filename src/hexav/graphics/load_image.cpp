#include "common.h"

#include "hexutil/basics/error.h"

#include "hexav/graphics/graphics.h"


namespace hex {

void load_image(const std::string& filename, Graphics *graphics, ImageMap& image_set) {
    SDL_Surface *surface = IMG_Load(filename.c_str());
    if (surface == NULL) {
        throw Error() << "Could not load image: " << filename;
    }

    Uint32 pf = SDL_GetWindowPixelFormat(graphics->window);
    if (SDL_PIXELTYPE(surface->format->format) != SDL_PIXELTYPE(pf)) {
        SDL_Surface *new_surface = SDL_ConvertSurfaceFormat(surface, pf, 0);
        SDL_FreeSurface(surface);
        surface = new_surface;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(graphics->renderer, surface);
    SDL_FreeSurface(surface);

    Image *im = new Image(0, texture);
    im->clip_x_offset = 0;
    im->clip_y_offset = 0;

    size_t pos = filename.find_last_of("/");
    std::string key(filename);
    if (pos != std::string::npos)
        key = filename.substr(pos+1);
    image_set[key] = im;
}

};
