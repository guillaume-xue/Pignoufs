/* calcul d'un SHA1 et affichage semblable à sha1sum
   par Fabien de Montgolfier */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <openssl/sha.h>

int main(int argc, char **argv)
{
    int fd;         // dexcripteur du fichier a sha1sommer
    struct stat st; // pour taille de fd
    void *proj;     // le fichier fd projeté

    if (argc != 2)
    {
        printf("Usage : %s fichier \nCalcul de SHA1\n", argv[0]);
        return 0;
    }
    // ouvre le fichier donné en premier argument
    if ((fd = open(argv[1], O_RDONLY)) < 0)
        err(1, "open %s", argv[1]);

    // pour avoir sa taille
    if (fstat(fd, &st) < 0)
        err(1, "fstat");

    // le projette en lecture
    proj = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (proj == MAP_FAILED)
        err(1, "mmap");
    close(fd);

    // calcul de SHA1. C'est tout simple non ?
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(proj, st.st_size, hash);

    // affichage hexa
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        printf("%02x", (unsigned int)hash[i]);
    }
    printf("  %s\n", argv[1]); // sha1sum met deux espaces alors moi aussi...

    // nettoyage final
    munmap(proj, st.st_size);
    return 0;
}
