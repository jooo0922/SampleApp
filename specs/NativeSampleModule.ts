import {TurboModule, TurboModuleRegistry} from 'react-native';

export interface Spec extends TurboModule {
  // Timeline 기반 Preview 제어 API
  readonly setImageSequence: (
    paths: string[],
    clipDurSec: number,
    xfadeSec: number,
  ) => void;
  readonly previewPlay: () => void;
  readonly previewPause: () => void;
  readonly previewStop: () => void;
  readonly getTimelineDuration: () => number;

  // Timeline 기반 Encode 제어 API
  readonly startEncoding: (
    width: number,
    height: number,
    fps: number,
    bitrate: number,
    mime: string,
    outputPath: string,
  ) => void;
  readonly cancelEncoding: () => void;
  readonly isEncoding: () => boolean;
  readonly getLastEncodedPath: () => string;
  readonly getEncodingProgress: () => number;
}

export default TurboModuleRegistry.getEnforcing<Spec>('NativeSampleModule');
