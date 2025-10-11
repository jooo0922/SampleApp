#include "../preview/PreviewController.h"
#include "../render/Renderer.h"
#include "../video/Timeline.h"
#include "../logger/Logger.h"
#include <core/SkData.h>
#include <core/SkImage.h>
#include <core/SkRect.h>

PreviewController::PreviewController(std::shared_ptr<Renderer> renderer)
  : m_pRenderer(std::move(renderer)) {};

bool PreviewController::setImageSequence(const std::vector<std::string>& paths, double clipDurSec, double xfadeSec) {
  // 1) 필수 체크: Renderer 준비 여부 확인
  if (!m_pRenderer) {
    Logger::error(k_logTag, "Renderer not set");
    return false;
  }

  // 2) 파일 경로 배열을 돌면서 이미지를 메모리로 읽어옴.
  //    - SkData::MakeFromFileName: 파일을 바이트로 읽음
  //    - SkImage::MakeFromEncoded: 바이트(압축)를 SkImage로 디코드(필요 시 지연 디코드)
  std::vector<sk_sp<SkImage>> images;
  images.reserve(paths.size());
  for (const auto& p : paths) {
    sk_sp<SkData> data = SkData::MakeFromFileName(p.c_str());
    if (!data) {
      Logger::warn(k_logTag, "Read failed: %s", p.c_str());
      continue; // 바이트 읽기 실패한 파일은 건너뜀.
    }
    sk_sp<SkImage> img = SkImages::DeferredFromEncodedData(std::move(data));
    if (img) {
      images.push_back(std::move(img));
    }
  }

  // SkImage 를 하나도 생성하지 못했다면 Timeline 생성 중단
  if (images.empty()) {
    Logger::warn(k_logTag, "No images loaded");
    return false;
  }

  std::vector<Timeline::ClipRenderData> renderDataList;
  for (auto& img : images) {
    // 3) 그릴 영역(dst) 설정
    //    - Preview 의 가로/세로 크기만큼 꽉 채우도록 사각형을 만듦.
    //    - 나중에 contain/cover 같은 맞춤 모드가 필요하면 여기서 계산을 바꾸면 됩니다.
    //    - 모든 클립 이미지는 가운데 정렬 + 모든 클립의 width 를 dstW 에 맞추고, height 는 비율에 맞게 조정 + height 가 dstH 보다 커지면 crop 처리
    float width = static_cast<float>(m_pRenderer->surfaceWidth());
    float height = width * (static_cast<float>(img->height()) / static_cast<float>(img->width()));
    float x = 0.0f;
    float y = (static_cast<float>(m_pRenderer->surfaceHeight()) - height) / 2.0f;
    renderDataList.emplace_back(img, SkRect::MakeXYWH(x, y, width, height));
  }

  // 4) 타임라인 생성
  //    - 각 이미지당 clipDurSec초 보여주고
  //    - 장면 끝부분에서 xfadeSec초 동안 다음 이미지와 겹치게(부드러운 전환)
  //    - dst 위치/크기로 렌더되도록 설정
  auto timeline = Timeline::FromClipRenderData(renderDataList, clipDurSec, xfadeSec);
  if (!timeline) {
    Logger::warn(k_logTag, "Timeline creation failed");
    return false;
  }

  // 5) 총 길이를 기록해 두고, Renderer에 새 타임라인을 적용.
  m_lastDurationSec = timeline->totalDuration();
  m_pRenderer->setTimeline(std::move(timeline));

  return true;
};

void PreviewController::previewPlay() {
  if (m_pRenderer) {
    m_pRenderer->previewPlay();
  }
};

void PreviewController::previewPause() {
  if (m_pRenderer) {
    m_pRenderer->previewPause();
  }
};

void PreviewController::previewStop() {
  if (m_pRenderer) {
    m_pRenderer->previewStop();
  }
};

double PreviewController::durationSec() const {
  return m_lastDurationSec;
};
