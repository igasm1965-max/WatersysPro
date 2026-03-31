const { getDefaultConfig } = require('expo/metro-config');
const path = require('path');

const config = getDefaultConfig(__dirname);

// Redirect Node.js `ws` module to a no-op shim so mqtt.min.js uses
// the global React Native WebSocket instead.
config.resolver.extraNodeModules = {
  ...config.resolver.extraNodeModules,
  ws: path.resolve(__dirname, 'ws-shim.js'),
  net: path.resolve(__dirname, 'ws-shim.js'),
  tls: path.resolve(__dirname, 'ws-shim.js'),
};

module.exports = config;
