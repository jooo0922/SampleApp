#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <android/native_window.h> // ANativeWindow
#include "../render/Renderer.h"
#include "../preview/PreviewController.h"

class Engine
{
public:
  static Engine& instance(); // 싱글톤 접근자

public:
  // android surface 초기화
  void initSurface(ANativeWindow *window);
  // android surface 크기 변경
  void changeSurface(int width, int height);
  // android surface 제거
  void destroySurface();

  // 입력받은 파일 경로 -> 이미지 시퀀스 생성 → Timeline 생성(경로 배열, 초 단위 길이/페이드, 그릴 영역 크기)
  void setImageSequence(const std::vector<std::string>& paths, double clipDurSec, double xfadeSec);

  // Preview 제어
  void previewPlay();
  void previewPause();
  void previewStop();

  // Timeline 총 재생 길이(초) 조회(최근에 생성된 Timeline 기준)
  double getTimelineDuration() { return m_lastTimelineDurationSec; };

private:
  Engine() = default;
  ~Engine() = default;
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

private:
  std::mutex m_mtx;
  std::shared_ptr<Renderer> m_renderer;
  std::shared_ptr<PreviewController> m_previewController;
  bool m_rendererStarted = false;
  double m_lastTimelineDurationSec = 0.0; // 가장 최근에 생성된 Timeline 전체 길이(초) 캐시
};
