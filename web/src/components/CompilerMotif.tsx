type MotifKind = 'flow' | 'values' | 'verify';

type Props = {
  kind: MotifKind;
  className?: string;
  label: string;
  caption?: string;
};

const copy: Record<MotifKind, { paths: string[]; nodes: Array<[number, number]> }> = {
  flow: {
    paths: ['M20 48H57L78 28H116L137 48H176', 'M78 28V70H128L148 50', 'M128 70L148 50'],
    nodes: [[20, 48], [78, 28], [128, 70], [148, 50], [176, 48]],
  },
  values: {
    paths: ['M20 24H60V48H100V72H176', 'M60 48H80L96 32H130', 'M100 48L118 64H148'],
    nodes: [[20, 24], [60, 48], [100, 48], [130, 32], [148, 64], [176, 72]],
  },
  verify: {
    paths: ['M20 46H62L84 24L112 68L142 38H176'],
    nodes: [[20, 46], [62, 46], [84, 24], [112, 68], [142, 38], [176, 38]],
  },
};

export function CompilerMotif({ kind, className = '', label, caption }: Props) {
  const motif = copy[kind];
  return <figure className={`compiler-motif compiler-motif-${kind} ${className}`.trim()} aria-label={label}>
    <svg viewBox="0 0 196 96" role="img" aria-hidden="true">
      <title>{label}</title>
      <path className="compiler-motif-baseline" d="M12 84H184" />
      {motif.paths.map((path, index) => <path key={path} className={`compiler-motif-route route-${index}`} d={path} />)}
      {motif.nodes.map(([cx, cy], index) => <g key={`${cx}-${cy}`} className="compiler-motif-node">
        <circle cx={cx} cy={cy} r="5" />
        {index === Math.min(2, motif.nodes.length - 1) ? <circle className="compiler-motif-signal" cx={cx} cy={cy} r="2.3" /> : null}
      </g>)}
    </svg>
    {caption ? <figcaption>{caption}</figcaption> : null}
  </figure>;
}
