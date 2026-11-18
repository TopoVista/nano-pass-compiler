import { expect, test } from '@playwright/test';

test.describe('public compiler story', () => {
  test('keeps the representative transformation controllable and clearly labeled', async ({ page }) => {
    await page.goto('/');

    await expect(page.getByText('static illustration · not a live compiler run')).toBeVisible();
    await page.getByRole('tab', { name: /verify ssa/i }).click();
    await expect(page.getByText('The representative SSA passes its structural check.')).toBeVisible();

    await page.getByRole('button', { name: /ssa optimizer/i }).click();
    await expect(page.getByText(/applies verifier-gated rewrites/i)).toBeVisible();
  });

  test('suppresses decorative animation when reduced motion is requested', async ({ page }) => {
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.goto('/');

    await expect(page.locator('.hero-code-typing').first()).toHaveCSS('animation-name', 'none');
    await expect(page.locator('.hero-code-typing').first()).toHaveCSS('width', /\d+px|auto/);
  });

  test('keeps the build brief readable without horizontal clipping on a narrow screen', async ({ page }) => {
    await page.setViewportSize({ width: 674, height: 900 });
    await page.goto('/');

    const brief = page.locator('.build-blueprint');
    await expect(brief).toBeVisible();
    await expect(page.getByText('Syntax / typed AST')).toBeVisible();
    await expect(page.getByRole('button', { name: /CoreIR normalize/i })).toBeVisible();
    await page.getByRole('button', { name: /CoreIR normalize/i }).click();
    await expect(brief.locator('.blueprint-inspector')).toContainText('Explicit IR');

    const layout = await page.evaluate(() => ({
      documentWidth: document.documentElement.scrollWidth,
      viewportWidth: document.documentElement.clientWidth,
      artifactWidth: document.querySelector('.blueprint-artifact button')?.getBoundingClientRect().width ?? 0,
    }));
    expect(layout.documentWidth).toBeLessThanOrEqual(layout.viewportWidth);
    expect(layout.artifactWidth).toBeGreaterThanOrEqual(44);
  });
});
