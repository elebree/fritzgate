import { svelte } from '@sveltejs/vite-plugin-svelte';
import { readFile, writeFile } from 'fs/promises';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { gzipSync } from 'zlib';

const configDir = dirname(fileURLToPath(import.meta.url));

function toHex(
  data: Buffer,
  linePrefix: string = '    ',
  columns: number = 16,
  delimiter: string = ',',
  numberPrefix: string = '0x'
): string {
  let result = '';
  for (let i = 0; i < data.length; i++) {
    result += `${i > 0 ? delimiter : ''}${i % columns ? ' ' : (i == 0 ? '' : '\r\n') + linePrefix}${numberPrefix}${data[i].toString(16).padStart(2, '0')}`;
  }
  return result;
}

function indexHtmlHeaderPlugin() {
  return {
    name: 'index-html-header',
    closeBundle: async (): Promise<void> => {
      const indexHtml = await readFile(resolve(configDir, 'dist/index.html'));
      const compressedIndexHtml = gzipSync(indexHtml);
      const header = [
        '// This file was generated automatically, do not replace content.\r\n\r\n',
        '#pragma once\r\n\r\n',
        '#define index_html_length sizeof(index_html)\r\n',
        'static const byte index_html[] PROGMEM = {\r\n',
        toHex(compressedIndexHtml),
        '\r\n};\r\n',
      ].join('');

      await writeFile(resolve(configDir, '../src/index_html.h'), header);
    }
  };
}

// https://vitejs.dev/config/
export default defineConfig(({ mode }) => ({
  plugins: [
    svelte(),
    viteSingleFile(),
    mode === 'firmware' ? indexHtmlHeaderPlugin() : undefined,
  ],
}));
