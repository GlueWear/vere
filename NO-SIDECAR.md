# UDP-backed Lick ports — Theseus no-sidecar transport

**Status: experimental, for review. NOT proposed to upstream urbit/vere yet.**

## What this branch does
Adds a UDP transport mode to the Lick IO driver (`pkg/vere/io/lick.c`) so a host
runtime can carry *guest* ships' Ames/Mesa packets on and off the real network —
with no external helper process.

This eliminates the Node "transport sidecar" that Theseus (a Gall app running
virtual moons/planets inside a host) previously required. Arvo has no way to put a
raw packet on the wire — only the runtime touches the socket, and Ames only carries
the host's own traffic. The sidecar sat on a Lick unix-socket doing the UDP. This
moves that job into the runtime itself.

## How it works
- A Lick port whose name appears in the `LICK_UDP` map binds its **own** `uv_udp_t`
  socket instead of the usual unix pipe. One socket per guest = reliable inbound
  demux (Mesa `%page` replies carry no recipient).
- `%spit [kind lane(s) blob]` is dispatched by `kind`:
  - `%send` — a single Ames lane, resolved through Ames' own resolver
    (`u3_ames_lane_from_noun`, incl. galaxy DNS), sent on the port's socket.
  - `%push` — a list of Mesa lanes, each realised (`u3_mesa_realise_lane`) and sent.
- Inbound datagrams are sniffed (Mesa discriminator `0x67e00200` at byte 4) and
  soaked back as `%heer` (mesa) or `%hear` (ames) carrying `[lane blob]`.

## Config (current)
`LICK_UDP="<port-name>=<udp-port>,..."` — the port name is the agent-prefixed Lick
path with any leading `/` stripped, e.g.
`theseus-pyre/utp/~sondel-baltel-bidlys=39990`.
**TODO before upstream:** promote this env var to a real `--lick-udp` launch flag.

## Robustness change also included
`_lick_init_sock` no longer calls `u3_king_bail()` (which kills the ship) on a
socket-path error — it returns non-fatally and the one bad port is skipped. A stale
socket path used to take the whole ship down.

## Build
Requires **Zig 0.15.2** (0.16.x fails — build.zig.zon format differs).
```
zig build -Doptimize=ReleaseFast   # binary at zig-out/<arch>/urbit
```

## Pairs with
Theseus desk branch **`udp-lick-transport`** (GlueWear/theseus) — the guest-side
policy (one `/utp/<ship>` port per guest, tagged spits, inbound routing). Neither
half is useful without the other.

## Proven live
A moon running under Theseus sent `|hi ~zod` over its own UDP Lick port and got a
reply, with **no sidecar running**. All guest ports bound
(`lick: udp transport … bound 0.0.0.0:399xx`).

## Base
Cut from tag `vere-v4.6` (the version the test host runs). The review PR is against
branch `base-v4.6` (= that tag) so the diff is only these commits.
