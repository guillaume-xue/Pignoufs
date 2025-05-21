// Déclarations de fonctions de test
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include "../include/commands.h"
#include "../include/structures.h"

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

void TEST_MKFS()
{
  printf("=== Test de la fonction cmd_mkfs ===\n");
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

  unlink(fsname_mkfs);
}

void TEST_LS()
{
  printf("=== Test cmd_ls ===\n");
  const char *fsname = "test_ls_fs";
  const char *file1 = "ls_file1";
  const char *file2 = "ls_file2";
  const char *content = "ls test\n";

  // Nettoyage préalable
  unlink(fsname);
  unlink(file1);
  unlink(file2);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour ls\n");
    return;
  }

  // 2. Créer et ajouter des fichiers
  write_external_file(file1, content);
  write_external_file(file2, content);
  if (cmd_add(fsname, file1, file1) != 0 ||
      cmd_add(fsname, file2, file2) != 0)
  {
    printf("[FAIL] cmd_add pour fichiers ls\n");
    unlink(fsname);
    return;
  }

  // 4. Test ls racine
  printf("Contenu racine (ls):\n");
  cmd_ls(fsname, NULL, NULL);

  // 5. Test ls racine détaillé
  printf("Contenu racine (ls -l):\n");
  cmd_ls(fsname, NULL, "-l");

  // Nettoyage
  unlink(fsname);
  unlink(file1);
  unlink(file2);
}

void TEST_CAT()
{
  printf("=== Test cmd_cat ===\n");
  const char *fsname = "test_cat_fs";
  const char *file_ext = "cat_external.txt";
  const char *file_int = "cat_file";
  const char *content = "Contenu pour test cat\n";

  // Nettoyage préalable
  unlink(fsname);
  unlink(file_ext);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour cat\n");
    return;
  }

  // 2. Créer un fichier externe
  write_external_file(file_ext, content);

  // 3. Ajouter le fichier externe dans le FS
  if (cmd_add(fsname, file_ext, file_int) != 0)
  {
    printf("[FAIL] cmd_add pour cat\n");
    unlink(fsname);
    unlink(file_ext);
    return;
  }

  // 4. Utiliser cmd_cat pour afficher le contenu du fichier interne
  printf("Affichage attendu : %s", content);
  printf("Affichage cmd_cat : ");
  int ret = cmd_cat(fsname, file_int);
  if (ret != 0)
  {
    printf("[FAIL] cmd_cat retourne %d\n", ret);
  }
  else
  {
    printf("[OK] cmd_cat affiche le contenu sans erreur\n");
  }

  // Nettoyage
  unlink(fsname);
  unlink(file_ext);
}

void TEST_FIND()
{
  printf("=== Test cmd_find ===\n");
  const char *fsname = "test_find_fs";
  const char *file1 = "file1";
  const char *file2 = "file2";
  const char *file3 = "file3";
  const char *dir1 = "dir1";
  const char *file_in_dir = "dirA/inside.txt";
  const char *content = "find test\n";

  // Nettoyage préalable
  unlink(fsname);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour find\n");
    return;
  }

  // 2. Créer et ajouter des fichiers
  write_external_file(file1, content);
  write_external_file(file2, content);
  write_external_file(file3, content);

  if (cmd_add(fsname, file1, file1) != 0 ||
      cmd_add(fsname, file2, file2) != 0 ||
      cmd_add(fsname, file3, file3) != 0)
  {
    printf("[FAIL] cmd_add pour fichiers find\n");
    unlink(fsname);
    unlink(file1);
    unlink(file2);
    unlink(file3);
    return;
  }

  // 3. Créer un dossier et un fichier dedans
  if (cmd_mkdir(fsname, dir1) != 0)
  {
    printf("[FAIL] cmd_mkdir pour find\n");
    unlink(file1);
    unlink(file2);
    unlink(file3);
    unlink(fsname);
    return;
  }
  write_external_file(file_in_dir, content);
  if (cmd_add(fsname, file_in_dir, file_in_dir) != 0)
  {
    printf("[FAIL] cmd_add fichier dans dossier\n");
    unlink(fsname);
    unlink(file1);
    unlink(file2);
    unlink(file3);
    return;
  }

  // 4. Test find par nom
  printf("Recherche par nom 'file1':\n");
  cmd_find(fsname, NULL, "file1", 0, NULL);

  // 5. Test find par type fichier
  printf("Recherche type fichier (f):\n");
  cmd_find(fsname, "f", NULL, 0, NULL);

  // 6. Test find par type dossier
  printf("Recherche type dossier (d):\n");
  cmd_find(fsname, "d", NULL, 0, NULL);

  // Nettoyage
  unlink(fsname);
  unlink(file1);
  unlink(file2);
  unlink(file3);
  unlink(file_in_dir);
}

