#include <glog/logging.h>
#include "pika_server.h"
#include "pika_command.h"
#include "pika_conf.h"
#include "pika_define.h"
#include "env.h"

PikaConf *g_pika_conf;

PikaServer* g_pika_server;

static void version() {
    printf("-----------Pika server %s ----------\n", kPikaVersion.c_str());
}

static void PikaConfInit(const std::string& path) {
  printf("path : %s\n", path.c_str());
  g_pika_conf = new PikaConf(path);
  if (g_pika_conf->Load() != 0) {
    LOG(FATAL) << "pika load conf error";
  }
  version();
  printf("-----------Pika config list----------\n");
  g_pika_conf->DumpConf();
  printf("-----------Pika config end----------\n");
}

static void PikaGlogInit() {
  if (!slash::FileExists(g_pika_conf->log_path())) {
    slash::CreatePath(g_pika_conf->log_path()); 
  }

  if (!g_pika_conf->daemonize()) {
    FLAGS_alsologtostderr = true;
  }
  FLAGS_log_dir = g_pika_conf->log_path();
  FLAGS_minloglevel = g_pika_conf->log_level();
  FLAGS_max_log_size = 1800;
  ::google::InitGoogleLogging("pika");
}

static void daemonize() {
  if (fork() != 0) exit(0); /* parent exits */
  setsid(); /* create a new session */
}

static void close_std() {
  int fd;
  if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
  }
}

static void create_pid_file(void) {
  /* Try to write the pid file in a best-effort way. */
  std::string path(g_pika_conf->pidfile());

  size_t pos = path.find_last_of('/');
  if (pos != std::string::npos) {
    // mkpath(path.substr(0, pos).c_str(), 0755);
    slash::CreateDir(path.substr(0, pos));
  } else {
    path = kPikaPidFile;
  }

  FILE *fp = fopen(path.c_str(), "w");
  if (fp) {
    fprintf(fp,"%d\n",(int)getpid());
    fclose(fp);
  }
}

static void IntSigHandle(const int sig) {
  DLOG(INFO) << "Catch Signal " << sig << ", cleanup...";
  g_pika_server->Exit();
}

static void PikaSignalSetup() {
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, &IntSigHandle);
  signal(SIGQUIT, &IntSigHandle);
}

static void usage()
{
    fprintf(stderr,
            "Pika module %s\n"
            "usage: pika [-hv] [-c conf/file]\n"
            "\t-h               -- show this help\n"
            "\t-c conf/file     -- config file \n"
            "  example: ./output/bin/pika -c ./conf/pika.conf\n",
            kPikaVersion.c_str()
           );
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    exit(-1);
  }

  bool path_opt = false;
  char c;
  char path[1024];
  while (-1 != (c = getopt(argc, argv, "c:hv"))) {
    switch (c) {
      case 'c':
        snprintf(path, 1024, "%s", optarg);
        path_opt = true;
        break;
      case 'h':
        usage();
        return 0;
      case 'v':
        version();
        return 0;
      default:
        usage();
        return 0;
    }
  }

  if (path_opt == false) {
    fprintf (stderr, "Please specify the conf file path\n" );
    usage();
    exit(-1);
  }

  PikaConfInit(path);

  // daemonize if needed
  if (g_pika_conf->daemonize()) {
    daemonize();
    create_pid_file();
  }


  PikaGlogInit();
  PikaSignalSetup();
  InitCmdInfoTable();

  DLOG(INFO) << "Server at: " << path;
  g_pika_server = new PikaServer();

  if (g_pika_conf->daemonize()) {
    close_std();
  }

  g_pika_server->Start();

  return 0;
}
