/* Execute a command until some file changed
 *
 * Currently, this is the command spawned (first argument)
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#include "tino/alarm.h"
#include "tino/proc.h"

static long		mypid;
static const char	*arg0;
static int		tock;

#define	MAXFILES	2000

static int sigs[100];
static struct statcheck
  {
    struct stat	stat;
    const char	*name;
  } statcheck[MAXFILES];

static void
vex(int ret, TINO_VA_LIST list, const char *type, int e)
{
  fprintf(stderr, "[%ld] %s %s: ", mypid, arg0, type);
  tino_vfprintf(stderr, list);
  if (e)
    fprintf(stderr, ": %s\n", strerror(e));
  else
    fprintf(stderr, "\n");
  exit(ret);
}

static void
info(const char *s, ...)
{
  tino_va_list	list;

  fprintf(stderr, "[%ld] %s info: ", mypid, arg0);
  tino_va_start(list, s);
  tino_vfprintf(stderr, &list);
  tino_va_end(list);
  fprintf(stderr, "\n");
}

static void
ex(const char *s, ...)
{
  tino_va_list	list;
  int		e = errno;

  tino_va_start(list, s);
  vex(23, &list, "error", e);
  /* never reached	*/
}

static void
setarg0(const char *s)
{
  char	*tmp;

  mypid	= getpid();
  arg0	= s;
  if ((tmp=strrchr(arg0, '/'))!=0)
    arg0	= tmp+1;
}

static int
tick(void *user, long delta, time_t now, long run)
{
  tock	= 1;
  return 0;
}

static void
statcheck_add(int nr, const char *name)
{
  if (nr >= sizeof statcheck/sizeof *statcheck)
    ex("too many change checks: %d", nr);
  statcheck[nr].name	= name;
  if (stat(name, &statcheck[nr].stat))
    ex("cannot stat: %s", name);
}

static void
sig_add(int sig)
{
  static int nr;

  if (sigs[nr])
    nr++;
  if (nr>=(sizeof sigs/sizeof *sigs)-1)
    ex("too many signal definitions: %d", nr);
  sigs[nr]	= sig;
}

int
main(int argc, char **argv)
{
  pid_t		pid, tmp;
  int		status, result;
  const char	*s;
  int		nr, sig;

  setarg0(argv[0]);

  for (nr=0; --argc>0 && strcmp(*++argv, "--"); )
    if (argv[0][0]=='-')
      sig_add(atoi(argv[0]+1));
    else
      statcheck_add(nr++, argv[0]);
  argv++;
  if (!nr)
    statcheck_add(nr++, argv[0]);
  if (!sigs[0])
    sigs[0]=15;

  if (argc<1)
    {
      fprintf(stderr, "Usage: %s program [args..]\n", arg0);
      return 42;
    }

  pid	= tino_fork_exec_sidE(NULL, 0, argv, NULL, 0, NULL, 1);
  info("[%ld] exec %s", (long)pid, argv[0]);
  if (pid==(pid_t)-1)
    ex("fork %s", argv[0]);

  sig = 0;
  tino_alarm_set(1, tick, NULL);

  while ((tmp=waitpid((pid_t)-1, &status, 0))!=pid)
    {
      struct stat	st;
      int		n;

      TINO_ALARM_RUN();
      if (tmp==(pid_t)-1 && errno!=EINTR)
        ex("wait error");
      if (!tock)
        continue;
      tock	= 0;
      for (n=nr; --n>=0; )
        {
          struct statcheck *p = statcheck+n;

          if (stat(p->name, &st))
            continue;  /* missing files are unknown if changed */
#define	EQ(x)	&& (st.st_##x == p->stat.st_##x)
          if (1 EQ(dev) EQ(ino) EQ(mode) EQ(uid) EQ(gid) EQ(size) EQ(mtime) EQ(ctime))
            continue;
          /* kill the session */
          if (kill(-pid, (sig<sizeof sigs/sizeof *sigs && sigs[sig]) ? sigs[sig] : 9))
            info("[%ld] kill %d failed for %s (changed %s)", (long)pid, sigs[sig], argv[0], p->name);
          else
            info("[%ld] killed %s: changed %s", (long)pid, argv[0], p->name);
          sig++;
          break;
        }
    }

  TINO_ALARM_RUN();
  s	= tino_wait_child_status_string(status, &result);
  info("[%ld] result %s: %s", (long)pid, argv[0], s);

  return status;
}

