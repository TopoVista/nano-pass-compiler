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

test('keeps verified compiler state available across dedicated lab workflows', async ({ page }) => {
  await page.goto('/lab');
  await expect(page.locator('.compile-status')).toContainText('verified');
  await expect(page.getByLabel('Compiler revision')).toHaveText('0.3.0+phase11-dev');

  const terminalCommand = page.getByRole('textbox', { name: 'Terminal command' });
  await terminalCommand.fill('verify');
  await terminalCommand.press('Enter');
  await expect(page.getByText(/PASS · 1 function/)).toBeVisible();

  await page.getByLabel('IR stage').selectOption('ssa');
  await expect(page.getByLabel('IR viewer')).toContainText('func @main');

  await page.getByRole('link', { name: /optimize/i }).click();
  await expect(page).toHaveURL(/\/optimizations$/);
  await expect(page.getByRole('heading', { name: /every rewrite has a reason/i })).toBeVisible();

  await page.getByRole('link', { name: /^graphs/i }).click();
  await expect(page).toHaveURL(/\/graphs$/);
  await expect(page.getByRole('heading', { name: /follow control, not decoration/i })).toBeVisible();

  await page.getByRole('link', { name: /^verify/i }).click();
  await expect(page).toHaveURL(/\/verification$/);
  await expect(page.getByRole('heading', { name: /measure what the compiler can actually prove/i })).toBeVisible();
  await page.getByRole('link', { name: /^workbench/i }).click();
  await expect(page).toHaveURL(/\/lab$/);
  await expect(page.locator('.compile-status')).toContainText('verified');
  await expect(page.getByRole('textbox', { name: 'Terminal command' })).toBeVisible();
});

test('keeps a selected custom pipeline and restores a shared session', async ({ page }) => {
  const pageErrors: string[] = [];
  page.on('pageerror', (error) => pageErrors.push(error.message));
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
  expect(pageErrors).toEqual([]);
});

test('renders the graph viewport without React Flow sizing warnings', async ({ page }) => {
  const graphWarnings: string[] = [];
  page.on('console', (message) => {
    if (message.type() === 'warning' && /React Flow/i.test(message.text())) {
      graphWarnings.push(message.text());
    }
  });

  await page.goto('/graphs');
  await expect(page.getByLabel('cfg graph')).toBeVisible();
  await expect(page.locator('.graph-flow-viewport .react-flow')).toBeVisible();
  expect(graphWarnings).toEqual([]);
});

test('returns a structured compiler diagnostic for malformed source', async ({ page }) => {
  await page.goto('/lab');
  const response = await page.evaluate(async () => {
    const result = await fetch('http://127.0.0.1:18080/api/v1/compile', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        schemaVersion: 1,
        source: 'int main( { return 0; }',
        optimizationLevel: 2,
        policy: 'default',
      }),
    });
    return { status: result.status, body: await result.json() };
  });

  expect(response.status).toBe(400);
  expect(response.body.success).toBe(false);
  expect(response.body.diagnostics[0].severity).toBe('error');
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
