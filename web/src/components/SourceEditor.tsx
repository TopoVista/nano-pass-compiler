import Editor, { type OnMount } from '@monaco-editor/react';
import { useEffect, useRef } from 'react';
import type { editor } from 'monaco-editor';
import { formatNano } from '../lib/formatNano';

type Props = { value: string; onChange: (value: string) => void; focusLine?: number; onCursorLine?: (line: number) => void };
export function SourceEditor({ value, onChange, focusLine, onCursorLine }: Props) {
  const editorRef = useRef<editor.IStandaloneCodeEditor>();
  useEffect(() => { if (focusLine && editorRef.current) { editorRef.current.revealLineInCenter(focusLine); editorRef.current.setPosition({ lineNumber: focusLine, column: 1 }); editorRef.current.focus(); } }, [focusLine]);
  const onMount: OnMount = (editor) => {
    editorRef.current = editor;
    editor.onDidChangeCursorPosition((event) => onCursorLine?.(event.position.lineNumber));
    editor.addAction({ id: 'nano.format-source', label: 'Format NanoPass source', keybindings: [2048 | 36], run: () => onChange(formatNano(editor.getValue())) });
  };
  return <Editor height="100%" language="nano" theme="vs-dark" value={value} onMount={onMount} onChange={(next) => onChange(next ?? '')} options={{ minimap: { enabled: false }, fontSize: 14, fontFamily: 'SFMono-Regular, Menlo, Monaco, Consolas, monospace', fontLigatures: true, lineHeight: 23, lineNumbersMinChars: 3, padding: { top: 16, bottom: 16 }, automaticLayout: true, scrollBeyondLastLine: false, renderLineHighlight: 'all', cursorBlinking: 'smooth', bracketPairColorization: { enabled: true }, guides: { indentation: true, bracketPairs: true }, smoothScrolling: true, ariaLabel: 'NanoPass C source editor' }} beforeMount={(monaco) => {
    monaco.languages.register({ id: 'nano' });
    monaco.languages.setMonarchTokensProvider('nano', { tokenizer: { root: [
      [/\/\/.*$/, 'comment'], [/\/\*/, 'comment', '@comment'], [/[{}()\[\]]/, '@brackets'],
      [/\b(?:fn|let|when|otherwise|loop|emit|yield|halt|next|if|else|while|for|return|break|continue|print)\b/, 'keyword'],
      [/\b(?:i32|truth|unit|int|bool|void|true|false)\b/, 'type'], [/[a-zA-Z_]\w*/, 'identifier'],
      [/\d+/, 'number'], [/(?:->|[+\-*/%<>=!&|]+)/, 'operator'],
    ], comment: [[/[^*/]+/, 'comment'], [/\*\//, 'comment', '@pop'], [/[*/]/, 'comment']] } });
  }} />;
}
