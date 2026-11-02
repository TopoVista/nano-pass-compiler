import Editor, { type OnMount } from '@monaco-editor/react';
import { useEffect, useRef } from 'react';
import type { editor } from 'monaco-editor';

type Props = { value: string; onChange: (value: string) => void; focusLine?: number; onCursorLine?: (line: number) => void };
export function SourceEditor({ value, onChange, focusLine, onCursorLine }: Props) {
  const editorRef = useRef<editor.IStandaloneCodeEditor>();
  useEffect(() => { if (focusLine && editorRef.current) { editorRef.current.revealLineInCenter(focusLine); editorRef.current.setPosition({ lineNumber: focusLine, column: 1 }); editorRef.current.focus(); } }, [focusLine]);
  const onMount: OnMount = (editor) => { editorRef.current = editor; editor.onDidChangeCursorPosition((event) => onCursorLine?.(event.position.lineNumber)); };
  return <Editor height="100%" language="nano" theme="vs-dark" value={value} onMount={onMount} onChange={(next) => onChange(next ?? '')} options={{ minimap: { enabled: false }, fontSize: 13, lineNumbersMinChars: 3, padding: { top: 12 }, automaticLayout: true, scrollBeyondLastLine: false, ariaLabel: 'Nano source editor' }} beforeMount={(monaco) => {
    monaco.languages.register({ id: 'nano' });
    monaco.languages.setMonarchTokensProvider('nano', { tokenizer: { root: [
      [/\/\/.*$/, 'comment'], [/\/\*/, 'comment', '@comment'], [/[{}()\[\]]/, '@brackets'],
      [/\b(?:if|else|while|for|return|break|continue|print)\b/, 'keyword'],
      [/\b(?:int|bool|void|true|false)\b/, 'type'], [/[a-zA-Z_]\w*/, 'identifier'],
      [/\d+/, 'number'], [/[+\-*/%<>=!&|]+/, 'operator'],
    ], comment: [[/[^*/]+/, 'comment'], [/\*\//, 'comment', '@pop'], [/[*/]/, 'comment']] } });
  }} />;
}
