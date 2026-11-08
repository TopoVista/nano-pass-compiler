const tokenPattern = /\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|(?:->|==|!=|<=|>=|&&|\|\||\+\+|--|\+=|-=|\*=|\/=|%=)|[A-Za-z_]\w*|\d+|[^\s]/g;
const word = /^[A-Za-z_]\w*$|^\d+$/;
const operators = new Set(['=', '+', '-', '*', '/', '%', '->', '==', '!=', '<', '>', '<=', '>=', '&&', '||', '+=', '-=', '*=', '/=', '%=']);
const controlWords = new Set(['if', 'while', 'for', 'switch']);

/**
 * Formats the supported Nano surface syntax without changing tokens. It is a
 * presentation helper, not a parser or a source transformation.
 */
export function formatNano(source: string): string {
  const tokens = source.match(tokenPattern) ?? [];
  const lines: string[] = [];
  let current = '';
  let indent = 0;
  let previous = '';
  const write = (text: string, spaceBefore = false) => {
    if (spaceBefore && current && !current.endsWith(' ')) current += ' ';
    current += text;
  };
  const flush = () => {
    const line = current.trimEnd();
    if (line) lines.push(`${'  '.repeat(Math.max(0, indent))}${line}`);
    current = '';
  };

  for (let index = 0; index < tokens.length; ++index) {
    const token = tokens[index];
    const next = tokens[index + 1] ?? '';
    if (token.startsWith('//')) { flush(); write(token); flush(); previous = token; continue; }
    if (token.startsWith('/*')) { flush(); for (const line of token.split('\n')) { write(line.trim()); flush(); } previous = token; continue; }
    if (token === '{') { write('{', Boolean(current) && !current.endsWith(' ')); flush(); ++indent; previous = token; continue; }
    if (token === '}') { flush(); --indent; write('}'); if (next !== 'else') flush(); previous = token; continue; }
    if (token === ';') { current = current.trimEnd(); write(';'); flush(); previous = token; continue; }
    if (token === ',') { current = current.trimEnd(); write(','); write(' '); previous = token; continue; }
    if (token === '(') { write('(', controlWords.has(previous)); previous = token; continue; }
    if (token === ')') { current = current.trimEnd(); write(')'); previous = token; continue; }
    if (token === '[') { write('['); previous = token; continue; }
    if (token === ']') { current = current.trimEnd(); write(']'); previous = token; continue; }
    if (token === ':') { current = current.trimEnd(); write(': '); previous = token; continue; }
    if (token === '++' || token === '--' || token === '!') { write(token, false); previous = token; continue; }
    if (operators.has(token)) { current = current.trimEnd(); write(` ${token} `); previous = token; continue; }
    const needsSpace = token === 'else' || (Boolean(current) && !current.endsWith(' ') && (word.test(token) || token.startsWith('"') || token.startsWith("'")) && (word.test(previous) || previous === ')' || previous === ']'));
    write(token, needsSpace);
    previous = token;
  }
  flush();
  return lines.join('\n').replace(/\n{3,}/g, '\n\n').trimEnd() + (source.endsWith('\n') ? '\n' : '');
}
