export type Diagnostic = { severity: string; code: string; message: string; line: number; column: number; kind: string };
export type Snapshot = { stageId: string; displayName: string; text: string };
export type PassEvent = { id: string; displayName: string; category: string; changed: boolean; durationUs: number; instructionsBefore: number; instructionsAfter: number; blocksBefore: number; blocksAfter: number; valuesBefore: number; valuesAfter: number; verified: boolean; counters?: Record<string, number> };
export type GraphNode = { id: string; label: string; metadata?: Record<string, string> };
export type GraphEdge = { from: string; to: string; kind: string; metadata?: Record<string, string> };
export type Graph = { kind: string; function: string; nodes: GraphNode[]; edges: GraphEdge[] };
export type Remark = { kind: 'applied' | 'missed' | 'analysis'; passId: string; message: string };
export type ValueInfo = { value: number; function: string; kind: string; type: string; definingBlock: number; line: number; column: number; uses?: { kind: string; block: number; detail: string }[] };
export type Compilation = { schemaVersion: number; success: boolean; compilerRevision: string; diagnostics: Diagnostic[]; snapshots: Snapshot[]; passes: PassEvent[]; remarks: Remark[]; graphs: Graph[]; values: ValueInfo[]; telemetry: { totalCompileUs: number; functions: number; blocks: number; values: number } };
export type RunResult = { success: boolean; errorKind: string; status: string; stdout: string; stderr: string; returnValue: string; stepsExecuted: number; runtimeProfile: RuntimeProfile | null; adaptiveEvents?: string[]; compilation: Compilation };
export type RuntimeProfile = { functions?: Record<string, { calls: number; blocks_executed: number; block_counts: Record<string, number>; edge_counts: { from: number; to: number; count: number }[]; loops: Record<string, { entries: number; backedges: number; exits: number }> }> };

export type CompileOptions = { source: string; optimizationLevel: number; passes?: string[]; includeSsa?: boolean; includeOptimizedSsa?: boolean; includeLLVM?: boolean; includeGraphs?: boolean; includeRemarks?: boolean; includeTelemetry?: boolean; includeProvenance?: boolean; includeCore?: boolean; includeAnf?: boolean; includeCfg?: boolean; includeAst?: boolean; includeDesugaredAst?: boolean };
const base = import.meta.env.VITE_NANO_SERVER_URL ?? 'http://127.0.0.1:8080';
const cache = new Map<string, Compilation>();
const request = async <T>(path: string, body: unknown): Promise<T> => { const response = await fetch(`${base}${path}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }); const data = await response.json() as T; if (!response.ok && !(data as { success?: boolean }).success) return data; return data; };
export async function compile(options: CompileOptions, path = '/api/v1/compile'): Promise<Compilation> { const key = `${path}:${JSON.stringify(options)}`; const existing = cache.get(key); if (existing) return existing; const result = await request<Compilation>(path, { schemaVersion: 1, ...options }); if (result.success) cache.set(key, result); return result; }
export async function run(options: CompileOptions, mode: 'core' | 'ssa' | 'jit' | 'adaptive-jit'): Promise<RunResult> { return request<RunResult>('/api/v1/run', { schemaVersion: 1, ...options, mode }); }
export const allowedPasses = ['sccp', 'simplify-cfg', 'constant-fold', 'algebraic-simplify', 'copy-prop', 'trivial-phi', 'cse', 'dce'];
