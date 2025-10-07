#pragma once

#include <AppSpecsJSI.h>
#include <android/native_window.h> // ANativeWindow
namespace facebook::react {

class NativeSampleModule
    : public NativeSampleModuleCxxSpec<NativeSampleModule> {
public:
  NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker);

public:
  // android surface 초기화
  void initSurface(ANativeWindow *window);
  // android surface 크기 변경
  void changeSurface(int width, int height);
  // android surface 제거
  void destroySurface();

public:
  // 입력받은 파일 경로 -> 이미지 시퀀스 생성 → Timeline 생성(경로 배열, 초 단위 길이/페이드, 그릴 영역 크기)
  void setImageSequence(jsi::Runtime &rt, const std::vector<std::string>& paths, double clipDurSec, double xfadeSec);

  // Preview 제어
  void previewPlay(jsi::Runtime &rt);
  void previewPause(jsi::Runtime &rt);
  void previewStop(jsi::Runtime &rt);

  // Timeline 총 재생 길이(초) 조회(최근에 생성된 Timeline 기준)
  double getTimelineDuration(jsi::Runtime &rt);

public:
  // Encoder 제어
  void startEncoding(jsi::Runtime &rt, int width, int height, int fps, int bitrate, const std::string& mime,const std::string& outputPath);
  void cancelEncoding(jsi::Runtime &rt);
  bool isEncoding(jsi::Runtime &rt);

  // 인코딩된 파일 경로 조회
  std::string getLastEncodedPath(jsi::Runtime &rt);

  // 인코딩 진행률([0.0, 1.0]) 조회
  double getEncodingProgress(jsi::Runtime &rt);
};

} // namespace facebook::react
