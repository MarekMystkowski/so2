#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/ipc.h> 
#include "komunikacja.h"
int *Saldo, iloscFirm, oplataStala, ograniczenieA;
int dzialajace_firmy, czy_muzeum_dziala;
int *Firma_dziala;
// Do jakiegoś pliku .h:

struct kom_1_z_baku_do_firmy{
	long adresat_komunikatu; // typ.
	int saldo, liczba_pracownikow;
}

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
	if(argc != 4){
		printf("BANK:Podales zla ilosc argumentow.\n");
		return 0;
	}
	
	iloscFirm = atoi(argv[1]);
	oplataStala = atoi(argv[2]);
	ograniczenieA = atoi(argv[3]);
	
	 // Uruchamia wszystkie procesy firm z ich identyfikatorami.
	 for(int id = 1; id <= iloscFirm; id++){
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
	Firma_dziala = malloc((iloscFirm + 1) * sizeof(int));
	Saldo[0] = 0; // Ustalenie salda dla muzeum.
	// Wczytanie danych i wysłanie komunikatów wstępnych dla firm:S
	int id_firmy, poczatkowe_saldo, liczba_pracownikow;
	for(int i = 1; i <= iloscFirm; i++){
		scanf("%d %d %d", &id_firmy, &poczatkowe_saldo, &liczba_pracownikow);
		Saldo[id_firmy] = poczatkowe_saldo;
		
		struct kom_1_z_baku_do_firmy komunikat;
		// ustawienie adresata na firmę.
		komunikat.adresat_komunikatu = typ_komunikatu('F', id_firmy, 0);
		komunikat.saldo = poczatkowe_saldo;
		komunikat.liczba_pracownikow = liczba_pracownikow;
		int x = msgsnd(ID_KOLEJKI_BANKU, &komunikat, sizeof(komunikat),0);
		if(x == -1)syserr("bank:Error in msgsnd\n");
	}
	for(int i = 1; i <= iloscFirm; i++) Firma_dziala[i] = 1;
	dzialajace_firmy = iloscFirm;
	
	// obsluga kont:
	while(1){
		kom_do_banku komunikat;
		int x = msgrcv(ID_KOLEJKI_BANKU, &komunikat, sizeof(komunikat),
						typ_komunikatu('B', 0, 0), 0);
		if(x == -1)syserr("bank:Error in msgrcv\n");
		
		// czy ktoś kasuje rachunek?
		if(komunikat.czy_zamykam_konto){
			if(komunikat.kto_zleca == 0){
				// muzeum zamyka konto: co się stanie z dorobkiem naszej cywilizacji .
				czy_muzeum_dziala = 0;
			} else {
				// firma zamyka konto: stać ich na Bitcoin ? :O .
				dzialajace_firmy--;
				Firma_dziala[komunikat.kto_zleca] = 0;
			}
			if(!dzialajace_firmy && !czy_muzeum_dziala)
				// nie posiadam już klijentow, zamykam interes :/ .
				break; 
			if(!dzialajace_firmy){
				// Powiedz muzeum by kończył działalność.
				struct kom_z_banku komunikat_odp;
				komunikat_odp.koncz_dzialalnosc = 1;
				komunikat_odp.adresat_komunikatu = typ_komunikatu('M', 0, 0);
				int x = msgsnd(ID_KOLEJKI_BANKU, &komunikat_odp, sizeof(komunikat_odp),0);
				if(x == -1)syserr("bank:Error in msgsnd\n");
			}
		} else if(komunikat.jakie_zlecenie == 'K'){
			// Rozeslanie informacji o prośbe zakończenia dzialaności
			// wszystkich dziłających firm.
			struct kom_z_banku komunikat_odp;
			komunikat_odp.koncz_dzialalnosc = 1;
			for(int i = 1; i <= iloscFirm; i++)
				if(Firma_dziala[i]){
					komunikat_odp.adresat_komunikatu = typ_komunikatu('F', i, 0);
					int x = msgsnd(ID_KOLEJKI_BANKU, &komunikat_odp, sizeof(komunikat_odp),0);
					if(x == -1)syserr("bank:Error in msgsnd\n");
				}
			
		} else {
			// Odpowiadanie na zapytanie
			struct kom_z_banku komunikat_odp;
			if(komunikat.kto_zleca == 0)
				komunikat_odp.adresat_komunikatu = typ_komunikatu('M', 0, 0);
			else
				komunikat_odp.adresat_komunikatu = typ_komunikatu('F', 
					komunikat.kto_zleca, 0);
			komunikat_odp.akceptacja_tranzakcji = 0;
			komunikat_odp.koncz_dzialalnosc = 0;
			if(komunikat.jakie_zlecenie == 'P'){
				if(komunikat.id1 == 0 || Saldo[komunikat.id1] >= komunikat.kwota){
					Saldo[komunikat.id1] -= komunikat.kwota;
					Saldo[komunikat.id2] += komunikat.kwota;
					komunikat.akceptacja_tranzakcji = 1;
			}
			komunikat_odp.stan_konta = Saldo[komunikat.kto_zleca];
			int x = msgsnd(ID_KOLEJKI_BANKU, &komunikat_odp, sizeof(komunikat_odp),0);
			if(x == -1)syserr("bank:Error in msgsnd\n");
		}
	}
	free(Saldo);
	return 0;
}
