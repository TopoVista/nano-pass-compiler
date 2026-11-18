import { fireEvent, render, screen } from '@testing-library/react';
import { BuildAtlas } from './BuildAtlas';

describe('BuildAtlas brief', () => {
  it('keeps the selected implementation and pipeline artifact in sync', () => {
    render(<BuildAtlas mode="brief" />);

    expect(screen.getByText('SSA builder', { selector: '.blueprint-inspector b' })).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: /CoreIR normalize/i }));

    expect(screen.getByText('Explicit IR', { selector: '.blueprint-inspector b' })).toBeInTheDocument();
    expect(screen.getByText('compiler/ir/core/ast_to_core.h')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /CoreIR normalize/i })).toHaveClass('selected');
    expect(screen.getByRole('button', { name: /ANF normalize/i })).not.toHaveClass('selected');
  });
});
