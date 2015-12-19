#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/ipc.h> 

// Makra do obslugi kwadratowych tablic, dzięki temu szybkie mallok i free.
#define Szacunek(x, y) Szacunek[((x) - 1) * glembokosc + (y) - 1]
#define Teren(x, y) Teren[((x) - 1) * glembokosc + (y) - 1]
int dlugosc, glembokosc, oplataStala, ograniczenieA;
int *Szacunek, *Teren;
// Do jakiegoś pliku .h:

struct kom_do_banku{
	long adres_banku; // typ.
	// I - informaca o saldzie, P - wykonanie przelewu, K - rozesłanie zakończenia dla firm.
	char jakie_zlecenie; 
	int kto_zleca, kwota;
	int id1, id2; // przelewy wykonywane są z id1-->id2 
	              // muzeum ma konto o id == 0 i to konto na początku ma 0
	              // i może zejść na poniżej 0.
	              // Na pozostałych kontach zabronione jest zejscie ponizej 0.
	int czy_zamykam_konto; // gdy proces kończy działanie informuje o tym bank.
}

struct kom_z_banku{
	long adresat_komunikatu; // typ.
	int stan_konta, akceptacja_tranzakcji;
	int koncz_dzialalnosc;
}

// ********************

int main(int argc, char **argv){
	if(argc != 5){
		printf("BANK:Podales zla ilosc argumentow.\n");
		return 0;
	}
	
	dlugosc = atoi(argv[1]);
	glebokosc = atoi(argv[2]);
	oplataStala = atoi(argv[3]);
	ograniczenieA = atoi(argv[4]);
	Szacunek = malloc(sizeof(int) * dlugosc * glembokosc);
	Teren = malloc(sizeof(int) * dlugosc * glembokosc);
	
	// wczytanie danych:
	for(int dlu = 1; dlu <= dlugosc; dlu++)
		for(int gle = 1; gle <= glembokosc; gle++)
			scanf("%d", &Teren(dlu, gle));
			
	for(int dlu = 1; dlu <= dlugosc; dlu++)
		for(int gle = 1; gle <= glembokosc; gle++)
			scanf("%d", &Szacunek(dlu, gle));
	
	
	free(Teren);
	free(Szacunek);	
	return 0;
}
