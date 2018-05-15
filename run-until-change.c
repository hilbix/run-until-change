/* Execute a command until some file changed
 *
 * Currently, this is the command spawned (first argument)
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#include "tino/alarm.h"
#include "tino/proc.h"

#include "run-until-change_version.h"

static long		mypid;
static const char	*arg0;
static int		tock;
static pid_t		child;

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

#define	SIG(X,Y)	void X (void) { if (child) kill(-child, Y); TINO_SIGNAL(SIG##X, X); }
#define	SIG1(X)		SIG(sig_##X,X)
#define	SIG2(X,Y)	SIG(sig_##X,Y)

SIG1(SIGHUP  )
SIG1(SIGINT  )
SIG1(SIGQUIT )
SIG2(SIGILL  ,SIGTERM)
SIG2(SIGTRAP ,SIGTERM)
SIG2(SIGABRT ,SIGTERM)
SIG2(SIGBUS  ,SIGTERM)
SIG2(SIGFPE  ,SIGTERM)
/* SIG1(SIGKILL ) impossible */
SIG1(SIGUSR1 )
SIG2(SIGSEGV ,SIGTERM)
SIG1(SIGUSR2 )
SIG1(SIGTERM )
SIG1(SIGCONT )
/* SIG1(SIGSTOP ) impossible */
SIG1(SIGTSTP )
SIG1(SIGURG  )
SIG1(SIGWINCH)
SIG1(SIGPWR  )

int
main(int argc, char **argv)
{
  pid_t		tmp;
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
  if (argc<2)
    {
      fprintf(stderr, "Usage: %s [-signal..] files_to_check.. -- program [args..]\n", arg0);
      fprintf(stderr, "\t\tVersion " RUN_UNTIL_CHANGE_VERSION " compiled " __DATE__ "\n");
      fprintf(stderr, "\tfiles_to_check defaults to 'program' if none given.\n");
      fprintf(stderr, "\t'program' is terminated by sending signals to it each second:\n");
      fprintf(stderr, "\tdefault 15 or given list, followed by 9 if list is exhausted.\n");
      return 42;
    }

  if (!nr)
    statcheck_add(nr++, argv[0]);
  if (!sigs[0])
    sigs[0]=15;

#define	PASS(X) TINO_SIGACTION(X, sig_##X); TINO_SIGNAL(X, sig_##X)

  PASS(SIGHUP  );
  PASS(SIGINT  );
  PASS(SIGQUIT );
  PASS(SIGILL  );
  PASS(SIGTRAP );
  PASS(SIGABRT );
  PASS(SIGBUS  );
  PASS(SIGFPE  );
  PASS(SIGUSR1 );
  PASS(SIGSEGV );
  PASS(SIGUSR2 );
  PASS(SIGTERM );
  PASS(SIGCONT );
  PASS(SIGTSTP );
  PASS(SIGURG  );
  PASS(SIGWINCH);
  PASS(SIGPWR  );

  child	= tino_fork_exec_sidE(NULL, 0, argv, NULL, 0, NULL, 1);
  info("[%ld] exec %s", (long)child, argv[0]);
  if (child==(pid_t)-1)
    ex("fork %s", argv[0]);

  sig = 0;
  tino_alarm_set(1, tick, NULL);

  while ((tmp=waitpid((pid_t)-1, &status, 0))!=child)
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
          info("[%ld] changed %s", (long)child, p->name);
          /* kill the session */
          if (kill(-child, (sig<sizeof sigs/sizeof *sigs && sigs[sig]) ? sigs[sig] : 9))
            info("[%ld] failed kill %s: signal %d", (long)child, argv[0], sigs[sig]);
          else
            info("[%ld] kill %s: signal %d", (long)child, argv[0], sigs[sig]);
          sig++;
          break;
        }
    }
  child	= 0;

  TINO_ALARM_RUN();
  s	= tino_wait_child_status_string(status, &result);
  info("[%ld] result %s: %s", (long)tmp, argv[0], s);

  return status;
}

