/// Scalenie komunikatów robotnika i firmy
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "komunikacja.h"

// Makra do obslugi kwadratowych tablic, dzięki temu szybkie mallok i free.
#define Szacunek(x, y) Szacunek[((x) - 1) * glembokosc + (y) - 1]
#define Teren(x, y) Teren[((x) - 1) * glembokosc + (y) - 1]
int dlugosc, glembokosc, oplataStala, ograniczenieA;
int *Szacunek, *Teren;



void delegat(void *id){
	int id_firmy = (int) id;
	struct kom_do_firmy komunikat_odp;
	struct kom_do_muzeum komunikat;
	struct kom_do_banku komunikat_b_odp;
	struct kom_z_banku komunikat_b;
	komunikat_b_odp.kto_zleca = 0;
	int pozwolenie = -1;
	int maksymalna_glebokosc = -1;
	int x, i, id_robotnika, ok;
	int stan_robotnika[ilosc_robotnikow[id_firmy] + 1];
	for (i = 0; i < ilosc_robotnikow[id_firmy] + 1; i++) stan_robotnika[i] = -1;
	// Jeden obrut pętli to obsługa jednego komunikatu.
	while(1){
		x = msgrcv(ID_KOLEJKI_DELEGATOW, &komunikat, sizeof(komunikat), id_firmy, 0);
		if(x == -1)syserr("muzeum:Error in msgrcv\n");
		
		// ustalenie adresata:
		komunikat_odp.id_adresata = komunikat.id_nadawcy;
		swich(komunikat.jakie_zlecenie){
			case 'R':
				// Przysyłają nam raport.
				id_robotnika = id_robotnika_z_adresu(komunikat.nadawca, id_firmy);
				ok = 1;
				x = 1;
				if(stan_robotnika[id_robotnika] <= 0) ok = 0;
				for(i = 0; ok && i < komunikat.ilosc_wykopanych_artefaktow; i++)
					x *= komunikat.wykopane_artefakty[i];
				if(x != stan_robotnika[id_robotnika]) ok = 0;
				
				// odpowiadanie na raport.
				if(ok)
					komunikat_odp.jakie_zlecenie = 'A';
					else komunikat_odp.jakie_zlecenie = 'N';
				stan_robotnika[id_robotnika] = -1; // już nie kopie
				break;
			case 'T':
				// Prosi o teren.
				id_robotnika = id_robotnika_z_adresu(komunikat.nadawca, id_firmy);
				ok = 1;
				if(stan_robotnika[id_robotnika] != -1) ok = 0; // już coś kopie.
				x = glembokosc_kopania[komunikat.nr_terenu];
				if(x == maksymalna_glebokosc) ok = 0; // głębiej nie może kopać.
				if(ok) glembokosc_kopania[komunikat.nr_terenu]++;
				
				
				if(ok){
					komunikat_odp.jakie_zlecenie = 'T';
					komunikat_odp.symbol_zbioru = Teren(komunikat.nr_terenu, 
						glembokosc_kopania[komunikat.nr_terenu]);
				else komunikat_odp.jakie_zlecenie = 'N';
				break;
			case 'S':
				// Sprzedają nam artefakty.
				
				// Wysłanie przelewu do banku.
				komunikat_b_odp.id1 = 0;
				komunikat_b_odp.id2 = id_firmy;
				komunikat_b_odp.kwota = komunikat.kolekcja * 10;
				x = msgsnd(ID_KOLEJKI_BANK_MUZEUM, &komunikat_b_odp, sizeof(komunikat_b_odp),0);
				if(x == -1)syserr("muzeum:Error in msgsnd\n");
				
				// Czekanie na akceptacje.
				x = msgrcv(ID_KOLEJKI_BANK_MUZEUM, &komunikat_b, sizeof(komunikat_b), id_firmy, 0);
				if(x == -1)syserr("muzeum:Error in msgrcv\n");
				assert(komunikat_b.akceptacja_tranzakcji);
				
				// Dodanie do kolekcji muzeum i przygotowanie odpowiedzi.
				pthread_mutex_lock( &mutex );
				kolekcja_muzeum[komunikat.kolekcja]++;
				pthread_mutex_unlock( &mutex );
				komunikat_odp.jakie_zlecenie = 'A';
				break;
			case 'Z':
				// Firma zwalnia teren.
				komunikat_odp.jakie_zlecenie = 'N';
				if(pozwolenie == -1) break; // już zwolniony.
				
				pthread_mutex_lock( &mutex );
				
				// Twra zwalnianie terenów.
				for(i = 0; i < ilosc_robotnikow[id_firmy]; i++)
					Zajety_teren[i] = 0;
	
				pthread_mutex_unlock( &mutex );
				
				pozwolenie = -1;
				komunikat_odp.jakie_zlecenie = 'Z'; // zwolniono.
				break;
				
			case 'O':
				// Przyjmowanie nowej oferty.
				komunikat_odp.jakie_zlecenie = 'N';
				if(pozwolenie != -1) break; // Ma już pozwolenie.
				
				// Procesz szukania rerenu do pracy:
				/// SZUKAJ.
				
				
				komunikat_odp.jakie_zlecenie = 'P';
				komunikat_odp.pozwolenie = pozwolenie;
				break;
			
		}
		
		// wysyłanie:
		x = msgsnd(ID_KOLEJKI_FIRM, &komunikat_odp, sizeof(komunikat_odp),0);
		if(x == -1)syserr("muzeum:Error in msgsnd\n");
	}
}













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
