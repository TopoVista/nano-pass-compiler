import Editor from '@monaco-editor/react';
type Props = { value: string; onChange: (value: string) => void };
export function SourceEditor({ value, onChange }: Props) {
  return <Editor height="100%" language="nano" theme="vs-dark" value={value} onChange={(next) => onChange(next ?? '')} options={{ minimap: { enabled: false }, fontSize: 13, lineNumbersMinChars: 3, padding: { top: 12 }, automaticLayout: true, scrollBeyondLastLine: false, ariaLabel: 'Nano source editor' }} beforeMount={(monaco) => {
    monaco.languages.register({ id: 'nano' });
    monaco.languages.setMonarchTokensProvider('nano', { tokenizer: { root: [
      [/\/\/.*$/, 'comment'], [/\/\*/, 'comment', '@comment'], [/[{}()\[\]]/, '@brackets'],
      [/\b(?:if|else|while|for|return|break|continue|print)\b/, 'keyword'],
      [/\b(?:int|bool|void|true|false)\b/, 'type'], [/[a-zA-Z_]\w*/, 'identifier'],
      [/\d+/, 'number'], [/[+\-*/%<>=!&|]+/, 'operator'],
    ], comment: [[/[^*/]+/, 'comment'], [/\*\//, 'comment', '@pop'], [/[*/]/, 'comment']] } });
  }} />;
}
