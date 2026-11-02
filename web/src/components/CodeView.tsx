import type { Snapshot, ValueInfo } from '../lib/api';
type Props = { snapshot?: Snapshot; selectedValue?: ValueInfo; selectedBlock?: number; onValue: (value: number) => void };
export function CodeView({ snapshot, selectedValue, selectedBlock, onValue }: Props) {
  const text = snapshot?.text ?? 'Compile source to inspect a compiler representation.';
  return <section className="code-view" aria-label="IR viewer"><header><div><strong>{snapshot?.displayName ?? 'Representation'}</strong><span>{snapshot?.stageId ?? 'waiting'}</span></div>{selectedValue && <aside><b>%{selectedValue.value}</b> · {selectedValue.type} · bb{selectedValue.definingBlock}</aside>}</header><pre>{text.split('\n').map((line, lineIndex) => <span key={lineIndex} className={selectedBlock !== undefined && line.includes(`bb${selectedBlock}`) ? 'ir-line selected' : 'ir-line'}>{line.split(/(\%\d+)/g).map((part, index) => { const match = /^%(\d+)$/.exec(part); return match ? <button key={index} className="ssa-token" onClick={() => onValue(Number(match[1]))}>{part}</button> : part; })}{'\n'}</span>)}</pre></section>;
}
