#ifndef GRAPHICS_H
#define GRAPHICS_H

class Image {
public:
    Image(int id, SDL_Texture *texture);
    ~Image();

public:
    int id;
    int x_offset, y_offset;
    int width, height;

private:
    SDL_Texture *texture;

    friend class Graphics;
};


typedef std::map<std::string, Image *> ImageMap;


class Graphics {
public:
    void start();
    void stop();
    void blit(Image *im, int x, int y, int alpha=255);
    void blit(SDL_Surface *surface, Image *im, int x, int y);
    void draw_lines(Uint8 R, Uint8 G, Uint8 B, SDL_Point *points, int count);
    void write_text(Uint8 R, Uint8 G, Uint8 B, std::string text, int x, int y);
    void update();
    int get_width() { return width; }
    int get_height() { return height; }

public:
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width;
    int height;
    TTF_Font *small_font;
};

#endif