void TEST_CHMOD()
{
  printf("=== Test cmd_chmod ===\n");
  const char *fsname = "test_chmod_fs";
  const char *file_ext = "chmod_external.txt";
  const char *file_int = "chmod_file";
  const char *content = "Test chmod\n";

  // Nettoyage préalable
  unlink(fsname);
  unlink(file_ext);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour chmod\n");
    return;
  }

  // 2. Créer un fichier externe
  write_external_file(file_ext, content);

  // 3. Ajouter le fichier externe dans le FS
  if (cmd_add(fsname, file_ext, file_int) != 0)
  {
    printf("[FAIL] cmd_add pour chmod\n");
    unlink(fsname);
    unlink(file_ext);
    return;
  }

  // 4. Enlever le droit de lecture
  if (cmd_chmod(fsname, file_int, "-r") != 0)
  {
    printf("[FAIL] cmd_chmod -r a échoué\n");
    unlink(fsname);
    unlink(file_ext);
    return;
  }

  // 5. Vérifier que la lecture échoue
  int ret = cmd_cat(fsname, file_int);
  if (ret == 0)
  {
    printf("[FAIL] cmd_cat devrait échouer sans droit de lecture\n");
  }
  else
  {
    printf("[OK] cmd_cat échoue sans droit de lecture\n");
  }

  // 6. Remettre le droit de lecture
  if (cmd_chmod(fsname, file_int, "+r") != 0)
  {
    printf("[FAIL] cmd_chmod +r a échoué\n");
    unlink(fsname);
    unlink(file_ext);
    return;
  }

  // 7. Vérifier que la lecture fonctionne
  ret = cmd_cat(fsname, file_int);
  if (ret != 0)
  {
    printf("[FAIL] cmd_cat devrait réussir avec droit de lecture\n");
  }
  else
  {
    printf("[OK] cmd_cat fonctionne avec droit de lecture\n");
  }

  // Nettoyage
  unlink(fsname);
  unlink(file_ext);
}

void TEST_ADD()
{
  printf("=== Test cmd_add ===\n");
  const char *fsname = "test_add_fs";
  const char *file_ext = "add_external.txt";
  const char *file_int = "added_file";
  const char *content = "Contenu pour test add\n";

  // Nettoyage préalable
  unlink(fsname);
  unlink(file_ext);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour add\n");
    return;
  }

  // 2. Créer un fichier externe
  write_external_file(file_ext, content);

  // 3. Ajouter le fichier externe dans le FS
  if (cmd_add(fsname, file_ext, file_int) != 0)
  {
    printf("[FAIL] cmd_add a échoué\n");
    unlink(fsname);
    unlink(file_ext);
    return;
  }

  // 4. Exporter le fichier interne vers un fichier externe temporaire pour vérifier le contenu
  int ret = cmd_cat(fsname, file_int) != 0;

  if (ret != 0)
  {
    printf("[FAIL] cmd_cat retourne %d\n", ret);
    return;
  }
  else
  {
    printf("[OK] cmd_cat affiche le contenu sans erreur\n");
  }

  printf("[OK] cmd_add a réussi\n");

  // Nettoyage
  unlink(fsname);
  unlink(file_ext);
}

void TEST_INPUT()
{
  printf("=== Test cmd_input ===\n");
  const char *fsname = "test_input_fs";
  const char *file_int = "input_file";
  const char *content = "Contenu via stdin pour test input\n";
  const char *tmp_input = "tmp_input.txt";

  // Nettoyage préalable
  unlink(fsname);
  unlink(tmp_input);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour input\n");
    return;
  }

  // 2. Créer un fichier temporaire avec le contenu à injecter via stdin
  write_external_file(tmp_input, content);

  // 3. Rediriger stdin depuis le fichier temporaire et appeler cmd_input
  int saved_stdin = dup(STDIN_FILENO);
  int fd_input = open(tmp_input, O_RDONLY);
  if (fd_input < 0)
  {
    printf("[FAIL] ouverture tmp_input.txt\n");
    unlink(fsname);
    unlink(tmp_input);
    return;
  }
  dup2(fd_input, STDIN_FILENO);

  int ret = cmd_input(fsname, file_int);

  // Restaurer stdin
  dup2(saved_stdin, STDIN_FILENO);
  close(saved_stdin);
  close(fd_input);

  if (ret != 0)
  {
    printf("[FAIL] cmd_input a échoué\n");
    unlink(fsname);
    unlink(tmp_input);
    return;
  }

  // 4. Vérifier le contenu avec cmd_cat
  printf("Affichage attendu : %s", content);
  printf("Affichage cmd_cat : ");
  int ret_cat = cmd_cat(fsname, file_int);
  if (ret_cat != 0)
  {
    printf("[FAIL] cmd_cat retourne %d\n", ret_cat);
  }
  else
  {
    printf("[OK] cmd_input a bien écrit le contenu depuis stdin\n");
  }

  // Nettoyage
  unlink(fsname);
  unlink(tmp_input);
}

