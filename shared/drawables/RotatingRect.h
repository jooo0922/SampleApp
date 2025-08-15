#pragma once
#include "IDrawable.h"
#include <core/SkPaint.h>

class RotatingRect : public IDrawable
{
public:
  void update(float dt) override;
  void draw(SkCanvas* canvas) override;

public:
  void setSize(float width, float height) {
    m_width = width;
    m_height = height;
  };
  void setColor(SkColor color) { m_color = color; };
  void setSpeed(float degPerSec) { m_speed = degPerSec; };

private:
  float m_angle = 0.0f;
  float m_speed = 60.0f;
  float m_width = 100.0f;
  float m_height = 100.0f;
  SkColor m_color = SK_ColorBLACK;
  SkPaint m_paint;
};
