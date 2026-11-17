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

    expect(screen.getByText('EQUIVALENT')).toBeInTheDocument();
    expect(screen.getByText('unavailable')).toBeInTheDocument();
    expect(screen.getByText(/JIT support is not present/)).toBeInTheDocument();
    expect(screen.getByText('JIT').closest('article')).toHaveClass('unavailable');
  });
});