void TEST_ADDINPUT()
{
  printf("=== Test cmd_addinput ===\n");
  const char *fsname = "test_addinput_fs";
  const char *file_int = "input_file";
  const char *content = "Contenu via stdin pour test addinput\n";
  const char *tmp_input = "tmp_input.txt";
  const char *file_out = "addinput_output.txt";

  // Nettoyage préalable
  unlink(fsname);
  unlink(tmp_input);
  unlink(file_out);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour addinput\n");
    return;
  }

  // 2. Créer un fichier temporaire avec le contenu à injecter via stdin
  write_external_file(tmp_input, content);

  // 3. Rediriger stdin depuis le fichier temporaire et appeler cmd_addinput
  int saved_stdin = dup(STDIN_FILENO);
  int fd_input = open(tmp_input, O_RDONLY);
  if (fd_input < 0)
  {
    printf("[FAIL] ouverture tmp_input.txt\n");
    unlink(fsname);
    unlink(tmp_input);
    return;
  }
  dup2(fd_input, STDIN_FILENO);

  int ret = cmd_addinput(fsname, file_int);

  // Restaurer stdin
  dup2(saved_stdin, STDIN_FILENO);
  close(saved_stdin);
  close(fd_input);

  if (ret != 0)
  {
    printf("[FAIL] cmd_addinput a échoué\n");
    unlink(fsname);
    unlink(tmp_input);
    return;
  }

  // 4. Exporter le fichier interne vers un fichier externe temporaire pour vérifier le contenu

  int ret_cat = (cmd_cat(fsname, file_int) != 0);

  if (ret_cat != 0)
  {
    printf("[FAIL] cmd_cat retourne %d\n", ret);
    return;
  }
  else
  {
    printf("[OK] cmd_cat affiche le contenu sans erreur\n");
  }

  printf("[OK] cmd_addinput a réussi\n");

  // Nettoyage
  unlink(fsname);
  unlink(tmp_input);
  unlink(file_out);
}

