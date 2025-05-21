#ifndef UTILITAIRES_H
#define UTILITAIRES_H

#include "structures.h"
#include "sha1.h"
#include <errno.h>
#include <dirent.h>

// Gestion centralisée des erreurs
int print_error(const char *msg);
void fatal_error(const char *msg);

void split_path(const char *path, char *parent_path, char *dir_name);

// Helpers pour accéder aux blocs
struct pignoufs *get_superblock(uint8_t *map);
struct bitmap_block *get_bitmap_block(uint8_t *map, int32_t b);
struct inode *get_inode(uint8_t *map, int32_t b);
struct data_block *get_data_block(uint8_t *map, int32_t b);
struct address_block *get_address_block(uint8_t *map, int32_t b);

// Récupération des données du conteneur
void get_conteneur_data(uint8_t *map, int32_t *nb1, int32_t *nbi, int32_t *nba, int32_t *nbb);

void decremente_lbl(uint8_t *map);

void incremente_lbl(uint8_t *map);

void decrement_nb_f(uint8_t *map);

void increment_nb_f(uint8_t *map);

bool check_bitmap_bit(uint8_t *map, int32_t blknum);

// Initialisation d'un inode, fichier simple pour l'instant
void init_inode(struct inode *in, const char *name, bool type);

// Ouverture du système de fichiers et projection en mémoire
int open_fs(const char *fsname, uint8_t **map, size_t *size);

// Fermeture du système de fichiers et synchronisation
void close_fs(int fd, uint8_t *map, size_t size);

void bitmap_alloc(uint8_t *map, int32_t blknum);

int find_inode_racine(uint8_t *map, int32_t nb1, int32_t nbi, const char *name, bool type);

int find_file_folder_from_inode(uint8_t *map, struct inode *in, const char *name, bool type);

int find_inode_folder(uint8_t *map, int32_t nb1, int32_t nbi, const char *name);

void add_inode(struct inode *in, int val);

void delete_separte_inode(struct inode *in, int val);

int32_t alloc_data_block(uint8_t *map);

void bitmap_dealloc(uint8_t *map, int32_t blknum);

void dealloc_data_block(struct inode *in, uint8_t *map);

int create_file(uint8_t *map, const char *filename);

int create_directory_main(uint8_t *map, const char *dirname, int profondeur);

int create_directory(uint8_t *map, char *dirname);

// On récupère le dernier bloc écrit
int32_t get_last_data_block(uint8_t *map, struct inode *in);

// Pour les calculs ont suppose qu'on ait des blocs pleins
int32_t get_last_data_block_null(uint8_t *map, struct inode *in);

// Verrouiller un bloc pour lecture ou écriture
int lock_block(int fd, int64_t block_offset, int lock_type);

// Déverrouiller un bloc
int unlock_block(int fd, int64_t block_offset);

bool is_folder_path(const char *parent, const char *child);

#endif