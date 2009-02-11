#ifndef GIT_EXEC_CMD_H
#define GIT_EXEC_CMD_H

extern void git_set_argv_exec_path(const char *exec_path);
extern void git_set_argv0_path(const char *path);
extern const char* git_exec_path(void);
extern void setup_path(void);
extern const char **prepare_git_cmd(const char **argv);
extern int execv_git_cmd(const char **argv); /* NULL terminated */
extern int execl_git_cmd(const char *cmd, ...);
extern const char *system_path(const char *path);

extern int enable_git_repo_exec_path;

#endif /* GIT_EXEC_CMD_H */
