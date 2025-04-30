/* Corruption de quelques octets aléatoires d'un fichier
   Par Fabien de Montgolfier */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>

int main(int argc, char **argv) {
    int fd;         // dexcripteur du fichier a corrompre.
    size_t randoff; // offset aléatoire dans le fichier à corrompre
    FILE *randfile; /* generateur nombres aléatoires. FILE * pour 
		       bufferiser  les I/O et accélérer le prog */
    struct stat st; // pour taille de fd
    void *proj;     // le fichier fd projeté

    if (argc != 3) {
	printf("Usage : %s fichier n\nCorromp n fois un octet aléatoires du fichier.\n", argv[0]);
	return 0;
    }
    
    unsigned n = atoi(argv[2]);
    /* si atoi échoue, renvoie 0, et notre programme ne fera rien, 
       donc pas de traitement d'erreur... */
    
    // ouvre le générateur de nombres aléatoires	
    if ((randfile = fopen("/dev/random","r")) ==NULL)
	err(1, "fopen /dev/random");
    
    // ouvre le fichier donné en premier argument
    if ((fd = open(argv[1],O_RDWR)) <0)
	err(1, "open %s", argv[1]);

    // pour avoir sa taille
    if (fstat(fd,&st)<0)
	err(1,"fstat");

    // le projette en lecture/écriture partagée (pour réécriture)
    proj = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(proj==MAP_FAILED)
	err(1,"mmap");
    close(fd);
    
    /* boucle de corruption. 
       Notez qu'un même octet malchanceux peut être corrompu plusieurs fois.
       This is a feature not a bug : en effet 
       n fois un octet random != n octets random */
    for(unsigned i=0; i<n ; i++)
	{
	    // tire un offset aléatoire
	    if(fread(&randoff, sizeof(size_t), 1, randfile) != 1) 
		err(1,"fread de /dev/random");
	    // l'arrondi à la taille du fichier
	    randoff = randoff % st.st_size; 
	    // on écrit un octet aléatoire en proj[randoff]
	    if(fread(proj+randoff, 1, 1, randfile) != 1) 
		err(1,"fread de /dev/random");
	}
    // nettoyage final
    munmap(proj, st.st_size);
    fclose(randfile);
    return 0;
}
