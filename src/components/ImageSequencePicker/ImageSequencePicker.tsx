import React, {useCallback} from 'react';
import {Button, Alert, StyleSheet, View} from 'react-native';
import DocumentPicker, {
  type DocumentPickerResponse,
} from 'react-native-document-picker';
import SampleTurboModule from '../../../specs/NativeSampleModule';

interface Props {
  clipDurSec?: number;
  xfadeSec?: number;
  onLoaded: (durationSec: number, hasTimeline: boolean) => void;
}

const ImageSequencePicker: React.FC<Props> = ({
  clipDurSec = 1.5,
  xfadeSec = 0.5,
  onLoaded,
}) => {
  /**
   * DocumentPicker 결과에서 네이티브(C++) 모듈에서 Skia 인터페이스가 읽을 수 있는 "실제 파일 절대경로" 배열만 추출한다.
   * 과정:
   * 1) 각 항목에서 fileCopyUri (캐시에 복사된 파일) 우선, 없으면 원래 uri 사용
   * 2) null/undefined 제거
   * 3) 경로가 file:// 로 시작하면 prefix 제거하여 순수 경로로 통일
   * 4) 여전히 '/' 로 시작하지 않는 것(content:// 등)은 제외
   * 결과: SkData::MakeFromFileName에 바로 전달 가능한 정제된 파일 경로 목록
   */
  const normalizePaths = (results: DocumentPickerResponse[]) => {
    return results
      .map(r => r.fileCopyUri || r.uri)
      .filter(Boolean)
      .map(u => (u!.startsWith('file://') ? u!.slice(7) : u!))
      .filter(p => p.startsWith('/')); // 절대경로만 유지
  };

  const pick = useCallback(async () => {
    try {
      // allowMultiSelection: true 로 다중 선택 활성화
      const results: DocumentPickerResponse[] = await DocumentPicker.pick({
        type: [DocumentPicker.types.images],
        allowMultiSelection: true,
        copyTo: 'cachesDirectory', // 캐시에 복사하여 fileCopyUri 제공 시도
      });

      // fileCopyUri 우선, 없으면 (file://) uri fallback
      const paths = normalizePaths(results);
      if (paths.length === 0) {
        // copy 실패 또는 모두 content:// 만 남은 경우
        const copyErrors = results
          .filter(r => r.copyError)
          .map(r => r.copyError);
        if (copyErrors.length > 0) {
          Alert.alert('Copy Failed', copyErrors.join('\n'));
        } else {
          Alert.alert('Selection Failed', 'Invalid file paths');
        }
        return;
      }

      // 로드된 이미지 시퀀스 fileCopyUri 로 타임라인 생성
      SampleTurboModule.setImageSequence(paths, clipDurSec, xfadeSec);

      // 생성된 타임라인에서 계산된 영상 전체 길이 가져오기
      const dur = SampleTurboModule.getTimelineDuration?.() ?? 0;
      onLoaded?.(dur, true);
    } catch (e: any) {
      if (DocumentPicker.isCancel(e)) {
        return; // 사용자 취소
      }
      Alert.alert('Error', String(e));
    }
  }, [clipDurSec, xfadeSec, onLoaded]);

  return (
    <View style={styles.container}>
      <Button title={'Upload images'} onPress={pick} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    alignSelf: 'stretch',
    marginBottom: 8,
  },
});

export default ImageSequencePicker;
