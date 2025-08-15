#pragma once
#include <core/SkCanvas.h>

class IDrawable
{
public:
  virtual ~IDrawable() = default;
  virtual void update(float dt) = 0;
  virtual void draw(SkCanvas* pcanvas) = 0;
};
