#ifndef COMMANDS_H
#define COMMANDS_H


int cmd_mkfs(const char *fsname, int nbi, int nba);
int cmd_ls(const char *fsname, const char *filename);
int cmd_df(const char *fsname);
int cmd_cp(int argc, char *argv[]);
int cmd_rm(int argc, char *argv[]);
int cmd_lock(int argc, char *argv[]);
int cmd_chmod(int argc, char *argv[]);
int cmd_cat(const char *fsname, const char *filename);
int cmd_input(int argc, char *argv[]);
int cmd_add(const char *fsname, const char *filename_ext, const char *filename_int);
int cmd_addinput(int argc, char *argv[]);
int cmd_fsck(int argc, char *argv[]);
int cmd_mount(int argc, char *argv[]);

#endif
