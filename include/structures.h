#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdint.h>

struct pignoufs
{
  char magic[8];   // Magic number : "pignoufs"
  int32_t nb_b;    // Nombre total de blocs
  int32_t nb_i;    // Nombre d'inodes
  int32_t nb_a;    // Nombre de blocs allouables
  int32_t nb_l;    // Nombre de blocs libres
  int32_t nb_f;    // Nombre de fichiers stockés
  char zero[3972]; // Padding pour atteindre 4000 octets
};

struct bitmap_block
{
  uint8_t bits[4000]; // 32 000 bits (4 000 octets)
  uint8_t sha1[20];   // SHA1 du contenu
  uint32_t type;      // Type du bloc (2 pour bitmap)
  char padding[72];   // Espace réservé
};

struct inode
{
  uint32_t flags;                // Droits et attributs du fichier
  uint32_t file_size;            // Taille en octets
  uint32_t creation_time;        // Date de création
  uint32_t access_time;          // Date du dernier accès
  uint32_t modification_time;    // Date de la dernière modification
  char filename[256];            // Nom du fichier (255 + 1 pour '\0')
  int32_t direct_blocks[900];    // Pointeurs directs vers les blocs de données
  int32_t double_indirect_block; // Pointeur vers un bloc d'indirection double
  char extensions[120];          // Zone pour extensions (optionnelle)
  uint8_t sha1[20];              // SHA1 du contenu
  uint32_t type;                 // Type du bloc (3 pour inode)
  char padding[72];              // Padding
};

struct data_block
{
  char data[4000];  // Données du fichier
  uint8_t sha1[20]; // SHA1 du contenu
  uint32_t type;    // Type du bloc (5 pour données)
  char padding[72]; // Padding
};

struct address_block
{
  int32_t addresses[1000]; // Pointeurs vers d'autres blocs
  uint8_t sha1[20];        // SHA1 du contenu
  uint32_t type;           // Type du bloc (6 pour indirection simple, 7 pour double)
  char padding[72];        // Padding
};

#endif