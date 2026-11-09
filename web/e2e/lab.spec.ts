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

test('presents the animated guide with inspectable compiler stages and boundaries', async ({ page }) => {
  await page.goto('/guide');
  await expect(page.getByRole('heading', { name: /a compiler lab, presented as a system/i })).toBeVisible();
  await page.getByRole('tab', { name: /CFG show possible paths/i }).click();
  await expect(page.getByText(/Basic blocks and arrows make every route/i)).toBeVisible();
  await page.getByRole('button', { name: /Request guard/i }).click();
  await expect(page.getByText(/Untrusted source never becomes a host command/i)).toBeVisible();
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

test('keeps the public and lab shells inside the viewport at supported breakpoints', async ({ page }) => {
  const breakpoints = [
    { width: 375, height: 812 },
    { width: 390, height: 844 },
    { width: 768, height: 1024 },
    { width: 1024, height: 900 },
    { width: 1440, height: 980 },
    { width: 1920, height: 1080 },
  ];

  for (const viewport of breakpoints) {
    await page.setViewportSize(viewport);
    await page.goto('/');
    await expect(page.getByRole('heading', { name: /a compiler you can interrogate/i })).toBeVisible();
    expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), `landing at ${viewport.width}px`).toBe(true);

    await page.goto('/lab');
    await expect(page.getByRole('textbox', { name: 'Terminal command' })).toBeVisible();
    const labBounds = await page.evaluate(() => {
      const oversized = [...document.querySelectorAll<HTMLElement>('*')]
        .map((element) => ({ element: element.className || element.tagName, parent: element.parentElement?.className || element.parentElement?.tagName, right: element.getBoundingClientRect().right }))
        .filter(({ right }) => right > window.innerWidth + 1)
        .slice(0, 8);
      return { scrollWidth: document.documentElement.scrollWidth, viewport: window.innerWidth, oversized };
    });
    expect(labBounds.scrollWidth <= labBounds.viewport, `lab at ${viewport.width}px: ${JSON.stringify(labBounds)}`).toBe(true);
  }
});
