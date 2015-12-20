#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "komunikacja.h"

// Makra do obslugi kwadratowych tablic, dzięki temu szybkie mallok i free.
#define Szacunek(x, y) Szacunek[((x) - 1) * glembokosc + (y) - 1]
#define Teren(x, y) Teren[((x) - 1) * glembokosc + (y) - 1]
int dlugosc, glembokosc, oplataStala, ograniczenieA, ilosc_firm;
int *Szacunek, *Teren;
int *ilosc_robotnikow, *glembokosc_kopania, kolekcja_muzeum[1000009], *zajety_teren;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
void SZUKAJ(int d, int max_szac, int *pozwolenie, int *max_gle);
void * delegat(void *id){
	int id_firmy = * (int *)id;
	struct kom_do_firmy komunikat_odp;
	struct kom_do_delegata komunikat;
	struct kom_do_banku komunikat_b_odp;
	struct kom_z_banku komunikat_b;
	komunikat_b_odp.id_konta = id_firmy;
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
		komunikat_odp.id_adresata = komunikat.nadawca;
		switch(komunikat.jakie_zlecenie ){
			case 'K':
				// Koniec działalności.
				return (NULL);
			case 'R':
				// Przysyłają nam raport.
				id_robotnika = id_robotnika_z_adresu(komunikat.nadawca, id_firmy);
				ok = 1;
				x = 1;
				if(stan_robotnika[id_robotnika] <= 0) ok = 0;
				for(i = 0; ok && i < komunikat.ilosc_wykopanych_artefaktow; i++)
					x *= komunikat.wykopane_artefakty[i];
				if(stan_robotnika[id_robotnika] % x != 0) ok = 0;
				
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
				if(x >= maksymalna_glebokosc) ok = 0; // głębiej nie może kopać.
				if(ok) glembokosc_kopania[komunikat.nr_terenu]++;
				
				
				if(ok){
					komunikat_odp.jakie_zlecenie = 'T';
					komunikat_odp.symbol_zbioru = Teren(komunikat.nr_terenu, 
						glembokosc_kopania[komunikat.nr_terenu]);
					stan_robotnika[id_robotnika] = komunikat_odp.symbol_zbioru;
					} else komunikat_odp.jakie_zlecenie = 'N';
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
					zajety_teren[i + pozwolenie] = 0;
	
				pthread_mutex_unlock( &mutex );
				
				pozwolenie = -1;
				maksymalna_glebokosc = -1;
				komunikat_odp.jakie_zlecenie = 'Z'; // zwolniono.
				break;
				
			case 'O':
				// Przyjmowanie nowej oferty.
				komunikat_odp.jakie_zlecenie = 'N';
				if(pozwolenie != -1) break; // Ma już pozwolenie.
				
				// Procesz szukania terenu do pracy:
				SZUKAJ(ilosc_robotnikow[id_firmy], komunikat.cena - oplataStala,
					&pozwolenie, &maksymalna_glebokosc);
				// Jeśli znaleziono pozwolenie to zarezerwuj teren.
				if(pozwolenie != -1){
					for(i = 0; i < ilosc_robotnikow[id_firmy]; i++)
						zajety_teren[i + pozwolenie] = 1;
				}
				komunikat_odp.jakie_zlecenie = 'P';
				komunikat_odp.pozwolenie = pozwolenie;
				break;
			
		}
		
		// wysyłanie:
		x = msgsnd(ID_KOLEJKI_FIRM, &komunikat_odp, sizeof(komunikat_odp),0);
		if(x == -1)syserr("muzeum:Error in msgsnd\n");
	}
	return (NULL);
}


