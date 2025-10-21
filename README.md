# pignoufs — Système de fichiers

**Pignoufs** (Projet Informatique de Gestation d’un Nouvel Outil Ultravérifié de File System), un mini-système de fichiers contenu dans un seul fichier conteneur. Ce conteneur est manipulé via des lignes de commande (`pignoufs <commande> <conteneur> [...]`) qui reproduisent des commandes de base comme `ls`, `cp`, `rm`, `fsck`, etc. L'accès interne au conteneur se fait par projection mémoire (`mmap`) — les accès via `read`/`write` classiques ne sont pas utilisés par conception.

---

## Objectifs

* Fournir un système de fichiers stocké dans un seul fichier (le *conteneur*).
* Garantir l'intégrité des blocs grâce à un SHA-1 par bloc.
* Proposer des commandes pour créer, lister, lire, écrire, supprimer et vérifier le système de fichiers.
* Gérer la concurrence avec un mécanisme de verrouillage (au minimum global, extensible par bloc ou inode).

---

## Caractéristiques essentielles

* **Accès via mmap** : le conteneur est projeté en mémoire pour toutes les opérations.
* **Intégrité** : chaque bloc contient un SHA-1 des 4000 octets de données pour détecter toute corruption.
* **Bloc physique** : 4096 octets (4 KiB) composés de :

  * 4000 octets de contenu effectif
  * 20 octets : SHA-1
  * 4 octets : type de bloc
  * 72 octets : espace réservé (verrouillage / métadonnées)
* **Types de blocs** : superbloc, bitmap, inode, bloc libre, bloc de données, indirection simple, indirection double.
* **Noms internes** : les fichiers internes du conteneur commencent par `//` (ex. `//toto.txt`).
* **Endianness** : entier de 4 octets en little-endian.

---

## Interface

```
pignoufs <commande> <conteneur> [options]
```

Commandes implémentées ou attendues :

* `mkfs` : créer un conteneur neuf (`pignoufs mkfs <conteneur> <nb_i> <nb_a>`)
* `ls`   : lister fichiers (comportement similaire à `ls`, options comme `-l`)
* `df`   : afficher l'espace libre (en blocs de 4 KiB)
* `cp`   : copier depuis/vers le conteneur et en interne
* `rm`   : supprimer un fichier
* `lock` : prendre un verrou lecture/écriture sur une ressource interne
* `chmod`: modifier les droits (flags lecture/écriture)
* `cat`  : afficher le contenu d'un fichier interne
* `input` / `add` : écrire ou concaténer des données depuis l'entrée standard
* `fsck` : vérifier l'intégrité (magic, SHA-1, cohérence)
* `mount` / `umount` : optionnels selon l'implémentation

Chaque commande traite proprement les erreurs et affiche des messages explicites sur la sortie d'erreur.

---

## Format physique

Le conteneur est organisé en 4 zones contiguës :

1. Superbloc (1 bloc)
2. Bitmaps des blocs libres (nb1 blocs)
3. Inodes (nbi blocs, 1 inode par bloc)
4. Blocs allouables (nba blocs)

Calcul : `nbb = 1 + nb1 + nbi + nba`. Taille du conteneur = `4096 * nbb`.

### Inode

Chaque inode occupe un bloc (4000 octets utiles) et contient entre autres :

* flags (existence, droits lecture/écriture, verrous, type répertoire)
* taille du fichier, dates (création / accès / modification)
* nom du fichier (256 octets, max 255 octets utiles)
* 900 adresses de blocs (valeur `-1` si non utilisée)
* numéro du bloc d'indirection double (ou `-1`)
* zone d'extension (120 octets)

---

## Intégrité & vérification

* Avant de lire un bloc, le SHA-1 est vérifié ; après chaque écriture, le SHA-1 est mise à jour.
* La commande `fsck` vérifie :

  * le magic number du superbloc
  * les hashes de tous les blocs
  * la cohérence entre bitmaps et inodes (blocs alloués vs libres)

Un outil de corruption peut être utilisé pour injecter des erreurs et tester `fsck`.

---

## Concurrence & verrouillage

* Au minimum un verrou global est implémenté (mutex sur l'ensemble du conteneur).
* Il est possible d'étendre le mécanisme à un verrou par bloc ou par inode (lecture/écriture) pour améliorer la concurrence.
* Les verrous doivent être libérés proprement en cas d'exception ou de signal (`SIGTERM`, `SIGINT`).

---

## Compilation

**Prérequis** : `gcc`, `make`, `libpthread`, `libcrypto`, `pkg-config`, `openssl`.

Exemple minimal :

```bash
make
```

Le projet produit un exécutable `pignoufs` (ou plusieurs binaires selon l'implémentation).

---

## Exemples d'utilisation

Créer un système de fichiers :

```bash
./pignoufs mkfs conteneur 128 1024
```

Lister les fichiers :

```bash
./pignoufs ls conteneur
```

Copier un fichier local dans le conteneur :

```bash
./pignoufs cp conteneur monfichier.txt //monfichier.txt
```

Extraire un fichier du conteneur :

```bash
./pignoufs cp conteneur //monfichier.txt sortie.txt
```

Vérifier le système :

```bash
./pignoufs fsck conteneur
```

Prendre un verrou en écriture :

```bash
./pignoufs lock conteneur //monfichier.txt w
```

---

## Limitations connues

* Le format suppose une architecture little-endian et des entiers 4 octets.
* Taille d'un bloc utile = 4000 octets ; un bloc physique = 4096 octets.
* L'intégrité dépend du SHA-1 ; sans l'implémentation des hashes l'intégrité n'est pas garantie.

---

## Licence

Ce dépôt public est distribué sous licence MIT — voir `LICENSE` pour les détails.
