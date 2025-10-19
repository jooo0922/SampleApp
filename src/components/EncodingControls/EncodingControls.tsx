import React, {useCallback, useEffect, useMemo, useState} from 'react';
import {Button, StyleSheet, View, Text} from 'react-native';
import SampleTurboModule from '../../../specs/NativeSampleModule';

interface Props {
  hasTimeline: boolean;
}

const DEFAULT_ENCODER_CONFIG = {
  width: 1280,
  height: 720,
  fps: 30,
  bitrate: 4_000_000,
  mime: 'video/avc',
};

const EncodingControls: React.FC<Props> = ({hasTimeline}) => {
  const [isEncoding, setIsEncoding] = useState(false);
  const [progress, setProgress] = useState(0);
  const [lastOutputPath, setLastOutputPath] = useState<string | null>(null);

  /** 네이티브 모듈의 encoding 관련 상태값들을 가져와서 동기화 */
  useEffect(() => {
    try {
      const encoding = SampleTurboModule.isEncoding();
      setIsEncoding(encoding);
      if (!encoding) {
        const path = SampleTurboModule.getLastEncodedPath();
        if (path) {
          setLastOutputPath(path);
        }
      }
    } catch (error) {
      console.warn('[EncodingControls] Failed to sync encoding state:', error);
    }
  }, []);

  /** 인코딩 중일 때 200ms 간격으로 진행률 polling */
  useEffect(() => {
    if (!isEncoding) return;

    const timer = setInterval(() => {
      try {
        setProgress(SampleTurboModule.getEncodingProgress()); // 여기가 200ms 마다 진행률 polling 하는 지점
        if (!SampleTurboModule.isEncoding()) {
          /**
           * 200ms 간격으로 진행률을 확인하다가 인코딩이 끝났으면(성공/취소) 엔진 쪽에서
           * encoding thread 를 정리했다는 신호니까 더 이상 polling을 돌리지 않도록 타이머 해제함.
           *
           * 더불어, 네이티브 쪽에 남아 있는 최종 진행률(getEncodingProgress)과
           * 마지막 출력 파일 경로(getLastEncodedPath)까지 한 번 더 읽어 와서 최신 정보로 UI를 갱신
           */
          clearInterval(timer);
          setIsEncoding(false);
          setProgress(SampleTurboModule.getEncodingProgress());
          const path = SampleTurboModule.getLastEncodedPath();
          if (path) {
            setLastOutputPath(path);
          }
        }
      } catch (error) {
        clearInterval(timer);
        console.warn(
          '[EncodingControls] Failed to poll encoding progress:',
          error,
        );
        setIsEncoding(false);
      }
    }, 200);

    // 의존성 배열의 isEncoding 값이 false로 바뀌면 이 클린업 함수가 호출되어 기존에 생성되었던 타이머 해제
    return () => clearInterval(timer);
  }, [isEncoding]);

  /** 인코딩 시작 버튼 클릭 시 콜백 정의 */
  const handleStartEncoding = useCallback(() => {
    const outputPath = `/sdcard/Movies/sampleapp_${Date.now()}.mp4`;
    try {
      SampleTurboModule.startEncoding(
        DEFAULT_ENCODER_CONFIG.width,
        DEFAULT_ENCODER_CONFIG.height,
        DEFAULT_ENCODER_CONFIG.fps,
        DEFAULT_ENCODER_CONFIG.bitrate,
        DEFAULT_ENCODER_CONFIG.mime,
        outputPath,
      );
      setIsEncoding(true);
      setProgress(0);
      setLastOutputPath(null);
    } catch (error) {}
  }, []);

  /** 인코딩 중단 버튼 클릭 시 콜백 정의 */
  const handleCancelEncoding = useCallback(() => {
    try {
      SampleTurboModule.cancelEncoding();
      setIsEncoding(SampleTurboModule.isEncoding());
      setProgress(SampleTurboModule.getEncodingProgress());
      const path = SampleTurboModule.getLastEncodedPath();
      if (path) {
        setLastOutputPath(path);
      }
    } catch (error) {
      console.warn('[EncodingControls] Failed to cancel encoding:', error);
    }
  }, []);

  return (
    <View style={styles.container}>
      <View style={styles.row}>
        <View style={styles.btn}>
          <Button title="Start Encoding" onPress={} disabled={!hasTimeline} />
        </View>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    gap: 8,
    paddingVertical: 8,
  },
  row: {
    flexDirection: 'row',
    gap: 8,
  },
  btn: {
    flex: 1,
  },
});

export default EncodingControls;
