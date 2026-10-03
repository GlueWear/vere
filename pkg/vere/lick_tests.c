/// @file

#include "vere.h"
#include "noun.h"

/* Track Lick's C allocations independently of the noun loom and libuv. */
static void* _live[16];
static size_t _live_count;

static void*
_test_malloc(size_t size)
{
  void* ptr = malloc(size);
  if ( !ptr || _live_count == 16 ) {
    fprintf(stderr, "lick: allocation tracker exhausted\n");
    exit(1);
  }
  _live[_live_count++] = ptr;
  return ptr;
}

static void
_test_free(void* ptr)
{
  for ( size_t i = 0; i < _live_count; i++ ) {
    if ( _live[i] == ptr ) {
      _live[i] = _live[--_live_count];
      break;
    }
  }
  free(ptr);
}

#undef c3_malloc
#undef c3_calloc
#undef c3_free
#define c3_malloc(size) _test_malloc(size)
#define c3_calloc(size) memset(_test_malloc(size), 0, (size))
#define c3_free(ptr) _test_free(ptr)
#include "io/lick.c"

static void
_expect_live(size_t expected, const char* test)
{
  if ( expected != _live_count ) {
    fprintf(stderr, "lick: %s: expected %zu live allocations, got %zu\n",
            test, expected, _live_count);
    exit(1);
  }
}

static void
_expect_uv(int result)
{
  if ( result ) {
    fprintf(stderr, "lick: libuv: %s\n", uv_strerror(result));
    exit(1);
  }
}

static u3_noun
_path(const char* name)
{
  return u3nc(u3i_string(name), u3_nul);
}

static void
_test_spit(void)
{
  u3_lick lic_u = {0};
  u3_port gen_u = {0};
  gen_u.nam_c = "/test";
  gen_u.udp_o = c3y;
  lic_u.gen_u = &gen_u;

  for ( size_t i = 0; i < 10000; i++ ) {
    // Empty lane list dispatches through UDP without sending network traffic.
    _lick_ef_spit(&lic_u, _path("test"), u3nt(c3__push, u3_nul, 0));
    _expect_live(0, "spit existing port");
    _lick_ef_spin(&lic_u, _path("test"));
    _expect_live(0, "spin existing port");
  }
}

static void
_test_missing_spit(void)
{
  u3_lick lic_u = {0};
  _lick_ef_spit(&lic_u, _path("missing"), 0);
  _expect_live(0, "spit missing port");
}

static void
_test_missing_shut(void)
{
  u3_lick lic_u = {0};
  for ( size_t i = 0; i < 10000; i++ ) {
    _lick_ef_shut(&lic_u, _path("missing"));
    _expect_live(0, "shut missing port");
  }
}

static u3_port*
_port(uv_loop_t* loop, const char* name)
{
  u3_port* gen_u = c3_calloc(sizeof(*gen_u));
  gen_u->nam_c = _lick_it_path(_path(name));
  gen_u->udp_o = c3y;
  gen_u->liv_o = c3y;
  gen_u->sun_u.ini_o = c3n;
  gen_u->sun_u.wok_o = c3n;
  _expect_uv(uv_udp_init(loop, &gen_u->wax_u));
  gen_u->wax_u.data = gen_u;
  return gen_u;
}

static void
_test_shut(void)
{
  uv_loop_t loop;
  _expect_uv(uv_loop_init(&loop));
  for ( size_t i = 0; i < 100; i++ ) {
    u3_lick lic_u = {0};
    lic_u.gen_u = _port(&loop, "first");
    lic_u.gen_u->nex_u = _port(&loop, "last");
    _expect_live(4, "two owned ports");

    _lick_ef_shut(&lic_u, _path("last"));
    _expect_live(4, "shut releases lookup, retains closing port");
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(2, "close callback releases one port");
    if ( !lic_u.gen_u || lic_u.gen_u->nex_u ) {
      fprintf(stderr, "lick: shut corrupted the port list\n");
      exit(1);
    }

    _lick_ef_shut(&lic_u, _path("first"));
    _expect_live(2, "shut first lookup");
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(0, "all ports released");
    if ( lic_u.gen_u ) {
      fprintf(stderr, "lick: shut did not empty the port list\n");
      exit(1);
    }
  }
  _expect_uv(uv_loop_close(&loop));
}

