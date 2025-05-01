#ifndef COMMANDS_H
#define COMMANDS_H


int cmd_mkfs(const char *fsname, int nbi, int nba);
int cmd_ls(const char *fsname, const char *filename);
int cmd_df(const char *fsname);
int cmd_cp(const char *fsname, const char *filename1, const char *filename2, bool mode1, bool mode2);
int cmd_rm(const char *fsname, const char *filename);
int cmd_lock(const char *fsname, const char *filename, const char *arg);
int cmd_chmod(const char *fsname, const char *filename, const char *arg);
int cmd_cat(const char *fsname, const char *filename);
int cmd_input(const char *fsname, const char *filename);
int cmd_add(const char *fsname, const char *filename_ext, const char *filename_int);
int cmd_addinput(int argc, char *argv[]);
int cmd_fsck(int argc, char *argv[]);
int cmd_mount(int argc, char *argv[]);

#endif
