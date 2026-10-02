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
#undef c3_free
#define c3_malloc(size) _test_malloc(size)
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
    _expect_live(2, "two owned port names");

    _lick_ef_shut(&lic_u, _path("last"));
    _expect_live(2, "shut releases lookup, retains closing port name");
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(1, "close callback releases last port name");
    if ( !lic_u.gen_u || lic_u.gen_u->nex_u ) {
      fprintf(stderr, "lick: shut corrupted the port list\n");
      exit(1);
    }

    _lick_ef_shut(&lic_u, _path("first"));
    _expect_live(1, "shut first lookup");
    uv_run(&loop, UV_RUN_DEFAULT);
    _expect_live(0, "all port names released");
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
    _expect_live(2, "rebound guest and other port names");

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
  fprintf(stderr, "lick: allocation tests passed\n");
  return 0;
}