static struct sockaddr_in
_bind_port(u3_port* gen_u)
{
  struct sockaddr_in addr;
  int len = sizeof(addr);
  _expect_uv(uv_ip4_addr("127.0.0.1", 0, &addr));
  _expect_uv(uv_udp_bind(&gen_u->wax_u, (struct sockaddr*)&addr, 0));
  _expect_uv(uv_udp_getsockname(&gen_u->wax_u, (struct sockaddr*)&addr, &len));
  return addr;
}

static void
_test_bound_shut(void)
{
  uv_loop_t loop;
  _expect_uv(uv_loop_init(&loop));
  for ( size_t i = 0; i < 100; i++ ) {
    u3_lick lic_u = {0};
    u3_port* guest = _port(&loop, "guest");
    u3_port* other = _port(&loop, "other");
    struct sockaddr_in guest_addr = _bind_port(guest);
    struct sockaddr_in other_addr = _bind_port(other);
    lic_u.gen_u = guest;
    guest->nex_u = other;

    // The address really is occupied before shut.
    uv_udp_t rival;
    _expect_uv(uv_udp_init(&loop, &rival));
    int err = uv_udp_bind(&rival, (struct sockaddr*)&guest_addr, 0);
    if ( UV_EADDRINUSE != err ) {
      fprintf(stderr, "lick: expected occupied guest port, got %d\n", err);
      exit(1);
    }
    uv_close((uv_handle_t*)&rival, NULL);
    uv_run(&loop, UV_RUN_DEFAULT);

    _lick_ef_shut(&lic_u, _path("guest"));
    _lick_ef_shut(&lic_u, _path("guest"));
    if ( other != lic_u.gen_u ) {
      fprintf(stderr, "lick: closing guest removed the other port\n");
      exit(1);
    }

    // Reuse the same address before the old handle's close callback runs.
    // This covers a quick kill/re-init without freeing libuv-owned memory early.
    u3_port* replacement = _port(&loop, "guest");
    _expect_uv(uv_udp_bind(&replacement->wax_u,
                           (struct sockaddr*)&guest_addr, 0));
    replacement->nex_u = lic_u.gen_u;
    lic_u.gen_u = replacement;
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(4, "rebound guest and other ports");

    _expect_uv(uv_udp_init(&loop, &rival));
    err = uv_udp_bind(&rival, (struct sockaddr*)&other_addr, 0);
    if ( UV_EADDRINUSE != err ) {
      fprintf(stderr, "lick: other guest lost its binding, got %d\n", err);
      exit(1);
    }
    uv_close((uv_handle_t*)&rival, NULL);
    _lick_ef_shut(&lic_u, _path("other"));
    _lick_ef_shut(&lic_u, _path("guest"));
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(0, "bound ports closed");
    if ( lic_u.gen_u ) {
      fprintf(stderr, "lick: closed bound ports remain linked\n");
      exit(1);
    }
  }
  _expect_uv(uv_loop_close(&loop));
}

static void
_test_automatic_udp(void)
{
  uv_loop_t loop;
  _expect_uv(uv_loop_init(&loop));
  uv_loop_t* previous = u3L;
  u3L = &loop;
  u3_lick lic_u = {0};
  if ( c3y != _lick_udp_guest("/theseus-pyre/utp/~sampel-siglup-narwet") ||
       c3n != _lick_udp_guest("/other/utp/~zod") ||
       c3n != _lick_udp_guest("/theseus-pyre/utp/~") ||
       c3n != _lick_udp_guest("/theseus-pyre/utp/~zod/extra") ) {
    fprintf(stderr, "lick: automatic UDP namespace check failed\n");
    exit(1);
  }
  const char* first = "theseus-pyre/utp/~sampel-siglup-narwet";
  const char* second = "theseus-pyre/utp/~sondel-siglup-narwet";
  for ( size_t i = 0; i < 100; i++ ) {
    _lick_ef_spin(&lic_u, _path(first));
    _lick_ef_spin(&lic_u, _path(second));
    _lick_ef_spin(&lic_u, _path(first));
    if ( !lic_u.gen_u || !lic_u.gen_u->nex_u || lic_u.gen_u->nex_u->nex_u ) {
      fprintf(stderr, "lick: automatic ports not unique/idempotent\n");
      exit(1);
    }
    struct sockaddr_in a, b;
    int len = sizeof(a);
    _expect_uv(uv_udp_getsockname(&lic_u.gen_u->wax_u, (struct sockaddr*)&a, &len));
    len = sizeof(b);
    _expect_uv(uv_udp_getsockname(&lic_u.gen_u->nex_u->wax_u, (struct sockaddr*)&b, &len));
    if ( !a.sin_port || !b.sin_port || a.sin_port == b.sin_port ) {
      fprintf(stderr, "lick: automatic port allocation failed\n");
      exit(1);
    }
    _lick_ef_shut(&lic_u, _path(first));
    _lick_ef_shut(&lic_u, _path(second));
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(0, "automatic UDP teardown");
  }
  u3L = previous;
  _expect_uv(uv_loop_close(&loop));
}

