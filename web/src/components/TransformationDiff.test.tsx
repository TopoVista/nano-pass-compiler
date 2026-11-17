import { render, screen } from '@testing-library/react';
import { expect, test } from 'vitest';
import { lineDiff, TransformationDiff } from './TransformationDiff';

test('keeps unchanged lines aligned and marks only snapshot additions/removals', () => {
  expect(lineDiff('bb0:\n  %1 = add %x, 0\n  return %1', 'bb0:\n  return %x')).toEqual([
    { before: 'bb0:', after: 'bb0:', kind: 'same' },
    { before: '  %1 = add %x, 0', kind: 'removed' },
    { before: '  return %1', kind: 'removed' },
    { after: '  return %x', kind: 'added' },
  ]);
});

test('labels the comparison as compiler-captured snapshots', () => {
  render(<TransformationDiff passId="sccp" before="%1 = const 1" after="return 1" />);
  expect(screen.getByLabelText(/before and after compiler snapshot for sccp/i)).toBeInTheDocument();
  expect(screen.getByText(/conventional line diff of compiler-captured snapshots/i)).toBeInTheDocument();
});
