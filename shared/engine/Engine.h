#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <android/native_window.h> // ANativeWindow
#include "../render/Renderer.h"
#include "../preview/PreviewController.h"
#include "../encoder/EncoderConfig.h"
#include "../encoder/IEncoder.h"

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

  // Encoder 제어
  void startEncoding(const EncoderConfig& config);
  void cancelEncoding();
  void joinEncodeThread();
  bool isEncoding() const { return m_isEncoding.load(); };

  // 인코딩된 파일 경로 조회
  std::string getLastEncodedPath() const { return m_lastEncodedPath; };

  // 인코딩 진행률([0.0, 1.0]) 조회
  double getEncodingProgress() const { return m_encodingProgress.load(); };

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

private:
  // Encoder 관련 멤버변수들
  std::shared_ptr<IEncoder> m_encoder;
  std::thread m_encodeThread;
  std::atomic<bool> m_isEncoding = false;
  std::atomic<bool> m_cancelFlag = false;
  std::atomic<double> m_encodingProgress = 0.0;
  std::string m_lastEncodedPath;
};
