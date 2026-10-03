/// @file

#include "noun.h"
#include "vere.h"

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);
}

/* _newt_encode(): synchronous serialization into a single buffer, for test purposes
*/
static c3_y*
_newt_encode(u3_atom mat, c3_w* len_w)
{
  c3_w  met_w = u3r_met(3, mat);
  c3_y* buf_y;

  *len_w = 5 + met_w;
  buf_y  = c3_malloc(*len_w);

  //  write header
  //
  buf_y[0] = 0x0;
  buf_y[1] = ( met_w        & 0xff);
  buf_y[2] = ((met_w >>  8) & 0xff);
  buf_y[3] = ((met_w >> 16) & 0xff);
  buf_y[4] = ((met_w >> 24) & 0xff);

  u3r_bytes(0, met_w, buf_y + 5, mat);
  u3z(mat);

  return buf_y;
}

static c3_w
_moat_length(u3_moat* mot_u)
{
  u3_meat* met_u = mot_u->ext_u;
  c3_w     len_w = 0;

  while ( met_u ) {
    met_u = met_u->nex_u;
    len_w++;
  }

  return len_w;
}

/* _test_newt_smol(): various scenarios with small messages
*/
static void
_test_newt_smol(void)
{
  //  =(2 (jam 0))
  //
  u3_atom     a = u3ke_jam(0);
  u3_moat mot_u;
  c3_w    len_w;
  c3_y*   buf_y;

  memset(&mot_u, 0, sizeof(u3_moat));

  //  one message one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);
    u3_newt_decode(&mot_u, buf_y, len_w);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (a)\n");
      exit(1);
    }
  }

  //  two messages one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    buf_y = c3_realloc(buf_y, 2 * len_w);
    memcpy(buf_y + len_w, buf_y, len_w);
    len_w = 2 * len_w;

    u3_newt_decode(&mot_u, buf_y, len_w);

    if ( 2 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (b)\n");
      exit(1);
    }
  }

  //  one message two buffers
  //
  {
    c3_y* end_y;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    end_y = c3_malloc(1);
    end_y[0] = buf_y[len_w - 1];

    u3_newt_decode(&mot_u, buf_y, len_w - 1);

    if ( 0 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (c)\n");
      exit(1);
    }

    u3_newt_decode(&mot_u, end_y, 1);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (d)\n");
      exit(1);
    }
  }

  //  two messages two buffers (overlapping length)
  //
  {
    c3_y* haf_y;
    c3_w  haf_w, dub_w;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    dub_w = 2 * len_w;
    haf_w = len_w / 2;

    //  buf_y is all of message one, half of message two (not a full length)
    //
    buf_y = c3_realloc(buf_y, dub_w - haf_w);
    memcpy(buf_y + len_w, buf_y, len_w - haf_w);

    //  haf_y is the second half of message two
    //
    haf_y = c3_malloc(haf_w);
    memcpy(haf_y, buf_y + (len_w - haf_w), haf_w);

    u3_newt_decode(&mot_u, buf_y, dub_w - haf_w);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (e)\n");
      exit(1);
    }

    u3_newt_decode(&mot_u, haf_y, haf_w);

    if ( 2 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (f)\n");
      exit(1);
    }
  }

  u3z(a);
}

