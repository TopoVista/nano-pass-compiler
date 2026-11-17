export type Diagnostic = { severity: string; code: string; message: string; line: number; column: number; kind: string };
export type Snapshot = { stageId: string; displayName: string; text: string };
export type Transformation = { passId: string; before: string; after: string };
export type PassEvent = { id: string; displayName: string; category: string; changed: boolean; durationUs: number; instructionsBefore: number; instructionsAfter: number; blocksBefore: number; blocksAfter: number; valuesBefore: number; valuesAfter: number; verified: boolean; counters?: Record<string, number> };
export type GraphNode = { id: string; label: string; metadata?: Record<string, string> };
export type GraphEdge = { from: string; to: string; kind: string; metadata?: Record<string, string> };
export type Graph = { kind: string; function: string; nodes: GraphNode[]; edges: GraphEdge[] };
export type Remark = { kind: 'applied' | 'missed' | 'analysis'; passId: string; message: string; block?: number | null; value?: number | null; line?: number; column?: number };
export type ValueInfo = { value: number; function: string; kind: string; type: string; definingBlock: number; line: number; column: number; uses?: { kind: string; block: number; detail: string }[] };
export type PolicyFeature = { function: string; instructions: number; blocks: number; loops: number; callers: number; callees: number; dynamicCalls: number; dynamicBlocks: number; dynamicLoopBackedges: number; hotness: string; hotnessReason: string };
export type InlinePolicyRecord = { record_kind: 'inline-imitation'; caller: string; callee: string; features: { instruction_count: number; block_count: number; dynamic_call_count: number; callee_count: number }; estimated_inline_cost: number; caller_instruction_count: number; heuristic_action: 'inline' | 'do-not-inline'; legality: 'pass' | string; policy_type: 'heuristic' | 'learned'; model_version?: string; model_action?: 'inline' | 'do-not-inline'; confidence?: number };
export type Compilation = { schemaVersion: number; success: boolean; compilerRevision: string; diagnostics: Diagnostic[]; snapshots: Snapshot[]; transformations: Transformation[]; passes: PassEvent[]; remarks: Remark[]; graphs: Graph[]; values: ValueInfo[]; policyFeatures: PolicyFeature[]; policyRecords?: InlinePolicyRecord[]; telemetry: { totalCompileUs: number; functions: number; blocks: number; values: number; instructions: number } };
export type AdaptiveEvent = { kind: string; function: string; calls: number; detail: string };
export type CorrectnessCheckStatus = 'passed' | 'failed' | 'skipped' | 'unavailable';
export type CorrectnessResult = { schemaVersion: number; success: boolean; equivalent: boolean; checks: { stage: string; passed: boolean; status: CorrectnessCheckStatus; detail: string }[]; compilation: Compilation };
export type RunResult = { success: boolean; errorKind: string; status: string; stdout: string; stderr: string; returnValue: string; stepsExecuted: number; executionDurationUs?: number; runtimeProfile: RuntimeProfile | null; adaptiveEvents?: AdaptiveEvent[]; policyFeatures?: PolicyFeature[]; compilation: Compilation };
export type RuntimeProfile = { functions?: Record<string, { calls: number; blocks_executed: number; block_counts: Record<string, number>; edge_counts: { from: number; to: number; count: number }[]; loops: Record<string, { entries: number; backedges: number; exits: number }> }> };

export type CompileOptions = { source: string; optimizationLevel: number; policy?: 'default' | 'heuristic' | 'learned'; passes?: string[]; includeSource?: boolean; includeSsa?: boolean; includeOptimizedSsa?: boolean; includeLLVM?: boolean; includeGraphs?: boolean; includeRemarks?: boolean; includeTelemetry?: boolean; includeProvenance?: boolean; includeCore?: boolean; includeAnf?: boolean; includeCfg?: boolean; includeAst?: boolean; includeDesugaredAst?: boolean };
export type ServiceCapabilities = { schemaVersion: number; status: 'ok'; ready: boolean; compilerRevision: string; llvmVersion?: string; executionPolicy: 'disabled' | 'interpreter-only' | 'local-trusted'; capabilities: { llvmSupport: boolean; llvmIREmission: boolean; jitBuild: boolean; adaptiveJitBuild: boolean; allowedExecutionModes: Array<'core' | 'ssa' | 'jit' | 'adaptive-jit'> } };
// Vercel variables are injected at build time. Normalize the configured origin
// so that a common dashboard value such as `https://api.example.com/` never
// becomes a double-slash API request.
const base = (import.meta.env.VITE_NANO_SERVER_URL ?? 'http://127.0.0.1:8080').replace(/\/+$/, '');
const cache = new Map<string, Compilation>();
const maxCachedCompilations = 32;
export class CompilerServiceError extends Error {
  constructor(message: string, readonly status?: number) {
    super(message);
    this.name = 'CompilerServiceError';
  }
}
const request = async <T>(path: string, body: unknown, signal?: AbortSignal): Promise<T> => {
  let response: Response;
  try {
    response = await fetch(`${base}${path}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
      signal,
    });
  } catch (error) {
    if (error instanceof DOMException && error.name === 'AbortError') throw error;
    throw new CompilerServiceError('The compiler service could not be reached.');
  }
  const text = await response.text();
  let data: T;
  try {
    data = JSON.parse(text) as T;
  } catch {
    throw new CompilerServiceError('The compiler service returned an invalid response.', response.status);
  }
  if (
    !response.ok &&
    (typeof data !== 'object' || data === null || !('success' in data))
  ) {
    throw new CompilerServiceError('The compiler service rejected this request.', response.status);
  }
  return data;
};
export async function compile(options: CompileOptions, path = '/api/v1/compile', signal?: AbortSignal): Promise<Compilation> { const key = `${path}:${JSON.stringify(options)}`; const existing = cache.get(key); if (existing) { cache.delete(key); cache.set(key, existing); return existing; } const result = await request<Compilation>(path, { schemaVersion: 1, ...options }, signal); if (result.success) { cache.set(key, result); if (cache.size > maxCachedCompilations) cache.delete(cache.keys().next().value!); } return result; }
export async function run(options: CompileOptions, mode: 'core' | 'ssa' | 'jit' | 'adaptive-jit', signal?: AbortSignal): Promise<RunResult> { return request<RunResult>('/api/v1/run', { schemaVersion: 1, ...options, mode }, signal); }
export async function checkCorrectness(options: CompileOptions, signal?: AbortSignal): Promise<CorrectnessResult> { return request<CorrectnessResult>('/api/v1/correctness', { schemaVersion: 1, ...options }, signal); }
export async function getCapabilities(signal?: AbortSignal): Promise<ServiceCapabilities> {
  let response: Response;
  try { response = await fetch(`${base}/api/v1/readiness`, { signal }); }
  catch (error) { if (error instanceof DOMException && error.name === 'AbortError') throw error; throw new CompilerServiceError('The compiler service could not be reached.'); }
  const data = await response.json() as ServiceCapabilities;
  if (!response.ok || data.schemaVersion !== 1) throw new CompilerServiceError('The compiler service readiness response is unsupported.', response.status);
  return data;
}
export const allowedPasses = ['sccp', 'simplify-cfg', 'constant-fold', 'algebraic-simplify', 'copy-prop', 'trivial-phi', 'cse', 'dce'];
