#ifndef GAME_H
#define GAME_H

#include "render_window.h"
#include "input_manager.h"

namespace apex {
class Game {
public:
    Game(const RenderWindow &window);
    virtual ~Game();

    InputManager *GetInputManager() const;
    RenderWindow &GetWindow();

    virtual void Initialize() = 0;
    virtual void Logic(double dt) = 0;
    virtual void Render() = 0;

protected:
    InputManager *inputmgr;
    RenderWindow window;
};
}

#endif