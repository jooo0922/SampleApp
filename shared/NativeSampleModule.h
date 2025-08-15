#pragma once

#include <AppSpecsJSI.h>
#include <memory>
#include <string>
#include <android/native_window.h> // ANativeWindow
#include "render/Renderer.h"

namespace facebook::react {

class NativeSampleModule
    : public NativeSampleModuleCxxSpec<NativeSampleModule> {
public:
  NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker);

  std::string reverseString(jsi::Runtime &rt, std::string input);

  // android surface 초기화
  void initSurface(ANativeWindow *window);
  // android surface 크기 변경
  void changeSurface(int width, int height);
  // android surface 제거
  void destroySurface();

private:
  std::shared_ptr<Renderer> m_pRenderer;
  bool m_bStarted = false;
};

} // namespace facebook::react
