import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { CompilerMotif } from './CompilerMotif';

describe('CompilerMotif', () => {
  it('renders a labelled compiler diagram with a concise explanation', () => {
    render(<CompilerMotif kind="flow" label="Compiler flow" caption="Source becomes graph structure." />);

    expect(screen.getByLabelText('Compiler flow')).toBeInTheDocument();
    expect(screen.getByText('Source becomes graph structure.')).toBeInTheDocument();
    expect(document.querySelectorAll('.compiler-motif-route')).toHaveLength(3);
  });
});
