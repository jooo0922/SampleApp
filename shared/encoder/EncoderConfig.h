#pragma once

#include <string>

/*
 * EncoderConfig
 * - 인코더 동작에 필요한 공통 설정값을 담는 구조체입니다.
 * - 플랫폼별 구현은 이 값을 기반으로 MediaCodec(안드로이드)/VideoToolbox(iOS, 향후) 등을 설정합니다.
 *
 * 주요 필드:
 *   width/height       : 출력 해상도(px)
 *   fps                : 초당 프레임 수
 *   bitrate            : 비트레이트(bps). 타겟 화질/파일 크기를 좌우합니다.
 *   iFrameIntervalSec  : 키프레임 간격(초). 탐색성/복원력에 영향
 *   mime               : 비디오 MIME. 예) "video/avc"(H.264), "video/hevc"(H.265)
 *   outputPath         : 최종 mp4 등 컨테이너 파일의 절대 경로
 *
 * 권장값:
 * - H.264 720p: 4~6 Mbps, 30fps
 * - H.264 1080p: 8~12 Mbps, 30fps
 */

struct EncoderConfig
{
  int width = 1280;                 // 출력 해상도 가로(px)
  int height = 720;                 // 출력 해상도 세로(px)
  int fps = 30;                     // 초당 프레임 수
  int bitrate = 4'000'000;          // 비트레이트(bps)
  int iFrameIntervalSec = 2;        // 키프레임 간격(초)
  std::string mime = "video/avc";   // 코덱 MIME (기본: H.264)
  std::string outputPath;           // 결과 파일 절대 경로(앱 전용 Movies 디렉터리 권장)
};
