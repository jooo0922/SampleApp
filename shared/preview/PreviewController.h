#pragma once
#include <memory>
#include <string>
#include <vector>
#include "render/Renderer.h"

class PreviewController
{
public:
  explicit PreviewController(std::shared_ptr<Renderer> renderer);

public:
  /** Preview 제어 함수 */
  // 파일 경로 배열을 받아 SkImage 로드 -> Timeline 생성하여 Renderer 객체에 적용
  bool setImageSequence(const std::vector<std::string>& paths, double clipDurSec, double xfadeSec);
  void previewPlay();
  void previewPause();
  void previewStop();
  double durationSec() const;

private:
  std::shared_ptr<Renderer> m_pRenderer;
  double m_lastDurationSec = 0.0;
};