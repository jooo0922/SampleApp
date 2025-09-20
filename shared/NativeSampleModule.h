#pragma once

#include <AppSpecsJSI.h>
#include <android/native_window.h> // ANativeWindow
#include "engine/Engine.h"
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
};

} // namespace facebook::react
