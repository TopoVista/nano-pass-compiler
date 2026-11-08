import { expect, test } from '@playwright/test';

test('uses the public landing page to enter the dashboard workbench', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('heading', { name: /a compiler you can interrogate/i })).toBeVisible();
  await Promise.all([
    page.waitForURL(/\/lab$/),
    page.getByRole('link', { name: /open compiler lab/i }).click(),
  ]);
  await expect(page.getByRole('navigation', { name: 'Lab views' })).toBeVisible();
  await expect(page.getByRole('link', { name: 'Workbench' })).toBeVisible();
});

test('inspects compiler stages, graphs, execution profile, and adaptive events', async ({ page }) => {
  await page.goto('/lab');
  await expect(page.locator('.compile-status')).toContainText('verified');
  await expect(page.getByLabel('Compiler revision')).toHaveText('0.3.0+phase11-dev');

  const terminalCommand = page.getByRole('textbox', { name: 'Terminal command' });
  await terminalCommand.fill('verify');
  await terminalCommand.press('Enter');
  await expect(page.getByText(/PASS · 1 function/)).toBeVisible();

  await page.getByLabel('IR stage').selectOption('ssa');
  await expect(page.getByLabel('IR viewer')).toContainText('func @main');
  await page.getByRole('button', { name: /Nano SSA Optimization @main/ }).click();
  await expect(page.getByText('Before / after')).toBeVisible();

  await page.getByLabel('Graph kind').selectOption('dominator-tree');
  await expect(page.getByLabel('dominator-tree graph')).toBeVisible();
  await page.getByLabel('Graph kind').selectOption('cfg');
  await page.getByText('bb1.then', { exact: true }).click();

  await page.getByRole('button', { name: 'ssa', exact: true }).click();
  await expect(page.getByText(/^\d+ calls$/)).toBeVisible();

  await page.getByLabel('Example program').selectOption('Adaptive JIT');
  await expect(page.locator('.compile-status')).toContainText('verified');
  await page.getByRole('button', { name: 'adaptive-jit', exact: true }).click();
  await expect(page.getByText('Adaptive JIT events')).toBeVisible();
});

test('keeps a selected custom pipeline and restores a shared session', async ({ page }) => {
  await page.goto('/lab');
  await expect(page.locator('.compile-status')).toContainText('verified');

  await page.locator('.pipeline-picker summary').click();
  await page.locator('.pipeline-options label', { hasText: 'sccp' }).locator('input').check();
  await page.locator('.pipeline-options label', { hasText: 'dce' }).locator('input').check();
  await expect(page.getByLabel('Custom pipeline order')).toBeVisible();
  await page.getByRole('button', { name: 'Move dce earlier' }).click();

  await expect(page.locator('.compile-status')).toContainText('verified');
  await page.getByRole('button', { name: 'Share session' }).click();
  await expect(page).toHaveURL(/#session=/);
  await page.reload();
  await expect(page.locator('.pipeline-picker summary')).toContainText('2 selected');
});

test('shows a useful compiler diagnostic for malformed source', async ({ page }) => {
  await page.goto('/lab');
  const editor = page.locator('.monaco-editor textarea');
  await editor.click();
  await page.keyboard.press(process.platform === 'darwin' ? 'Meta+A' : 'Control+A');
  await page.keyboard.insertText('int main( { return 0; }');
  await expect(page.locator('.compile-status')).toContainText('diagnostics reported');
  await expect(page.getByText(/error\[/i).first()).toBeVisible();
});

test('preserves an operable lab shell on mobile and explains an unavailable API', async ({ browser }) => {
  const mobile = await browser.newContext({ viewport: { width: 390, height: 844 } });
  const page = await mobile.newPage();
  await page.route('**/api/v1/**', (route) => route.abort());
  await page.goto('/lab');
  await expect(page.getByRole('navigation', { name: 'Lab views' })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Retry compiler service' })).toBeVisible();
  await expect(page.getByRole('textbox', { name: 'Terminal command' })).toBeVisible();
  await mobile.close();
});
