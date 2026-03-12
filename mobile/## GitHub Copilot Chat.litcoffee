## GitHub Copilot Chat

- Extension: 0.39.0 (prod)
- VS Code: 1.111.0 (ce099c1ed25d9eb3076c11e4a280f3eb52b4fbeb)
- OS: win32 10.0.26200 x64
- GitHub Account: igasm1965-max

## Network

User Settings:
```json
  "http.systemCertificatesNode": true,
  "github.copilot.advanced.debug.useElectronFetcher": true,
  "github.copilot.advanced.debug.useNodeFetcher": false,
  "github.copilot.advanced.debug.useNodeFetchFetcher": true
```

Environment Variables:
- NO_PROXY=127.0.0.1

Connecting to https://api.github.com:
- DNS ipv4 Lookup: 140.82.121.5 (137 ms)
- DNS ipv6 Lookup: Error (5 ms): getaddrinfo ENOTFOUND api.github.com
- Proxy URL: None (1 ms)
- Electron fetch (configured): HTTP 200 (144 ms)
- Node.js https: HTTP 200 (385 ms)
- Node.js fetch: HTTP 200 (117 ms)

Connecting to https://api.githubcopilot.com/_ping:
- DNS ipv4 Lookup: 140.82.113.22 (172 ms)
- DNS ipv6 Lookup: Error (4 ms): getaddrinfo ENOTFOUND api.githubcopilot.com
- Proxy URL: None (7 ms)
- Electron fetch (configured): HTTP 200 (554 ms)
- Node.js https: HTTP 200 (590 ms)
- Node.js fetch: HTTP 200 (563 ms)

Connecting to https://copilot-proxy.githubusercontent.com/_ping:
- DNS ipv4 Lookup: 20.250.119.64 (95 ms)
- DNS ipv6 Lookup: Error (169 ms): getaddrinfo ENOTFOUND copilot-proxy.githubusercontent.com
- Proxy URL: None (1 ms)
- Electron fetch (configured): HTTP 200 (387 ms)
- Node.js https: HTTP 200 (389 ms)
- Node.js fetch: HTTP 200 (397 ms)

Connecting to https://mobile.events.data.microsoft.com: HTTP 404 (563 ms)
Connecting to https://dc.services.visualstudio.com: HTTP 404 (725 ms)
Connecting to https://copilot-telemetry.githubusercontent.com/_ping: HTTP 200 (601 ms)
Connecting to https://copilot-telemetry.githubusercontent.com/_ping: HTTP 200 (576 ms)
Connecting to https://default.exp-tas.com: HTTP 400 (574 ms)

Number of system certificates: 151

## Documentation

In corporate networks: [Troubleshooting firewall settings for GitHub Copilot](https://docs.github.com/en/copilot/troubleshooting-github-copilot/troubleshooting-firewall-settings-for-github-copilot).