/**
 * Minimal HTTP client for proxying requests to ESP32 devices.
 * Uses Node's built-in http/https modules to avoid extra dependencies.
 */
const http = require('http');

/**
 * Perform a GET request to the given URL and return the body as string.
 * @param {string} url - Full URL (e.g. http://192.168.0.103/logs.txt)
 * @param {number} [timeout=8000] - Request timeout in ms
 * @returns {Promise<string>}
 */
function httpGet(url, timeout = 8000) {
  return new Promise((resolve, reject) => {
    const parsed = new URL(url);
    const mod = parsed.protocol === 'https:' ? require('https') : http;

    const req = mod.get(url, { timeout }, (res) => {
      let data = '';
      res.on('data', (chunk) => { data += chunk; });
      res.on('end', () => {
        if (res.statusCode < 200 || res.statusCode >= 300) {
          return reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
        }
        resolve(data);
      });
    });

    req.on('error', (err) => reject(err));
    req.on('timeout', () => { req.destroy(); reject(new Error('Request timeout')); });
  });
}

module.exports = { httpGet };