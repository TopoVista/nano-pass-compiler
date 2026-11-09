import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';

function publicSiteUrl(value: string | undefined): string {
  if (!value) return '';
  try {
    const url = new URL(value);
    return url.protocol === 'https:' ? url.origin : '';
  } catch {
    return '';
  }
}

function generatedPublicFiles(siteUrl: string) {
  const routes = ['/', '/lab', '/guide', '/graphs', '/optimizations', '/verification'];
  const robots = [
    'User-agent: *',
    'Allow: /',
    ...(siteUrl ? [`Sitemap: ${siteUrl}/sitemap.xml`] : []),
    '',
  ].join('\n');
  const sitemap = siteUrl
    ? `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n${routes.map((route) => `  <url><loc>${siteUrl}${route}</loc></url>`).join('\n')}\n</urlset>\n`
    : '';
  return { robots, sitemap };
}

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '');
  const siteUrl = publicSiteUrl(env.VITE_SITE_URL);
  const canonical = siteUrl ? `${siteUrl}/` : '/';
  const socialImage = siteUrl ? `${siteUrl}/nanopass-og.svg` : '/nanopass-og.svg';
  const publicFiles = generatedPublicFiles(siteUrl);

  return {
    plugins: [
      react(),
      {
        name: 'nanopass-public-metadata',
        transformIndexHtml(html) {
          return html
            .replaceAll('__NANOPASS_CANONICAL__', canonical)
            .replaceAll('__NANOPASS_SOCIAL_IMAGE__', socialImage);
        },
        generateBundle() {
          this.emitFile({ type: 'asset', fileName: 'robots.txt', source: publicFiles.robots });
          if (publicFiles.sitemap) {
            this.emitFile({ type: 'asset', fileName: 'sitemap.xml', source: publicFiles.sitemap });
          }
        },
      },
    ],
    server: { port: 5173, host: '127.0.0.1' },
    test: { environment: 'jsdom', setupFiles: ['./src/test/setup.ts'], globals: true, exclude: ['e2e/**', 'node_modules/**'] },
  };
});
