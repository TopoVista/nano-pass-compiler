import { describe, expect, it } from 'vitest';
import { formatNano } from './formatNano';

describe('formatNano', () => {
  it('formats nesting and control-flow without changing Nano tokens', () => {
    expect(formatNano('int main(){int x=4;if(true){x=x+3;}else{x=99;}print(x);return x;}')).toBe([
      'int main() {', '  int x = 4;', '  if (true) {', '    x = x + 3;', '  } else {', '    x = 99;', '  }', '  print(x);', '  return x;', '}'
    ].join('\n'));
  });

  it('keeps comments and array indexing readable', () => {
    expect(formatNano('int main(){// add\nint a[2];a[0]=1;return a[0];}')).toBe([
      'int main() {', '  // add', '  int a[2];', '  a[0] = 1;', '  return a[0];', '}'
    ].join('\n'));
  });
});
