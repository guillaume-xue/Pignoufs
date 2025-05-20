// Déclarations de fonctions de test
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include "../include/commands.h"
#include "../include/structures.h"

#define TEST_FS "test_cp_fs"
#define TEST_FILE1 "file1.txt"
#define TEST_FILE2 "file2.txt"
#define TEST_FILE3 "file3.txt"
#define TEST_FILE_EXT "external.txt"
#define TEST_FILE_EXT2 "external2.txt"
#define TEST_FILE_OUT "output.txt"

void write_external_file(const char *filename, const char *content)
{
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  write(fd, content, strlen(content));
  close(fd);
}

int read_file_content(const char *filename, char *buf, size_t buflen)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
    return -1;
  int n = read(fd, buf, buflen - 1);
  if (n >= 0)
    buf[n] = 0;
  close(fd);
  return n;
}

int main()
{
  printf("==> Test de la fonction cmd_mkfs\n");
  const char *fsname_mkfs = "test_fs";
  int nbi_mkfs = 10, nba_mkfs = 20;

  int ret_mkfs = cmd_mkfs(fsname_mkfs, nbi_mkfs, nba_mkfs);
  if (ret_mkfs != 0)
  {
    printf("[FAIL] cmd_mkfs retourne %d\n", ret_mkfs);
  }
  else
  {
    printf("[OK] cmd_mkfs retourne 0\n");
  }

  int fd_mkfs = open(fsname_mkfs, O_RDONLY);
  if (fd_mkfs < 0)
  {
    printf("[FAIL] Le fichier système n'a pas été créé\n");
  }
  else
  {
    printf("[OK] Le fichier système a été créé\n");
    struct pignoufs sb;
    ssize_t r = read(fd_mkfs, &sb, sizeof(sb));
    if (r < 8)
    {
      printf("[FAIL] Lecture du superbloc impossible\n");
    }
    else if (strncmp(sb.magic, "pignoufs", 8) != 0)
    {
      printf("[FAIL] Magic string incorrecte: %.8s\n", sb.magic);
    }
    else
    {
      printf("[OK] Magic string correcte\n");
    }
    close(fd_mkfs);
  }

  unlink(fsname_mkfs); // Supprimer le fichier de test

  printf("=== Test cmd_df ===\n");
  const char *fsname_df = "test_df";
  int nbi_df = 5, nba_df = 10;

  unlink(fsname_df);
  if (cmd_mkfs(fsname_df, nbi_df, nba_df) != 0)
  {
    printf("[FAIL] mkfs pour df\n");
    return 1;
  }

  int ret_df = cmd_df(fsname_df);
  if (ret_df != 0)
  {
    printf("[FAIL] cmd_df retourne %d\n", ret_df);
  }
  else
  {
    printf("[OK] cmd_df retourne 0\n");
  }

  unlink(fsname_df);

  // printf("==> Test cmd_cp\n");
  // unlink(TEST_FS);
  // unlink(TEST_FILE_EXT);
  // unlink(TEST_FILE_EXT2);
  // unlink(TEST_FILE_OUT);

  // // 1. Create FS
  // if (cmd_mkfs(TEST_FS, 10, 20) != 0)
  // {
  //   printf("[FAIL] mkfs failed\n");
  //   return 1;
  // }

  // // 2. Ajouter un fichier externe: external.txt → file1.txt
  // const char *content = "Hello, world!\n";
  // write_external_file(TEST_FILE_EXT, content);
  // if (cmd_add(TEST_FS, TEST_FILE_EXT, TEST_FILE1) != 0)
  // {
  //   printf("[FAIL] cmd_add failed\n");
  //   return 1;
  // }

  // // 3. Interne→interne copie: file1.txt → file2.txt
  // if (cmd_cp(TEST_FS, TEST_FILE1, TEST_FILE2, true, true, false) != 0)
  // {
  //   printf("[FAIL] cmd_cp internal→internal failed\n");
  //   return 1;
  // }

  // // 4. Vérifier le contenu de file2.txt
  // char buf[256] = {0};
  // int fd_cp = open(TEST_FS, O_RDONLY);
  // if (fd_cp < 0)
  // {
  //   printf("[FAIL] open fs for cat\n");
  //   return 1;
  // }
  // close(fd_cp);
  // // copier le contenu de file2.txt dans output.txt
  // if (cmd_cp(TEST_FS, TEST_FILE2, TEST_FILE_OUT, true, false, false) != 0)
  // {
  //   printf("[FAIL] cmd_cp internal→external failed\n");
  //   return 1;
  // }
  // memset(buf, 0, sizeof(buf));
  // if (read_file_content(TEST_FILE_OUT, buf, sizeof(buf)) < 0)
  // {
  //   printf("[FAIL] read output.txt failed\n");
  //   return 1;
  // }
  // if (strcmp(buf, content) != 0)
  // {
  //   printf("[FAIL] file2.txt content mismatch: '%s'\n", buf);
  //   return 1;
  // }
  // else
  // {
  //   printf("[OK] internal→internal and internal→external copy content OK\n");
  // }

  // // 5. Externe→interne copie: external2.txt → file3.txt
  // const char *content2 = "Another test line.\n";
  // write_external_file(TEST_FILE_EXT2, content2);
  // if (cmd_cp(TEST_FS, TEST_FILE_EXT2, TEST_FILE3, false, true, false) != 0)
  // {
  //   printf("[FAIL] cmd_cp external→internal failed\n");
  //   return 1;
  // }
  // // copier le contenu de file3.txt dans output.txt
  // if (cmd_cp(TEST_FS, TEST_FILE3, TEST_FILE_OUT, true, false, false) != 0)
  // {
  //   printf("[FAIL] cmd_cp file3.txt→output.txt failed\n");
  //   return 1;
  // }
  // memset(buf, 0, sizeof(buf));
  // if (read_file_content(TEST_FILE_OUT, buf, sizeof(buf)) < 0)
  // {
  //   printf("[FAIL] read output.txt (file3) failed\n");
  //   return 1;
  // }
  // if (strcmp(buf, content2) != 0)
  // {
  //   printf("[FAIL] file3.txt content mismatch: '%s'\n", buf);
  //   return 1;
  // }
  // else
  // {
  //   printf("[OK] external→internal copy content OK\n");
  // }

  // // nettoyer
  // unlink(TEST_FS);
  // unlink(TEST_FILE_EXT);
  // unlink(TEST_FILE_EXT2);
  // unlink(TEST_FILE_OUT);
  printf("=== Test cmd_rm ===\n");
  const char *fsname_rm = "test_rm_fs";
  const char *file_int = "to_delete";
  const char *file_ext = "rm_external.txt";
  const char *content = "Fichier à supprimer\n";

  // Nettoyage préalable
  unlink(fsname_rm);
  unlink(file_ext);

  // 1. Créer le FS
  if (cmd_mkfs(fsname_rm, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour rm\n");
    return;
  }

  // 2. Créer un fichier externe et l'ajouter dans le FS
  write_external_file(file_ext, content);
  if (cmd_add(fsname_rm, file_ext, file_int) != 0)
  {
    printf("[FAIL] cmd_add pour rm\n");
    unlink(fsname_rm);
    unlink(file_ext);
    return;
  }

  // 3. Vérifier que le fichier existe (ls ou cat)
  printf("Vérification présence fichier avant suppression :\n");
  int ret_cat = cmd_cat(fsname_rm, file_int);
  if (ret_cat != 0)
  {
    printf("[FAIL] Le fichier n'existe pas avant suppression\n");
    unlink(fsname_rm);
    unlink(file_ext);
    return;
  }

  // 4. Supprimer le fichier
  if (cmd_rm(fsname_rm, file_int) != 0)
  {
    printf("[FAIL] cmd_rm retourne erreur\n");
    unlink(fsname_rm);
    unlink(file_ext);
    return;
  }

  // 5. Vérifier que le fichier n'existe plus
  printf("Vérification absence fichier après suppression :\n");
  ret_cat = cmd_cat(fsname_rm, file_int);
  if (ret_cat == 0)
  {
    printf("[FAIL] Le fichier existe encore après suppression\n");
  }
  else
  {
    printf("[OK] cmd_rm supprime bien le fichier\n");
  }

  // Nettoyage
  unlink(fsname_rm);
  unlink(file_ext);

  return 0;
}