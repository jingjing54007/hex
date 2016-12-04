#ifndef MESSAGE_WINDOW_H
#define MESSAGE_WINDOW_H

#include "hexav/ui/ui.h"


class Resources;
class Graphics;
class GameView;

class MessageWindow: public UiWindow {
public:
    MessageWindow(int x, int y, int width, int height, Resources *resources, Graphics *graphics, GameView *view);
    void draw(const UiContext& context);

private:
    Resources *resources;
    Graphics *graphics;
    GameView *view;
};

#endif