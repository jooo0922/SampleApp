#include "VideoRect.h"
#include <core/SkRect.h>

VideoRect::VideoRect(std::shared_ptr<IVideoPlayer> player)
  : m_player(player) {};

void VideoRect::update(float dt) {
  // notifyFrame() 를 통해 decoding 된 신규 video frame 갱신 여부 알림 및 SkImage 업데이트
  if (m_player) {
    m_player->update();
  }
};

void VideoRect::draw(SkCanvas* canvas) {
  if (!m_player) return;

  // 가장 최근에 갱신된 video frame(SkImage 포맷) 의 포인터 가져오기
  auto img = m_player->currentFrame();
  if (!img) return;

  // 최근 갱신된 video frame 이미지를 기반으로 rect draw call
  SkSamplingOptions samp;
  SkRect dst = SkRect::MakeWH((float)m_player->width(), (float)m_player->height()); // video frame 이미지와 동일한 크기의 SkRect 생성
  canvas->drawImageRect(img, dst, samp, nullptr);
};
