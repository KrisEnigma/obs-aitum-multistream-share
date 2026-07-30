# Aitum Multistream — Encoder Share Fork

Fork of [Aitum/obs-aitum-multistream](https://github.com/Aitum/obs-aitum-multistream) that adds **peer encoder sharing** between destinations.

Upstream Aitum can only:

1. Reuse OBS **main** stream encoder, or
2. Create a **new** encode session per destination

This fork adds a third option: **Share from &lt;other destination&gt;** — same encoded bitstream, extra RTMP push only.

## Target topology

| Role | Platform | Encode |
|------|----------|--------|
| OBS main | YouTube 1440p @ ~40 Mbps | OBS built-in (Manage Broadcast / scheduled streams) |
| Shared secondary | Twitch + Kick 1080p @ 7000 kbps | **One** NVENC session |
| Shared tertiary (optional) | TikTok + Vertical YouTube 9:16 @ 7000 kbps | **One** session via Vertical + share |

Result: **2 encodes for 3 platforms** (or 3 with vertical), never duplicate encodes for identical quality.

## Setup (core case)

1. Install this plugin **instead of** stock Aitum Multistream (same plugin id — do not run both).
2. OBS **Settings → Stream → YouTube** (Connect Account) at 1440p / 40 Mbps.
3. Multistream → add **Twitch** → Advanced → Video Encoder: NVENC H.264, scale 1920×1080, 7000 kbps. Same for audio (AAC). **Save / close settings** so Twitch is stored as a shareable owner.
4. Re-open settings → **Kick** → Advanced → Video Encoder: **Share from Twitch**. Audio Encoder: **Share from Twitch**.
5. Start OBS main (YouTube), then start Twitch and Kick from the Multistream dock (order does not matter; the shared encoder is created from Twitch’s saved settings on first use).

> Tip: “Share from …” only lists destinations that already have a **dedicated** encoder (not Main Encoder). Configure the owner first, then the sharer.

## How sharing works

Encoder field values use a sentinel:

```text
share:<owner_output_name>
```

At start, the plugin:

1. Resolves the owner destination’s encoder settings (follows share chains, rejects cycles / Main-Encoder owners).
2. Looks up `aitum_multi_video_encoder_<owner>` / `aitum_multi_audio_encoder_<owner>`.
3. Creates that encoder once if missing; otherwise reuses the live instance.

## Build

Same as upstream. See [README.md](README.md) for in-tree / out-of-tree OBS plugin builds.

```bash
cmake -S . -B build -DBUILD_OUT_OF_TREE=On
cmake --build build
```

## Upstream

- Base: Aitum Multistream 1.0.8
- Branch: `feature/peer-encoder-share`
- Remote `upstream` → https://github.com/Aitum/obs-aitum-multistream.git

GPL-2.0 — same license as upstream. Credit to Aitum / Exeldro for the original plugin.