static u3_noun
_saxo_chain(c3_d our_d, c3_y dad_y)
{
  return u3nc(u3i_chub(our_d),
         u3nc(u3i_chub(0x123456),
         u3nc(u3i_chub(0x2345),
         u3nc(u3i_word(dad_y), u3_nul))));
}

static void
_test_stun_lifecycle(void)
{
  uv_loop_t loop;
  _expect_uv(uv_loop_init(&loop));
  uv_loop_t* previous = u3L;
  u3L = &loop;

  u3_lick lic_u = {0};
  const char* guest = "theseus-pyre/utp/~sampel-siglup-narwet";
  _lick_ef_spin(&lic_u, _path(guest));
  u3_port* gen_u = lic_u.gen_u;

  if ( !gen_u || c3y != gen_u->sun_u.ini_o ) {
    fprintf(stderr, "lick: STUN timer was not initialized\n");
    exit(1);
  }

  _lick_ef_spit(&lic_u, _path(guest),
                u3nc(c3__saxo, _saxo_chain(0x123456789ULL, 115)));
  if ( (115 != gen_u->sun_u.dad_y) ||
       (LICK_STUN_KEEPALIVE != gen_u->sun_u.sat_y) ||
       !uv_is_active((uv_handle_t*)&gen_u->sun_u.tim_u) ) {
    fprintf(stderr, "lick: valid sponsorship chain did not start STUN\n");
    exit(1);
  }

  {
    u3_lane lane_u = { .pip_w = 0x7f000001, .por_s = 13337 };
    u3_noun actual = _lick_stun_card(gen_u, c3__once, lane_u);
    u3_noun dev = u3nc(u3i_string("theseus-pyre"),
                  u3nc(u3i_string("utp"),
                  u3nc(u3i_string("~sampel-siglup-narwet"), u3_nul)));
    u3_noun expect = u3nt(
      c3__soak,
      dev,
      u3nc(c3__stun,
      u3nt(c3__once, u3i_word(115),
           u3nc(c3n, u3_ames_encode_lane(lane_u)))));
    if ( c3n == u3r_sing(actual, expect) ) {
      fprintf(stderr, "lick: STUN soak card has the wrong noun shape\n");
      exit(1);
    }
    u3z(actual);
    u3z(expect);
  }

  gen_u->sun_u.sef_u = (u3_lane){ .pip_w = 1, .por_s = 2 };
  gen_u->sun_u.wok_o = c3y;
  _lick_ef_spit(&lic_u, _path(guest),
                u3nc(c3__saxo, _saxo_chain(0x123456789ULL, 144)));
  if ( (144 != gen_u->sun_u.dad_y) ||
       gen_u->sun_u.sef_u.pip_w || gen_u->sun_u.sef_u.por_s ||
       (c3n != gen_u->sun_u.wok_o) ) {
    fprintf(stderr, "lick: sponsor change did not reset STUN lane state\n");
    exit(1);
  }

  _lick_ef_spit(&lic_u, _path(guest), u3nc(c3__saxo, u3i_word(42)));
  if ( 144 != gen_u->sun_u.dad_y ) {
    fprintf(stderr, "lick: malformed sponsorship chain changed STUN state\n");
    exit(1);
  }

  _lick_ef_spit(&lic_u, _path(guest),
                u3nc(c3__saxo, _saxo_chain(1, 144)));
  if ( LICK_STUN_OFF != gen_u->sun_u.sat_y ) {
    fprintf(stderr, "lick: galaxy guest incorrectly started STUN\n");
    exit(1);
  }

  _lick_ef_spit(&lic_u, _path(guest),
                u3nc(c3__saxo, _saxo_chain(0x123456789ULL, 115)));
  _lick_ef_shut(&lic_u, _path(guest));
  uv_run(&loop, UV_RUN_DEFAULT);
  _expect_live(0, "armed STUN teardown");
  if ( lic_u.gen_u ) {
    fprintf(stderr, "lick: STUN port remained linked after shutdown\n");
    exit(1);
  }

  u3L = previous;
  _expect_uv(uv_loop_close(&loop));
}

