# UDP-backed Lick ports — Theseus no-sidecar transport

**Status: experimental; runs the Theseus host `~siglup-narwet`. Not proposed to
upstream urbit/vere yet.** In this fork, `main` is this work and `develop`
mirrors upstream.

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
- A Theseus guest port (`/theseus-pyre/utp/~<ship>`, exactly one path segment
  after `utp/`) binds its **own** `uv_udp_t` socket instead of the usual unix
  pipe. One socket per guest = reliable inbound demux (Mesa `%page` replies
  carry no recipient). By default the OS assigns the UDP port; the runtime logs
  it as `lick: udp transport <name> bound 0.0.0.0:<port>`.
- Any port named in the `LICK_UDP` map binds that fixed UDP port instead; a
  mapping takes precedence over automatic allocation. All other Lick paths keep
  Unix-socket behavior.
- UDP setup failures (init, bind e.g. `EADDRINUSE`, recv start, getsockname) are
  logged and the port is skipped, never reassigned to another port and never
  fatal to the host. Arvo is not notified; later spits log `gen ... not found`.
- `%spit [kind lane(s) blob]` is dispatched by `kind`:
  - `%send` — a single Ames lane, resolved through Ames' own resolver
    (`u3_ames_lane_from_noun`, incl. galaxy DNS), sent on the port's socket.
  - `%push` — a list of Mesa lanes, each realised (`u3_mesa_realise_lane`) and sent.
- Inbound datagrams are sniffed (Mesa discriminator `0x67e00200` at byte 4) and
  soaked back as `%heer` (mesa) or `%hear` (ames) carrying `[lane blob]`.

## Per-guest STUN

A stock ship keeps its NAT mapping open, and learns its public address, by
running STUN from its Ames socket to its sponsoring galaxy; that is what lets a
galaxy relay packets to a ship behind NAT. Guest sockets need the same, or
relayed traffic (from the host, its sponsor star, or any ship that first
reaches the guest through a galaxy) never arrives.

- The guest's virtual Ames gives its sponsorship chain as `%saxo`; Theseus
  passes it on as `%spit [%saxo sponsors]` on the guest's port.
- The runtime picks the terminal galaxy, resolves it through the host's Ames,
  and sends STUN binding requests from the guest's own socket: every 25 seconds
  once established, retrying with backoff while trying, and restarting 5
  seconds after a failed cycle. It also answers binding requests arriving on
  that socket.
- Results are soaked back to the guest with mark `%stun` in the same shape a
  stock Ames driver produces (`%once` on first success, `%stop`, `%fail`, with
  the galaxy and the discovered lane); Theseus injects them into the guest's
  Ames as its `%stun` task.
- Closing the port (`%shut`) stops the guest's STUN timer with its socket.

## Config (current)
No configuration is needed for new guests. To pin a guest to a fixed port (for
example a router port-forward), set
`LICK_UDP="<port-name>=<udp-port>,..."` — the port name is the agent-prefixed Lick
path with any leading `/` stripped, e.g.
`theseus-pyre/utp/~sondel-baltel-bidlys=39990`.
Automatic ports are ephemeral: a runtime restart re-binds guests on new ports,
and inbound reachability depends on outbound traffic opening NAT mappings, as
for an ordinary ship on a dynamic port.
**TODO before upstream:** promote this env var to a real `--lick-udp` launch flag.

## Robustness change also included
`_lick_init_sock` no longer calls `u3_king_bail()` (which kills the ship) on a
socket-path error — it returns non-fatally and the one bad port is skipped. A stale
socket path used to take the whole ship down.

## Build
Requires **Zig 0.15.2** (0.16.x fails — build.zig.zon format differs).
```
zig build -Doptimize=ReleaseFast                    # binary at zig-out/<arch>/urbit
zig build lick-test ames-test newt-test -Doptimize=ReleaseFast
```
The build is reproducible: rebuilding this commit on aarch64 macOS gives
SHA-256 `a5b8bdccb9983e9241c17d11324273f4926d74a6f20c04b68bf0184557f4e1b8`,
the runtime installed on `~siglup-narwet` since 2026-10-02 23:17. The version
string still reads `4.6-8ddc4b7` (the base commit); identify builds by hash.

`lick-test` covers allocation and teardown accounting (including port
structs), automatic ports (distinct, idempotent, released), fixed-port
precedence and `EADDRINUSE` handling, and the STUN lifecycle.

## Pairs with
Theseus (GlueWear/theseus, `main`; developed on `udp-lick-transport`) — the guest-side
policy (one `/utp/<ship>` port per guest, tagged spits, inbound routing). Neither
half is useful without the other.

## Proven live
A moon running under Theseus sent `|hi ~zod` over its own UDP Lick port and got a
reply, with **no sidecar running**. With automatic ports and STUN, moons have
also reached their host and its sponsor star on the same network (both
previously unreachable), and a moon installed `%landscape` over the network.

## Base
Cut from tag `vere-v4.6` (the version the test host runs). The review PR is against
branch `base-v4.6` (= that tag) so the diff is only these commits.