void TEST_DF()
{
  printf("=== Test cmd_df ===\n");
  const char *fsname_df = "test_df";
  int nbi_df = 5, nba_df = 10;

  unlink(fsname_df);
  if (cmd_mkfs(fsname_df, nbi_df, nba_df) != 0)
  {
    printf("[FAIL] mkfs pour df\n");
    return;
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
}

void TEST_CP()
{
  const char *TEST_FS = "test_cp_fs";
  const char *TEST_FILE_EXT = "external.txt";
  const char *TEST_FILE_EXT2 = "external2.txt";
  const char *TEST_FILE1 = "file";
  const char *TEST_FILE2 = "file";
  const char *TEST_FILE3 = "file";
  const char *TEST_FILE_OUT = "output.txt";

  printf("==> Test cmd_cp\n");
  unlink(TEST_FS);
  unlink(TEST_FILE_EXT);
  unlink(TEST_FILE_EXT2);
  unlink(TEST_FILE_OUT);

  // 1. Create FS
  if (cmd_mkfs(TEST_FS, 10, 20) != 0)
  {
    printf("[FAIL] mkfs failed\n");
    return;
  }

  // 2. Ajouter un fichier externe: external.txt -> file1
  const char *content = "Hello, world!\n";
  write_external_file(TEST_FILE_EXT, content);
  if (cmd_add(TEST_FS, TEST_FILE_EXT, TEST_FILE1) != 0)
  {
    printf("[FAIL] cmd_add failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FS);
    return;
  }

  // 3. Interne -> interne copie: file1 -> file2
  if (cmd_cp(TEST_FS, TEST_FILE1, TEST_FILE2, true, true) != 0)
  {
    printf("[FAIL] cmd_cp internal -> internal failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FS);
    return;
  }

  if (cmd_cat(TEST_FS, TEST_FILE2) != 0)
  {
    printf("[FAIL] cmd_cat file2 failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FS);
    return;
  }

  // 4. Vérifier le contenu de file2
  char buf[256] = {0};
  // copier le contenu de file2 dans output.txt (interne -> externe)
  if (cmd_cp(TEST_FS, TEST_FILE2, TEST_FILE_OUT, true, false) != 0)
  {
    printf("[FAIL] cmd_cp internal -> external failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FS);
    return;
  }
  memset(buf, 0, sizeof(buf));
  if (read_file_content(TEST_FILE_OUT, buf, sizeof(buf)) < 0)
  {
    printf("[FAIL] read output.txt failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FS);
    return;
  }
  if (strcmp(buf, content) != 0)
  {
    printf("[FAIL] file2 content mismatch: '%s'\n", buf);
    unlink(TEST_FILE_EXT);
    unlink(TEST_FS);
    return;
  }
  else
  {
    printf("[OK] internal→internal and internal -> external copy content OK\n");
  }

  // 5. Externe -> interne copie: external2.txt -> file3.txt
  const char *content2 = "Another test line.\n";
  write_external_file(TEST_FILE_EXT2, content2);
  if (cmd_cp(TEST_FS, TEST_FILE_EXT2, TEST_FILE3, false, true) != 0)
  {
    printf("[FAIL] cmd_cp external -> internal failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FILE_EXT2);
    unlink(TEST_FS);
    return;
  }
  // copier le contenu de file3 dans output.txt
  if (cmd_cp(TEST_FS, TEST_FILE3, TEST_FILE_OUT, true, false) != 0)
  {
    printf("[FAIL] cmd_cp file3 -> output.txt failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FILE_EXT2);
    unlink(TEST_FS);
    return;
  }
  memset(buf, 0, sizeof(buf));
  if (read_file_content(TEST_FILE_OUT, buf, sizeof(buf)) < 0)
  {
    printf("[FAIL] read output.txt (file3) failed\n");
    unlink(TEST_FILE_EXT);
    unlink(TEST_FILE_EXT2);
    unlink(TEST_FS);
    return;
  }
  if (strcmp(buf, content2) != 0)
  {
    printf("[FAIL] file3 content mismatch: '%s'\n", buf);
    unlink(TEST_FILE_EXT);
    unlink(TEST_FILE_EXT2);
    unlink(TEST_FS);
    return;
  }
  else
  {
    printf("[OK] external -> internal copy content OK\n");
  }

  // nettoyer
  unlink(TEST_FS);
  unlink(TEST_FILE_EXT);
  unlink(TEST_FILE_EXT2);
  unlink(TEST_FILE_OUT);
}

void TEST_GREP()
{
  printf("=== Test cmd_grep ===\n");
  const char *fsname = "test_grep_fs";
  const char *file1 = "alpha";
  const char *file2 = "beta";
  const char *file3 = "gamma";
  const char *pattern = "motif";
  const char *content1 = "Ceci est un fichier avec motif.\n";
  const char *content2 = "Pas de motif ici.\n";
  const char *content3 = "motif au début\nEt encore motif\n";

  // Nettoyage préalable
  unlink(fsname);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour grep\n");
    return;
  }

  // 2. Créer et ajouter des fichiers
  write_external_file(file1, content1);
  write_external_file(file2, content2);
  write_external_file(file3, content3);

  if (cmd_add(fsname, file1, file1) != 0 ||
      cmd_add(fsname, file2, file2) != 0 ||
      cmd_add(fsname, file3, file3) != 0)
  {
    printf("[FAIL] cmd_add pour fichiers grep\n");
    unlink(fsname);
    return;
  }

  // 3. Test grep
  printf("Recherche du motif '%s' :\n", pattern);
  cmd_grep(fsname, pattern);

  // Nettoyage
  unlink(fsname);
  unlink(file1);
  unlink(file2);
  unlink(file3);
}

void TEST_RM()
{
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

  // 3. Vérifier que le fichier existe
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
    unlink(fsname_rm);
    unlink(file_ext);
    return;
  }
  else
  {
    printf("[OK] cmd_rm supprime bien le fichier\n");
  }

  // Nettoyage
  unlink(fsname_rm);
  unlink(file_ext);
}

void TEST_MKDIR()
{
  printf("=== Test cmd_mkdir ===\n");
  const char *fsname = "test_mkdir_fs";
  char dir1[] = "mon_dossier";
  char dir2[] = "autre_dossier";
  char dir3[] = "mon_dossier/mon_sous_dossier";
  char dir4[] = "mon_dossier/mon_sous_dossier2";
  char dir5[] = "mon_dossier/mon_sous_dossier2/mon_sous_sous_dossier";
  const char *content = "test mkdir\n";

  // Nettoyage préalable
  unlink(fsname);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour mkdir\n");
    return;
  }

  // 2. Créer un dossier
  if (cmd_mkdir(fsname, dir1) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir1);
    unlink(fsname);
    return;
  }

  // 6. Créer un deuxième dossier
  if (cmd_mkdir(fsname, dir2) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir2);
    unlink(fsname);
    return;
  }

  // 3. Créer un sous-dossier dans le premier dossier
  if (cmd_mkdir(fsname, dir3) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir3);
    unlink(fsname);
    return;
  }

  // 4. Créer un sous-dossier dans le premier dossier
  if (cmd_mkdir(fsname, dir4) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir4);
    unlink(fsname);
    return;
  }

  // 5. Créer un sous-sous-dossier dans le premier dossier
  if (cmd_mkdir(fsname, dir5) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir5);
    unlink(fsname);
    return;
  }
  printf("Resultat attendu :\n");
  printf("mon_dossier/\n");
  printf("mon_dossier/mon_sous_dossier/\n");
  printf("mon_dossier/mon_sous_dossier2/\n");
  printf("mon_dossier/mon_sous_dossier2/mon_sous_sous_dossier/\n");
  printf("autre_dossier/\n");
  printf("Resultat de cmd_tree :\n");

  cmd_tree(fsname);
  printf("[OK] cmd_mkdir a créé les dossiers\n");

  // Nettoyage
  unlink(fsname);
}

