import Editor, { loader, type OnMount } from '@monaco-editor/react';
import { useEffect, useRef } from 'react';
import * as monaco from 'monaco-editor/editor/editor.api.js';
import type { editor } from 'monaco-editor';
import EditorWorker from 'monaco-editor/editor/editor.worker.js?worker';
import { formatNano } from '../lib/formatNano';

// The React wrapper defaults to a CDN loader. Keep Monaco self-contained so
// the editor works on private networks and does not leak a worker error when
// third-party scripts are blocked.
loader.config({ monaco });
const workerHost = globalThis as typeof globalThis & {
  MonacoEnvironment?: { getWorker: () => Worker };
};
workerHost.MonacoEnvironment = { getWorker: () => new EditorWorker() };

type Props = { value: string; onChange: (value: string) => void; focusLine?: number; onCursorLine?: (line: number) => void };
export function SourceEditor({ value, onChange, focusLine, onCursorLine }: Props) {
  const editorRef = useRef<editor.IStandaloneCodeEditor>();
  useEffect(() => { if (focusLine && editorRef.current) { editorRef.current.revealLineInCenter(focusLine); editorRef.current.setPosition({ lineNumber: focusLine, column: 1 }); editorRef.current.focus(); } }, [focusLine]);
  const onMount: OnMount = (editor) => {
    editorRef.current = editor;
    editor.onDidChangeCursorPosition((event) => onCursorLine?.(event.position.lineNumber));
    editor.addAction({ id: 'nano.format-source', label: 'Format NanoPass source', keybindings: [2048 | 36], run: () => onChange(formatNano(editor.getValue())) });
  };
  return <Editor height="100%" language="nano" theme="vs-dark" value={value} onMount={onMount} onChange={(next) => onChange(next ?? '')} options={{ minimap: { enabled: true, maxColumn: 88, renderCharacters: false, size: 'fit', showSlider: 'mouseover' }, fontSize: 15, fontFamily: 'SFMono-Regular, SF Mono, Menlo, Monaco, Consolas, monospace', fontLigatures: true, lineHeight: 25, lineNumbersMinChars: 3, padding: { top: 20, bottom: 28 }, automaticLayout: true, scrollBeyondLastLine: false, renderLineHighlight: 'all', cursorBlinking: 'smooth', bracketPairColorization: { enabled: true }, guides: { indentation: true, bracketPairs: true, highlightActiveIndentation: true }, smoothScrolling: true, stickyScroll: { enabled: true }, wordWrap: 'off', ariaLabel: 'NanoPass C source editor' }} beforeMount={(monaco) => {
    monaco.languages.register({ id: 'nano' });
    monaco.languages.setMonarchTokensProvider('nano', { tokenizer: { root: [
      [/\/\/.*$/, 'comment'], [/\/\*/, 'comment', '@comment'], [/[{}()\[\]]/, '@brackets'],
      [/\b(?:if|else|while|for|return|break|continue|print)\b/, 'keyword'],
      [/\b(?:int|bool|void|true|false)\b/, 'type'], [/[a-zA-Z_]\w*/, 'identifier'],
      [/\d+/, 'number'], [/(?:->|[+\-*/%<>=!&|]+)/, 'operator'],
    ], comment: [[/[^*/]+/, 'comment'], [/\*\//, 'comment', '@pop'], [/[*/]/, 'comment']] } });
  }} />;
}
