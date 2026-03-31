// Shim for Node.js modules (ws, net, tls) not available in React Native.
// mqtt/dist/mqtt.min.js uses the global WebSocket directly and does not need these.
module.exports = {};
