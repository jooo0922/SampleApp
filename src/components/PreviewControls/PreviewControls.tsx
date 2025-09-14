import React from 'react';
import {Button, StyleSheet, View, Text} from 'react-native';
import SampleTurboModule from '../../../specs/NativeSampleModule';

interface Props {
  durationSec: number;
  hasTimeline: boolean;
}

const PreviewControls: React.FC<Props> = ({durationSec, hasTimeline}) => {
  return (
    <View style={styles.container}>
      <View style={styles.row}>
        <View style={styles.btn}>
          <Button
            title="Play"
            onPress={() => SampleTurboModule.previewPlay()}
            disabled={!hasTimeline}
          />
        </View>
        <View style={styles.btn}>
          <Button
            title="Pause"
            onPress={() => SampleTurboModule.previewPause()}
            disabled={!hasTimeline}
          />
        </View>
        <View style={styles.btn}>
          <Button
            title="Stop"
            onPress={() => SampleTurboModule.previewStop()}
            disabled={!hasTimeline}
          />
        </View>
      </View>
      <Text style={styles.dur}>
        Duration: {durationSec > 0 ? durationSec.toFixed(2) + 's' : '-'}
      </Text>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    gap: 8,
  },
  row: {
    flexDirection: 'row',
    gap: 8,
    width: '100%',
  },
  btn: {
    flex: 1,
  },
  dur: {
    color: '#aaa',
    fontSize: 12,
  },
});

export default PreviewControls;
