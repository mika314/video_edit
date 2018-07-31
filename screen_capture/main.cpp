#include "const.hpp"
#include "screen_grabber.hpp"
#include "sdlpp.hpp"
#include <SDL_opengl.h>
#include <iostream>

int main(int argc, char **argv)
{
  int inputWidth;
  int inputHeight;
  sdl::Init(SDL_INIT_EVERYTHING);

  if (argc == 2 && argv[1] == std::string("480"))
  {
    inputWidth = 854;
    inputHeight = 480;
  }
  else if (argc == 1 || (argc == 2 && argv[1] == std::string("720")))
  {
    inputWidth = 1280;
    inputHeight = 720;
  }
  else if (argc == 2 && argv[1] == std::string("1080"))
  {
    inputWidth = 1920;
    inputHeight = 1080;
  }
  else
  {
    std::clog << "input resolution is not specified\n";
    return 1;
  }
  sdl::Window w("Screen Capture",
                1280 + 65,
                0,
                inputWidth + Border * 2,
                inputHeight + Border * 2,
                SDL_WINDOW_OPENGL);
  sdl::Renderer r(w.get(), -1, 0);
  glClearColor(0, 0.5, 0, 1.0);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, inputWidth + Border * 2, inputHeight + Border * 2, 0, -1, 1);

  GLuint texture;
  glGenTextures(1, &texture);

  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glEnable(GL_TEXTURE_2D);

  ScreenGrabber screenGrabber(inputWidth, inputHeight);
  sdl::EventHandler e;
  e.quit = [&screenGrabber](const SDL_QuitEvent &) { screenGrabber.done = true; };
  auto time = SDL_GetTicks() * OutputFrameRate;
  auto drawTime = SDL_GetTicks();
  while (!screenGrabber.done)
  {
    while (e.poll()) {}
    screenGrabber.tick();
    auto curTime = SDL_GetTicks();
    if (curTime > drawTime)
    {
      drawTime += 250;
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   3,
                   screenGrabber.width,
                   screenGrabber.height,
                   0,
                   GL_BGRA,
                   GL_UNSIGNED_BYTE,
                   screenGrabber.packet.data);
      glClear(GL_COLOR_BUFFER_BIT);
      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2f(Border, Border);

      glTexCoord2f(1, 0);
      glVertex2f(screenGrabber.width + Border, Border);

      glTexCoord2f(1, 1);
      glVertex2f(screenGrabber.width + Border, screenGrabber.height + Border);

      glTexCoord2f(0, 1);
      glVertex2f(Border, screenGrabber.height + Border);
      glEnd();
      r.present();
    }

    curTime = SDL_GetTicks() * OutputFrameRate;
    if (curTime < time)
      SDL_Delay((time - curTime) / OutputFrameRate);
    time += 1000;
  }
  std::clog << "bye\n";
  screenGrabber.done = true;
}