int main(int argc, char **argv){
	inituj_komunikacje('M');
	if(argc != 5){
		printf("MUZEUM_:Podales zla ilosc argumentow.\n");
		return 0;
	}
	
	dlugosc = atoi(argv[1]);
	glembokosc = atoi(argv[2]);
	oplataStala = atoi(argv[3]);
	ograniczenieA = atoi(argv[4]);
	Szacunek = malloc(sizeof(int) * dlugosc * glembokosc);
	Teren = malloc(sizeof(int) * dlugosc * glembokosc);
	int dlu, gle, i, x, j;
	// Wczytanie danych:
	for(dlu = 1; dlu <= dlugosc; dlu++)
		for(gle = 1; gle <= glembokosc; gle++)
			scanf("%d", &Teren(dlu, gle));
			
	for(dlu = 1; dlu <= dlugosc; dlu++)
		for(gle = 1; gle <= glembokosc; gle++)
			scanf("%d", &Szacunek(dlu, gle));
	

	
	// Pobranie informacji o ilości firm i ilości robotników w każdej firmie.
	struct kom_1_z_banku_do_muzeum komunikat;
	x = msgrcv(ID_KOLEJKI_BANK_MUZEUM, &komunikat, sizeof(komunikat), 1, 0);
	if(x == -1)syserr("muzeum:Error in msgrcv\n");
	ilosc_firm = komunikat.ilosc_firm; 
	ilosc_robotnikow = malloc(sizeof(int) * ilosc_firm);
	
	i = 0;
	while(i * komunikat.rozmiar_porcji < ilosc_firm){
		if( i != 0){
			x = msgrcv(ID_KOLEJKI_BANK_MUZEUM, &komunikat, sizeof(komunikat), i + 1, 0);
			if(x == -1)syserr("muzeum:Error in msgrcv\n");
		}
		
		for(j = 0; j < komunikat.rozmiar_porcji; j++)
			ilosc_robotnikow[i * komunikat.rozmiar_porcji + j +1] = komunikat.dane[j];
		i++;
	}
	
	// Stworzenie tablic.
	glembokosc_kopania = malloc(sizeof(int) * (dlugosc + 1));
	zajety_teren = malloc(sizeof(int) * (dlugosc + 1));
	for(i = 0; i <= dlugosc; i++)
		glembokosc_kopania[i] = zajety_teren[i] = 0;
	
	
	// Stworzenie delegatów.
	pthread_t pth_delegaci[ilosc_firm];
	int indeksy[ilosc_firm];
	for(i = 1; i <= ilosc_firm; i++){
		indeksy[i - 1] = i;
		pthread_create( &pth_delegaci[i - 1], NULL, &delegat, (void *) &indeksy[i - 1]);
	}
	
	// Oddelegowanie delegatów.
	for(i = 1; i <= ilosc_firm; i++)
		pthread_join(pth_delegaci[i - 1], NULL);
	
	// poinformowanie banku by skończył działalność.
	struct kom_do_banku komunikat_do_banku;
	komunikat_do_banku.id_konta = 0;
	komunikat_do_banku.jakie_zlecenie = 'Z';
	x = msgsnd(ID_KOLEJKI_BANK_MUZEUM, &komunikat_do_banku, sizeof(komunikat_do_banku), 0);
	if(x == -1)syserr("muzeum:Error in msgrcv\n");
		
		
	free(Teren);
	free(Szacunek);	
	free(ilosc_robotnikow);
	free(glembokosc_kopania);
	free(zajety_teren);
	return 0;
}

//glembokosc_kopania[], zajety_teren[];

void SZUKAJ(int d, int max_szac, int *pozwolenie, int *max_gle){
	int i, zajetych, szac, pop_szac, gle, j;
	zajetych = 0;
	if(dlugosc < d){
		*pozwolenie = -1;
		return ;
	}
	for(i = 1; i <= d; i++)
		zajetych += zajety_teren[i];
	
	while(i <= dlugosc){
		if(zajetych == 0){
			// Nikt nie kopie na tym odcinku.
			gle = 1;
			szac= 0;
			while(gle <= glembokosc){
				pop_szac = szac;
				for(j = 0; j < d; j++)
					if(glembokosc_kopania[i - j] < gle)
						szac += Szacunek(i - j, gle);
				if(szac > max_szac){
					if(pop_szac > 0){
						// Udane poszukiwanie.
						*pozwolenie = i - d +1;
						*max_gle = gle - 1;
						return;
					} else break;
				}
				gle++;
			}
			if(szac > 0 && szac <= max_szac){
				// Udane poszukiwanie.
				*pozwolenie = i - d +1;
				*max_gle = gle ;
				return;
			}
		}
		if( i < dlugosc)
			zajetych += zajety_teren[i + 1] - zajety_teren[i - d + 1];
		i++;
	}
	
}

