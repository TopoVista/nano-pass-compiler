import { afterEach, describe, expect, it, vi } from 'vitest';
import { getCapabilities, run } from './api';

const options = { source: 'int main() { return 0; }', optimizationLevel: 2 };

afterEach(() => vi.unstubAllGlobals());

describe('compiler service client contract', () => {
  it('rejects a readiness response that cannot safely gate execution controls', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => new Response(JSON.stringify({
      schemaVersion: 1,
      status: 'ok',
      ready: true,
      compilerRevision: 'test',
      executionPolicy: 'interpreter-only',
      capabilities: { allowedExecutionModes: ['core', 'ssa'] },
    }))));

    await expect(getCapabilities()).rejects.toThrow('unsupported');
  });

  it('marks a legacy run response as requested rather than falsely reported', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => new Response(JSON.stringify({
      success: true,
      status: 'success',
      errorKind: 'none',
      stdout: '',
      stderr: '',
      returnValue: '0',
      stepsExecuted: 1,
      runtimeProfile: null,
      compilation: {},
    }))));

    const result = await run(options, 'ssa');
    expect(result.executionMode).toBe('ssa');
    expect(result.executionModeReported).toBe(false);
  });
});
