import { render, screen } from '@testing-library/react';
import { Correctness } from './Correctness';

describe('Correctness', () => {
  it('distinguishes an unavailable native check from a failed comparison', () => {
    render(<Correctness result={{
      schemaVersion: 1,
      success: true,
      equivalent: true,
      checks: [
        { stage: 'CoreIR', status: 'passed', passed: true, detail: 'reference execution completed' },
        { stage: 'JIT', status: 'unavailable', passed: false, detail: 'JIT support is not present in this service build' },
      ],
      compilation: {} as never,
    }} />);

    expect(screen.getByText('equivalent')).toBeInTheDocument();
    expect(screen.getByText('unavailable')).toBeInTheDocument();
    expect(screen.getByText(/JIT support is not present/)).toBeInTheDocument();
    expect(screen.getByText('JIT').closest('article')).toHaveClass('unavailable');
  });
  it('does not call an incomplete verification a semantic mismatch', () => {
    render(<Correctness result={{
      schemaVersion: 1,
      success: false,
      equivalent: false,
      checks: [{ stage: 'compiler', status: 'failed', passed: false, detail: 'source could not be compiled' }],
      compilation: {} as never,
    }} />);

    expect(screen.getByText('verification incomplete')).toBeInTheDocument();
    expect(screen.queryByText('mismatch')).not.toBeInTheDocument();
  });
});
