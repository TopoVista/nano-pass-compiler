import { defineConfig } from '@playwright/test';

const compilerUrl = process.env.NANOPASS_SERVER_URL ?? 'http://127.0.0.1:18080';
const compilerCommand = process.env.NANOPASS_SERVER_COMMAND ??
  (process.platform === 'win32'
    ? 'build\\compiler\\nano-server.exe --port=18080'
    : 'build/compiler/nano-server --port=18080');
const compilerServer = process.env.NANOPASS_SERVER_URL
  ? []
  : [{
      command: compilerCommand,
      cwd: '..',
      url: `${compilerUrl}/api/v1/health`,
      reuseExistingServer: !process.env.CI,
      timeout: 30_000,
      env: { NANOPASS_ALLOWED_ORIGINS: 'http://127.0.0.1:15173' },
    }];

export default defineConfig({
  testDir: './e2e',
  timeout: 30_000,
  // Keep one reviewed baseline for the supported Chromium rendering path. The
  // previous platform suffix made Linux CI create a new, unreviewed baseline.
  snapshotPathTemplate: '{testDir}/{testFilePath}-snapshots/{arg}{ext}',
  use: { baseURL: 'http://127.0.0.1:15173', browserName: 'chromium', headless: true },
  webServer: [
    ...compilerServer,
    {
      command: 'npm run dev -- --host 127.0.0.1 --port=15173',
      url: 'http://127.0.0.1:15173',
      reuseExistingServer: !process.env.CI,
      timeout: 30_000,
      env: { VITE_NANO_SERVER_URL: compilerUrl },
    },
  ],
});