/* _test_newt_vast(): various scenarios with larger messages
*/
static void
_test_newt_vast(void)
{
  //  =(53 (met 3 (jam "abcdefghijklmnopqrstuvwxyz")))
  //
  u3_atom     a = u3ke_jam(u3i_tape("abcdefghijklmnopqrstuvwxyz"));
  u3_moat mot_u;
  c3_w    len_w;
  c3_y*   buf_y;

  memset(&mot_u, 0, sizeof(u3_moat));

  //  one message one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);
    u3_newt_decode(&mot_u, buf_y, len_w);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (a)\n");
      exit(1);
    }
  }

  //  two messages one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    buf_y = c3_realloc(buf_y, 2 * len_w);
    memcpy(buf_y + len_w, buf_y, len_w);
    len_w = 2 * len_w;

    u3_newt_decode(&mot_u, buf_y, len_w);

    if ( 2 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (b)\n");
      exit(1);
    }
  }

  //  one message many buffers
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    {
      c3_y* cop_y = c3_malloc(len_w);
      c3_w  haf_w = len_w / 2;
      memcpy(cop_y, buf_y, len_w);

      u3_newt_decode(&mot_u, buf_y, haf_w);

      while ( haf_w < len_w ) {
        c3_y* end_y = c3_malloc(1);
        end_y[0] = cop_y[haf_w];

        if ( 0 != _moat_length(&mot_u) ) {
          fprintf(stderr, "newt vast fail (c) %u\n", haf_w);
          exit(1);
        }

        u3_newt_decode(&mot_u, end_y, 1);
        haf_w++;
      }

      c3_free(cop_y);
    }

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (d)\n");
      exit(1);
    }
  }

  //  two messages two buffers
  //
  {
    c3_y* haf_y;
    c3_w  haf_w, dub_w;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    dub_w = 2 * len_w;
    haf_w = len_w / 2;

    //  buf_y is all of message one, half of message two
    //
    buf_y = c3_realloc(buf_y, dub_w - haf_w);
    memcpy(buf_y + len_w, buf_y, len_w - haf_w);

    //  haf_y is the second half of message two
    //
    haf_y = c3_malloc(haf_w);
    memcpy(haf_y, buf_y + (len_w - haf_w), haf_w);

    u3_newt_decode(&mot_u, buf_y, dub_w - haf_w);

    if ( 1 !=  _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (e)\n");
      exit(1);
    }

    u3_newt_decode(&mot_u, haf_y, haf_w);

    if ( 2 !=  _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (f)\n");
      exit(1);
    }
  }

  //  two messages many buffers
  //
  {
    c3_w dub_w;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_w);

    dub_w = 2 * len_w;

    //  buf_y is two copies of message
    //
    buf_y = c3_realloc(buf_y, dub_w);
    memcpy(buf_y + len_w, buf_y, len_w);

    {
      c3_y* cop_y = c3_malloc(dub_w);
      c3_w  haf_w = len_w + 1;
      memcpy(cop_y, buf_y, dub_w);

      u3_newt_decode(&mot_u, buf_y, haf_w);

      while ( haf_w < dub_w ) {
        c3_y* end_y = c3_malloc(1);
        end_y[0] = cop_y[haf_w];

        if ( 1 !=  _moat_length(&mot_u) ) {
          fprintf(stderr, "newt vast fail (g) %u\n", haf_w);
          exit(1);
        }

        u3_newt_decode(&mot_u, end_y, 1);
        haf_w++;
      }

      c3_free(cop_y);
    }

    if ( 2 !=  _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (h)\n");
      exit(1);
    }
  }

  u3z(a);
}

/* _test_newt_hangup(): a reply written after the peer hung up.
**
**   A conn.sock client that sends a command and closes its socket before
**   the response arrives makes the response write fail with EPIPE. The
**   owner's bail function then does what conn.c does: queues one more
**   write (its bail message) and stops the stream, replacing bal_f with a
**   destructor. That queued write fails too; if newt reports it through
**   bal_f, the destructor runs while libuv still owns the handle and again
**   from the close callback, and the runtime later faults inside libuv.
**   The destructor must run exactly once, from the close callback.
*/
static u3_mojo _hup_moj_u;
static c3_w    _hup_fal_w;
static c3_w    _hup_fre_w;

static void
_hup_free(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  _hup_fre_w++;
}

static void
_hup_bail(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  _hup_fal_w++;

  c3_y* buf_y = c3_malloc(1);
  buf_y[0] = 0;
  u3_newt_send(&_hup_moj_u, 1, buf_y);
  u3_newt_mojo_stop(&_hup_moj_u, _hup_free);
}

static void
_test_newt_hangup(void)
{
  uv_loop_t lup_u;
  c3_i      fds_i[2];

  signal(SIGPIPE, SIG_IGN);   //  as the runtime does

  if ( uv_loop_init(&lup_u)
       || socketpair(AF_UNIX, SOCK_STREAM, 0, fds_i)
       || uv_pipe_init(&lup_u, &_hup_moj_u.pyp_u, 0)
       || uv_pipe_open(&_hup_moj_u.pyp_u, fds_i[0]) )
  {
    fprintf(stderr, "newt: hangup: setup failed\r\n");
    exit(1);
  }
  _hup_moj_u.bal_f = _hup_bail;
  _hup_moj_u.ptr_v = 0;

  close(fds_i[1]);            //  the client hangs up

  c3_y* buf_y = c3_malloc(4);
  memcpy(buf_y, "pong", 4);
  u3_newt_send(&_hup_moj_u, 4, buf_y);

  uv_run(&lup_u, UV_RUN_DEFAULT);

  if ( (1 != _hup_fal_w) || (1 != _hup_fre_w) ) {
    fprintf(stderr, "newt: hangup: %u failure call(s), %u destructor call(s); want 1 and 1\r\n",
            _hup_fal_w, _hup_fre_w);
    exit(1);
  }

  if ( uv_loop_close(&lup_u) ) {
    fprintf(stderr, "newt: hangup: loop still has handles\r\n");
    exit(1);
  }
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_newt_smol();
  _test_newt_vast();
  _test_newt_hangup();

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_newt: ok\n");

  return 0;
}