static c3_s
_udp_port(uv_udp_t* wax_u)
{
  struct sockaddr_in add_u;
  int len = sizeof(add_u);
  _expect_uv(uv_udp_getsockname(wax_u, (struct sockaddr*)&add_u, &len));
  return ntohs(add_u.sin_port);
}

static void
_test_fixed_udp(void)
{
  uv_loop_t loop;
  _expect_uv(uv_loop_init(&loop));
  uv_loop_t* previous = u3L;
  u3L = &loop;

  //  Hold one port so a fixed mapping to it must fail to bind, and find a
  //  second port that is currently free for a mapping that must succeed.
  struct sockaddr_in any_u;
  _expect_uv(uv_ip4_addr("0.0.0.0", 0, &any_u));
  uv_udp_t blk_u, tmp_u;
  _expect_uv(uv_udp_init(&loop, &blk_u));
  _expect_uv(uv_udp_bind(&blk_u, (const struct sockaddr*)&any_u, 0));
  _expect_uv(uv_udp_init(&loop, &tmp_u));
  _expect_uv(uv_udp_bind(&tmp_u, (const struct sockaddr*)&any_u, 0));
  c3_s busy = _udp_port(&blk_u);
  c3_s free_s = _udp_port(&tmp_u);
  uv_close((uv_handle_t*)&tmp_u, NULL);
  uv_run(&loop, UV_RUN_DEFAULT);

  const char* first = "theseus-pyre/utp/~sampel-siglup-narwet";
  const char* second = "theseus-pyre/utp/~sondel-siglup-narwet";
  c3_c map_c[256];
  snprintf(map_c, sizeof(map_c), "%s=%u,%s=%u", first, free_s, second, busy);
  u3_lick lic_u = {0};
  lic_u.uce_c = map_c;

  for ( size_t i = 0; i < 100; i++ ) {
    //  A fixed mapping takes precedence over automatic allocation.
    _lick_ef_spin(&lic_u, _path(first));
    if ( !lic_u.gen_u || c3y != lic_u.gen_u->udp_o ||
         free_s != _udp_port(&lic_u.gen_u->wax_u) ) {
      fprintf(stderr, "lick: fixed UDP mapping not honored\n");
      exit(1);
    }
    //  A fixed port that cannot bind is skipped, not reassigned or fatal.
    _lick_ef_spin(&lic_u, _path(second));
    if ( lic_u.gen_u->nex_u ) {
      fprintf(stderr, "lick: failed fixed UDP bind left a port\n");
      exit(1);
    }
    _lick_ef_spit(&lic_u, _path(second), u3nt(c3__push, u3_nul, 0));
    _lick_ef_shut(&lic_u, _path(second));
    _lick_ef_shut(&lic_u, _path(first));
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(0, "fixed UDP mapping");
  }
  uv_close((uv_handle_t*)&blk_u, NULL);
  uv_run(&loop, UV_RUN_DEFAULT);
  u3L = previous;
  _expect_uv(uv_loop_close(&loop));
}

int
main(int argc, char* argv[])
{
  u3m_init(1 << 22);
  u3m_pave(c3y);
  _test_spit();
  _test_missing_spit();
  _test_missing_shut();
  _test_shut();
  _test_bound_shut();
  _test_automatic_udp();
  _test_stun_lifecycle();
  _test_fixed_udp();
  fprintf(stderr, "lick: allocation tests passed\n");
  return 0;
}
