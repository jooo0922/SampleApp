/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 */

import React, {useState} from 'react';
import {
  SafeAreaView,
  StyleSheet,
  View,
  Text,
  requireNativeComponent,
} from 'react-native';

import ImageSequencePicker from '../src/components/ImageSequencePicker/ImageSequencePicker';
import PreviewControls from '../src/components/PreviewControls/PreviewControls';

const SkiaView = requireNativeComponent('SkiaView');

function App(): React.JSX.Element {
  const [timelineDurationSec, setTimelineDurationSec] = useState(0);
  const [timelineReady, setTimelineReady] = useState(false);

  return (
    <SafeAreaView style={styles.container}>
      <View style={styles.title}>
        <Text style={styles.title}>image sequence encoder</Text>
      </View>

      <View style={styles.preview}>
        <SkiaView style={styles.skiaView} />
      </View>

      <View style={styles.controls}>
        <ImageSequencePicker
          onLoaded={(durationSec, hasTimeline) => {
            setTimelineDurationSec(durationSec);
            setTimelineReady(hasTimeline);
          }}
        />

        <PreviewControls
          durationSec={timelineDurationSec}
          hasTimeline={timelineReady}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    margin: 16,
  },
  title: {
    flex: 1,
    justifyContent: 'center', // 세로 가운데 정렬
    fontSize: 24,
    fontFamily: 'Roboto',
    fontWeight: '700',
    textAlign: 'center',
    textTransform: 'uppercase',
    marginBottom: 16,
  },
  preview: {
    flex: 7, // 7 : 3 비율 (위)
    marginBottom: 16, // 영역 사이 간격 1rem
  },
  controls: {
    flex: 2, // 7 : 3 비율 (아래)
    justifyContent: 'flex-start',
    gap: 8,
  },
  skiaView: {
    flex: 1,
    width: '100%',
    height: '100%',
  },
});

export default App;
