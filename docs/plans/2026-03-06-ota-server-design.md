# OTA Firmware Update Server — Design

## Goal

A Cloudflare Worker + R2 storage service that serves firmware updates to ESP32 devices over HTTP, protected by API key auth. Supports multiple projects from a single deployment.

## Architecture

```
ESP32 device                    Cloudflare Worker              R2 Bucket
    │                                │                            │
    │  GET /project/manifest.json    │                            │
    │  Authorization: Bearer <key>   │                            │
    │ ──────────────────────────────>│  validate API key          │
    │                                │  GET manifest.json ──────> │
    │  { version, url }             │  <────────────────────────  │
    │ <──────────────────────────────│                            │
    │                                │                            │
    │  GET /project/v1.2.0/fw.bin   │                            │
    │  Authorization: Bearer <key>   │                            │
    │ ──────────────────────────────>│  validate API key          │
    │                                │  GET firmware.bin ────────> │
    │  <binary stream>              │  <────────────────────────  │
    │ <──────────────────────────────│                            │
```

## Components

### Cloudflare Worker (`src/index.ts`)

- Receives HTTP requests from ESP32 devices
- Validates `Authorization: Bearer <API_KEY>` header on all requests
- Routes requests by project name: `/<project-slug>/manifest.json` and `/<project-slug>/v<version>/firmware.bin`
- Proxies files from the bound R2 bucket
- Returns proper `Content-Type` and `Content-Length` headers (ESP32 OTA client requires `Content-Length`)

### R2 Bucket

Layout:
```
<project-slug>/
  manifest.json           # { "version": "1.2.0" }
  v1.2.0/firmware.bin
  v1.1.0/firmware.bin     # Previous versions kept for rollback
```

The `manifest.json` per project:
```json
{
  "version": "1.2.0"
}
```

The firmware download URL is constructed by the worker (not stored in manifest), so the manifest only needs the version number. The worker builds the full authed URL path internally.

### Deploy Script (`deploy.sh`)

Usage:
```bash
./deploy.sh <project-slug> <version> <path-to-firmware.bin>
```

Example:
```bash
./deploy.sh big-blue-bus-button 1.2.0 ../Big-Blue-Bus-Button/.pio/build/feather_esp32s3/firmware.bin
```

Steps:
1. Upload `firmware.bin` to R2 at `<project>/<version>/firmware.bin` via Wrangler CLI
2. Update `<project>/manifest.json` with the new version
3. Print confirmation with the manifest URL

### Wrangler Config (`wrangler.toml`)

- Worker name: `ota-server`
- R2 bucket binding: `FIRMWARE_BUCKET`
- Secret: `API_KEY` (set via `wrangler secret put API_KEY`)
- Custom domain (optional): `ota.make3.co` or use the default `*.workers.dev` URL

## ESP32 Client Changes

In the Big Blue Bus Button repo, update `config.h`:
```cpp
static constexpr const char* FIRMWARE_UPDATE_URL = "https://ota-server.<account>.workers.dev/big-blue-bus-button/manifest.json";
```

Add API key header to `ota_updater.cpp` HTTP requests:
```cpp
http.addHeader("Authorization", "Bearer <API_KEY>");
```

The API key will be stored in NVS (set once via serial or captive portal) or compiled into firmware. Since the firmware binary is on a physical device anyway, compiling it in is acceptable.

## Auth Model

- Single API key shared across all projects and devices
- Key stored as a Cloudflare Worker secret (not in code)
- Key compiled into ESP32 firmware in `config.h`
- If a key is compromised, rotate it: update the worker secret + reflash devices

## Error Handling

- Invalid/missing API key: 401 Unauthorized
- Unknown project: 404 Not Found
- Missing manifest or firmware file: 404 Not Found
- R2 read failure: 500 Internal Server Error
- ESP32 client already handles all HTTP error codes gracefully

## Repo Structure

```
ota-server/
  src/
    index.ts          # Cloudflare Worker
  deploy.sh           # Upload firmware + update manifest
  wrangler.toml       # Cloudflare config
  package.json        # Wrangler dependency
  README.md
```

## Cost

All within Cloudflare free tier:
- Workers: 100,000 requests/day free
- R2: 10 GB storage free, 1M reads/month free
- Effectively $0 for this use case
