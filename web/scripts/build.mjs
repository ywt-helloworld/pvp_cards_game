import { build } from 'esbuild';
import { mkdir, readFile, writeFile, copyFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('..', import.meta.url));
await mkdir(`${root}/dist/assets`, { recursive: true });
await build({ entryPoints: [`${root}/client/app.js`], bundle: true, minify: true,
  outfile: `${root}/dist/assets/app.js`, target: ['es2020'], format: 'esm' });
await copyFile(`${root}/client/style.css`, `${root}/dist/assets/style.css`);
await writeFile(`${root}/dist/index.html`, await readFile(`${root}/client/index.html`));
console.log('Built web/dist (static HTML, CSS and bundled Socket.IO client)');
