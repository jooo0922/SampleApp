#include "RotatingRect.h"
#include <core/SkRect.h>

void RotatingRect::update(float dt) {
  // 매 프레임마다 증가시킨 rect 의 회전각을 나머지 연산을 통해 [0, 360] 범위 내로 clamping
  m_angle = fmodf(m_angle + m_speed * dt, 360.0f);
};

void RotatingRect::draw(SkCanvas* canvas) {
  if (!canvas) return;

  int canvas_width = canvas->imageInfo().width();
  int canvas_height = canvas->imageInfo().height();

  // 변환 적용 전 캔버스 상태 저장
  canvas->save();

  // 캔버스 상태 변환
  float cx = static_cast<float>(canvas_width * 0.5f);
  float cy = static_cast<float>(canvas_height * 0.5f);
  canvas->translate(cx, cy);
  canvas->rotate(m_angle);

  // 출력 색상 및 antialiasing 상태 설정
  m_paint.setColor(m_color);
  m_paint.setAntiAlias(true);

  // rect draw call
  float offset_x = -(m_width * 0.5f);
  float offset_y = -(m_height * 0.5f);
  SkRect rect = SkRect::MakeXYWH(offset_x, offset_y, m_width, m_height);
  canvas->drawRect(rect, m_paint);

  // 변환 적용 전 상태로 캔버스 복원
  canvas->restore();
};