void TEST_RMDIR()
{
  printf("=== Test cmd_rmdir ===\n");
  const char *fsname = "test_rmdir_fs";
  char dir1[] = "dossier_a_supprimer";
  const char *content = "test rmdir\n";

  // Nettoyage préalable
  unlink(fsname);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour rmdir\n");
    return;
  }

  // 2. Créer un dossier
  if (cmd_mkdir(fsname, dir1) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir1);
    unlink(fsname);
    return;
  }

  // 3. Vérifier la présence du dossier avec cmd_find
  printf("Vérification présence dossier avant suppression :\n");
  cmd_tree(fsname);

  // 4. Supprimer le dossier
  if (cmd_rmdir(fsname, dir1) != 0)
  {
    printf("[FAIL] cmd_rmdir pour %s\n", dir1);
    unlink(fsname);
    return;
  }

  // 5. Vérifier que le dossier n'existe plus
  printf("Vérification absence dossier après suppression :\n");
  cmd_tree(fsname);

  printf("[OK] cmd_rmdir a supprimé le dossier\n");

  // Nettoyage
  unlink(fsname);
}

void TEST_MV_DIR()
{
  printf("=== Test cmd_mv sur dossier vide ===\n");
  const char *fsname = "test_mvdir_fs";
  const char *dir1 = "dossier1";
  const char *dir2 = "dossier2";

  // Nettoyage préalable
  unlink(fsname);

  // 1. Créer le FS
  if (cmd_mkfs(fsname, 10, 20) != 0)
  {
    printf("[FAIL] mkfs pour mv dir\n");
    return;
  }

  // 2. Créer deux dossiers
  if (cmd_mkdir(fsname, (char *)dir1) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir1);
    unlink(fsname);
    return;
  }
  if (cmd_mkdir(fsname, (char *)dir2) != 0)
  {
    printf("[FAIL] cmd_mkdir pour %s\n", dir2);
    unlink(fsname);
    return;
  }

  // 3. Renommer dossier1 en dossier1_renamed
  const char *dir1_renamed = "dossier1_renamed";
  if (cmd_mv(fsname, dir1, dir1_renamed) != 0)
  {
    printf("[FAIL] cmd_mv pour dossier vide\n");
    unlink(fsname);
    return;
  }

  // 4. Vérifier le résultat
  printf("Résultat attendu : dossier1_renamed/ et dossier2/\n");
  printf("Résultat de cmd_tree :\n");
  cmd_tree(fsname);

  // Nettoyage
  unlink(fsname);
}

int main()
{
  TEST_MKFS();
  printf("\n");
  TEST_CAT();
  printf("\n");
  TEST_CHMOD();
  printf("\n");
  TEST_ADD();
  printf("\n");
  TEST_INPUT();
  printf("\n");
  TEST_ADDINPUT();
  printf("\n");
  TEST_LS();
  printf("\n");
  TEST_DF();
  printf("\n");
  TEST_CP();
  printf("\n");
  TEST_GREP();
  printf("\n");
  TEST_FIND();
  printf("\n");
  TEST_MKDIR();
  printf("\n");
  TEST_RMDIR();
  printf("\n");
  TEST_MV_DIR();
  printf("\n");
  TEST_RM();
  printf("\n");

  return 0;
}