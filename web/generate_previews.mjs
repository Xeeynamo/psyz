import { spawn } from 'node:child_process';
import { mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import { extname, join, normalize, resolve } from 'node:path';
import { mkdtemp } from 'node:fs/promises';

import { samples } from './site/samples.js';

const args = new Map();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}
const distDir = resolve(args.get('--dist') || 'build-web/dist');
const chromePath = args.get('--chrome');
if (!chromePath) throw new Error('missing --chrome path');

const mimeTypes = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.png': 'image/png'
};

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url, 'http://127.0.0.1');
    const relative = decodeURIComponent(url.pathname).replace(/^\/+/, '') || 'index.html';
    const path = normalize(join(distDir, relative));
    if (!path.startsWith(`${distDir}/`) && path !== join(distDir, 'index.html')) {
      response.writeHead(403).end();
      return;
    }
    const data = await readFile(path);
    response.writeHead(200, { 'Content-Type': mimeTypes[extname(path)] || 'application/octet-stream' });
    response.end(data);
  } catch (error) {
    response.writeHead(404).end();
  }
});
await new Promise((resolveListen, reject) => {
  server.once('error', reject);
  server.listen(0, '127.0.0.1', resolveListen);
});
const { port } = server.address();

const profileDir = await mkdtemp(join(tmpdir(), 'psyz-preview-chrome-'));
const chrome = spawn(chromePath, [
  '--headless=new',
  '--no-first-run',
  '--no-default-browser-check',
  '--disable-background-timer-throttling',
  '--disable-renderer-backgrounding',
  '--disable-backgrounding-occluded-windows',
  '--force-device-scale-factor=1',
  '--window-size=640,480',
  `--user-data-dir=${profileDir}`,
  '--remote-debugging-pipe',
  'about:blank'
], {
  stdio: ['ignore', 'ignore', 'ignore', 'pipe', 'pipe']
});
const chromeExited = new Promise((resolveExit) => chrome.once('exit', resolveExit));

let nextId = 0;
let pipeBuffer = Buffer.alloc(0);
const pending = new Map();
chrome.stdio[4].on('data', (chunk) => {
  pipeBuffer = Buffer.concat([pipeBuffer, chunk]);
  while (true) {
    const end = pipeBuffer.indexOf(0);
    if (end < 0) break;
    const raw = pipeBuffer.subarray(0, end).toString();
    pipeBuffer = pipeBuffer.subarray(end + 1);
    if (!raw) continue;
    const message = JSON.parse(raw);
    if (message.id && pending.has(message.id)) {
      pending.get(message.id)(message);
      pending.delete(message.id);
    }
  }
});

function send(method, params = {}, sessionId) {
  const id = ++nextId;
  const message = { id, method, params };
  if (sessionId) message.sessionId = sessionId;
  chrome.stdio[3].write(`${JSON.stringify(message)}\0`);
  return new Promise((resolveSend, reject) => {
    const timeout = setTimeout(() => {
      pending.delete(id);
      reject(new Error(`${method} timed out`));
    }, 30000);
    pending.set(id, (reply) => {
      clearTimeout(timeout);
      if (reply.error) reject(new Error(`${method}: ${reply.error.message}`));
      else resolveSend(reply.result);
    });
  });
}

async function evaluate(expression, sessionId) {
  const result = await send('Runtime.evaluate', {
    expression,
    returnByValue: true
  }, sessionId);
  if (result.exceptionDetails) throw new Error(result.exceptionDetails.text);
  return result.result.value;
}

async function waitForFrame(frame, sessionId) {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    try {
      const state = await evaluate(`(() => {
        const canvas = document.getElementById('canvas');
        const status = document.getElementById('status');
        return {
          ready: Boolean(canvas && canvas.width && canvas.height && status && !status.textContent),
          frame: globalThis.__psyzFrameCount || 0
        };
      })()`, sessionId);
      if (state.ready && state.frame >= frame) return;
    } catch (error) {
    }
    await new Promise((resolveWait) => setTimeout(resolveWait, 50));
  }
  throw new Error(`sample did not reach frame ${frame}`);
}

let exitCode = 0;
try {
  const target = await send('Target.createTarget', { url: 'about:blank' });
  const attached = await send('Target.attachToTarget', {
    targetId: target.targetId,
    flatten: true
  });
  const sessionId = attached.sessionId;
  await send('Page.enable', {}, sessionId);
  await send('Emulation.setDeviceMetricsOverride', {
    width: 640,
    height: 480,
    deviceScaleFactor: 1,
    mobile: false
  }, sessionId);
  await mkdir(join(distDir, 'previews'), { recursive: true });

  for (const sample of samples) {
    const url = `http://127.0.0.1:${port}/samples/${sample.id}/index.html?preview=1`;
    await send('Page.navigate', { url }, sessionId);
    await waitForFrame(sample.previewFrame, sessionId);
    await new Promise((resolveWait) => setTimeout(resolveWait, 50));
    const shot = await send('Page.captureScreenshot', {
      format: 'png',
      fromSurface: true,
      captureBeyondViewport: false
    }, sessionId);
    const output = join(distDir, 'previews', `${sample.id}.png`);
    await writeFile(output, Buffer.from(shot.data, 'base64'));
    console.log(`Captured ${sample.id} at frame ${sample.previewFrame}`);
  }
} catch (error) {
  exitCode = 1;
  console.error(`Preview generation failed: ${error.message}`);
} finally {
  chrome.kill('SIGTERM');
  server.close();
  await Promise.race([
    chromeExited,
    new Promise((resolveWait) => setTimeout(resolveWait, 2000))
  ]);
  await rm(profileDir, { recursive: true, force: true });
}
process.exitCode = exitCode;
