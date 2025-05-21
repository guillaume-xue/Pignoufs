#ifndef COMMANDS_H
#define COMMANDS_H

#include "../include/structures.h"
#include "../include/utilitaires.h"
#include "../include/cmd_add.h"
#include "../include/cmd_addinput.h"
#include "../include/cmd_cat.h"
#include "../include/cmd_chmod.h"
#include "../include/cmd_cp.h"
#include "../include/cmd_df.h"
#include "../include/cmd_find.h"
#include "../include/cmd_fsck.h"
#include "../include/cmd_grep.h"
#include "../include/cmd_input.h"
#include "../include/cmd_lock.h"
#include "../include/cmd_ls.h"
#include "../include/cmd_mkfs.h"
#include "../include/cmd_mkdir.h"
#include "../include/cmd_mv.h"
#include "../include/cmd_rm.h"
#include "../include/cmd_rmdir.h"
#include "../include/cmd_tree.h"

int commands(int argc, char *argv[]);

#endif
