type DiffRow = { before?: string; after?: string; kind: 'same' | 'removed' | 'added' };

/**
 * Builds a conventional line diff for compiler-captured snapshots. It does
 * not imply an instruction mapping: unchanged lines are aligned, while other
 * lines remain explicitly added or removed.
 */
export function lineDiff(before: string, after: string): DiffRow[] {
  const left = before.split('\n');
  const right = after.split('\n');
  // Avoid quadratic allocation for unusually large emitted representations.
  if (left.length * right.length > 30_000) {
    return [...left.map((line) => ({ before: line, kind: 'removed' as const })), ...right.map((line) => ({ after: line, kind: 'added' as const }))];
  }
  const table = Array.from({ length: left.length + 1 }, () => Array<number>(right.length + 1).fill(0));
  for (let i = left.length - 1; i >= 0; i -= 1) {
    for (let j = right.length - 1; j >= 0; j -= 1) {
      table[i][j] = left[i] === right[j] ? table[i + 1][j + 1] + 1 : Math.max(table[i + 1][j], table[i][j + 1]);
    }
  }
  const rows: DiffRow[] = [];
  let i = 0;
  let j = 0;
  while (i < left.length && j < right.length) {
    if (left[i] === right[j]) {
      rows.push({ before: left[i], after: right[j], kind: 'same' });
      i += 1;
      j += 1;
    } else if (table[i + 1][j] >= table[i][j + 1]) {
      rows.push({ before: left[i], kind: 'removed' });
      i += 1;
    } else {
      rows.push({ after: right[j], kind: 'added' });
      j += 1;
    }
  }
  while (i < left.length) rows.push({ before: left[i++], kind: 'removed' });
  while (j < right.length) rows.push({ after: right[j++], kind: 'added' });
  return rows;
}

export function TransformationDiff({ before, after, passId }: { before: string; after: string; passId: string }) {
  const rows = lineDiff(before, after);
  return <section className="transformation-diff" aria-label={`Before and after compiler snapshot for ${passId}`}>
    <header><div><strong>Before / after</strong><span>{passId}</span></div><small>Conventional line diff of compiler-captured snapshots</small></header>
    <div className="transformation-columns"><div><span>BEFORE</span>{rows.map((row, index) => <pre key={`before:${index}`} className={row.kind === 'removed' ? 'removed' : ''}>{row.before ?? ''}</pre>)}</div><div><span>AFTER</span>{rows.map((row, index) => <pre key={`after:${index}`} className={row.kind === 'added' ? 'added' : ''}>{row.after ?? ''}</pre>)}</div></div>
  </section>;
}
