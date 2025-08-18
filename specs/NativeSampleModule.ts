import {TurboModule, TurboModuleRegistry} from 'react-native';

export interface Spec extends TurboModule {
  readonly reverseString: (input: string) => string;
  readonly startDecoding: (filePath: string) => void;
  readonly stopDecoding: () => void;
}

export default TurboModuleRegistry.getEnforcing<Spec>('NativeSampleModule');
