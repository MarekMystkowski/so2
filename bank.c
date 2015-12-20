/* Bank musi obsługiwać tylko zlecenia od Muzeum.
 * I tylko przelewy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/ipc.h> 
#include "komunikacja.h"
int *Saldo, *LiczbaPracownikow, iloscFirm, oplataStala, ograniczenieA;

int main(int argc, char **argv){
	inituj_komunikacje('B');
	if(argc != 4){
		printf("BANK_:Podales zla ilosc argumentow %d.\n", argc);
		return 0;
	}
	
	iloscFirm = atoi(argv[1]);
	oplataStala = atoi(argv[2]);
	ograniczenieA = atoi(argv[3]);
	int id, i, x;
	 // Uruchamia wszystkie procesy firm z ich identyfikatorami.
	 for(id = 1; id <= iloscFirm; id++){
		pid_t pid = fork();
		if(pid == -1) syserr("Error in fork\n");
		if(pid == 0){ 
			char buf1[10];
			sprintf(buf1, "%d", id);
			execl("./firma", "firma", buf1, argv[2], argv[3], (char *) 0);
			syserr("bank:Error in execl\n");
		}
	}
	
	Saldo = malloc((iloscFirm + 1) * sizeof(int));
	LiczbaPracownikow = malloc((iloscFirm + 1) * sizeof(int));
	Saldo[0] = 0; // Ustalenie salda dla muzeum.
	// Wczytanie danych i wysłanie komunikatów wstępnych dla firm:
	int id_firmy, poczatkowe_saldo, liczba_pracownikow, j, ok;
	struct kom_1_z_baku_do_firmy komunikat_do_firm1;
	for(i = 1; i <= iloscFirm; i++){
		scanf("%d %d %d", &id_firmy, &poczatkowe_saldo, &liczba_pracownikow);
		Saldo[id_firmy] = poczatkowe_saldo;
		LiczbaPracownikow[id_firmy] = liczba_pracownikow;
		
		// ustawienie adresata na firmę.
		komunikat_do_firm1.adresat_komunikatu =adres_firmy(id_firmy, 0);
		komunikat_do_firm1.saldo = poczatkowe_saldo;
		komunikat_do_firm1.liczba_pracownikow = liczba_pracownikow;
		x = msgsnd(ID_KOLEJKI_BANK_FIRMA, &komunikat_do_firm1, 
			sizeof(komunikat_do_firm1),0);
		if(x == -1)syserr("bank:Error in msgsnd1\n");
	}
	
	
	// Wysłanie wstępnych informacji do muzeum.
	
	// Pobranie informacji o ilości firm i ilości robotników w każdej firmie.
	struct kom_1_z_banku_do_muzeum komunikat_do_muzeum1;
	komunikat_do_muzeum1.ilosc_firm = iloscFirm;
	komunikat_do_muzeum1.nr_porcja = 1;
	komunikat_do_muzeum1.rozmiar_porcji = 100;
	
	i = 0;
	while(i * komunikat_do_muzeum1.rozmiar_porcji < iloscFirm){
		for(j = 0; komunikat_do_muzeum1.rozmiar_porcji * i + j + 1
				<= iloscFirm && j < komunikat_do_muzeum1.rozmiar_porcji; j++)
			komunikat_do_muzeum1.dane[j] = 
				LiczbaPracownikow[komunikat_do_muzeum1.rozmiar_porcji * i + j + 1] ;

		x = msgsnd(ID_KOLEJKI_BANK_MUZEUM, &komunikat_do_muzeum1,
				sizeof(komunikat_do_muzeum1), 0);
		if(x == -1)syserr("bank:Error in msgsnd2\n");
		i++;
	}
	
	printf("Bank przechodzi w obsługe kont\n");
	
	ok = 1;
	struct kom_z_banku komunikat_odp;
	struct kom_do_banku komunikat;
	// obsluga kont:
	while(ok){
		x = msgrcv(ID_KOLEJKI_BANK_MUZEUM, &komunikat, sizeof(komunikat),0, 0);
		if(x == -1)syserr("bank:Error in msgrcv3\n");
		
		switch(komunikat.jakie_zlecenie ){
			case 'Z':
				// Zamknięcie banku.
				ok = 0;
				printf("bank:    zadanie zakonczenia\n");
				break;
			case 'P':
				printf("bank:    zadanie przeledu %d -> %d [%d]\n",komunikat.id1, komunikat.id2, komunikat.kwota );
				komunikat_odp.adresat_komunikatu = komunikat.id_konta;
				if(Saldo[komunikat.id1] < komunikat.kwota && komunikat.id1 != 0 )
					komunikat_odp.akceptacja_tranzakcji = 0;
				else {
					komunikat_odp.akceptacja_tranzakcji = 1;
					Saldo[komunikat.id2] += komunikat.kwota;
					Saldo[komunikat.id1] -= komunikat.kwota;
				}
				
				x = msgsnd(ID_KOLEJKI_BANK_MUZEUM, &komunikat_odp, sizeof(komunikat_odp), 0);
				if(x == -1)syserr("bank:Error in msgsnd4\n");
				break;
				
			case 'I':
				komunikat_odp.adresat_komunikatu = komunikat.id_konta;
				komunikat_odp.stan_konta = Saldo[komunikat.id1];	
				x = msgsnd(ID_KOLEJKI_BANK_MUZEUM, &komunikat_odp, sizeof(komunikat_odp), 0);
				if(x == -1)syserr("bank:Error in msgsnd5\n");
				break;
			
		}
	}
	free(Saldo);
	return 0;
}
