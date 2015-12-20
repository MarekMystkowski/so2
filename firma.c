#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "komunikacja.h"
	
int id_firmy, oplataStala, ograniczenieA, koncz_prace;
int ilosc_pracownikow, saldo, ilu_robotnikow_pracuje;
int kolekcja_firmowa[1000009];
int pozwolenie; // -1 - brak pozwolenia; wpf: mamy pozwolenie na pola [pozwoleinie,..]

void PRACUJ(int symbol_zbioru, struct kom_do_delegata *odp);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t oczekujacy_robotnicy = PTHREAD_COND_INITIALIZER;
pthread_cond_t oczekujacy_kierownik = PTHREAD_COND_INITIALIZER;


void * kierownik(){
	struct kom_do_firmy komunikat;
	struct kom_do_delegata komunikat_odp;
	komunikat_odp.id_firmy = id_firmy;
	komunikat_odp.nadawca = adres_firmy(id_firmy, 0);
	komunikat_odp.ilosc_pol = ilosc_pracownikow;
	
	int cena = oplataStala * 3 + ilosc_pracownikow*(3 +ograniczenieA/10000);
	if(cena > saldo) cena = saldo;
	
	pthread_mutex_lock( &mutex );
	printf("firma %d :     P(mutex) kierownik\n", id_firmy ); 
	int i, j, x;
	while(1){
		printf("firma %d :     wait() kierownik\n", id_firmy ); 
		pthread_cond_wait( &oczekujacy_kierownik, &mutex );
		if(!koncz_prace){
			// Sprzedanie kolekcji.
			j = 0;
			for(i = 2; i <= ograniczenieA; i++)
				while( kolekcja_firmowa[i] >= i){

					// wyślij tranzakcje do muzeum.
					komunikat_odp.jakie_zlecenie = 'S';
					komunikat_odp.kolekcja = i;
					x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
					if(x == -1)syserr("firma:Error in msgsnd\n");
					kolekcja_firmowa[i] -= i;
					saldo+= i * 10;
					j++;
				}
			
			// Odbieranie akceptacji tranzakcji.
			printf("firma %d :     V(mutex) kierownik\n", id_firmy ); 
			pthread_mutex_unlock( &mutex );
			
			while(j-- > 0){
				x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
						adres_firmy(id_firmy, 0), 0);
				if(x == -1)syserr("firma:Error in msgrcv\n");
				assert(komunikat.jakie_zlecenie == 'A');
			}
			
			// Jeśli ma jakieś pozwolenie to zwolń je.
			if(pozwolenie != -1){
				komunikat.jakie_zlecenie = 'Z';
				x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
				if(x == -1)syserr("firma:Error in msgsnd\n");
				
				// Odebranie potwierdzenia.
				x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
						adres_firmy(id_firmy, 0), 0);
				if(x == -1)syserr("firma:Error in msgrcv\n");
				assert(komunikat.jakie_zlecenie == 'Z');
			}
			pthread_mutex_lock( &mutex );
			printf("firma %d :     P(mutex) kierownik\n", id_firmy ); 
			pozwolenie = -1;
			
			// Negocjowanie pozwoleń.
			while(pozwolenie == -1){
				if(saldo < cena && oplataStala < saldo)
					cena = saldo;
					// wysyłanie oferty:
				komunikat_odp.jakie_zlecenie = 'O';
				komunikat_odp.cena = cena;
				x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
				if(x == -1)syserr("firma:Error in msgsnd\n");
				
				x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
					adres_firmy(id_firmy, 0), 0);
				if(x == -1)syserr("firma:Error in msgrcv\n");
				
				if(komunikat.jakie_zlecenie == 'P'){
					// Udana tranzakcja :D
					saldo -= cena;
					pozwolenie = komunikat.pozwolenie;
				}else {
					//odmowa :'( 
					saldo -= oplataStala;
					if (cena >= saldo) break /* nic nie wytarguję już*/ ;
					cena *= 2; // sprubuję podwujną cenę:D
				}
			}
		}
		pthread_cond_broadcast(&oczekujacy_robotnicy);
		printf("firma %d :     Wzbudz(robotnicy) kierownik\n", id_firmy ); 
		if(koncz_prace){
			printf("firma %d :     V(mutex) kierownik\n", id_firmy ); 
			pthread_mutex_unlock( &mutex );
			return /*koniec pracy*/(NULL);
		}
	}
}
	
void * pracownik(void *id){
	int moje_id = * (int *)id;
	int x, i;
	struct kom_do_delegata komunikat_odp;
	struct kom_do_firmy komunikat;
	komunikat_odp.id_firmy = id_firmy;
	komunikat_odp.nadawca = adres_firmy(id_firmy, moje_id);
	komunikat_odp.ilosc_pol = ilosc_pracownikow;
	
	pthread_mutex_lock( &mutex );
	printf("firma %d pr:%d :     P(mutex)\n", id_firmy, moje_id ); 
	
	// Jeden obrut pętli to jedno pozwolenie.
	while(1){
		ilu_robotnikow_pracuje--;
		printf("firma %d pr:%d :     ilu_rob: %d\n", id_firmy, moje_id, ilu_robotnikow_pracuje); 
		// Jeśli nie jestem ostatnim to wieszam się.
		if(ilu_robotnikow_pracuje != 0){
			printf("firma %d pr:%d :     wait(robotnik)\n", id_firmy, moje_id ); 
			pthread_cond_wait( &oczekujacy_robotnicy, &mutex );
			ilu_robotnikow_pracuje++;
		} else {
			// Może trzeba wybudzić kierownika by zdobył nowe pozwolenie.
			ilu_robotnikow_pracuje++; // dzięki temu nikt tu nie wejdzie gdy ja tu jestem.
			
			// budzenie kierownika
			printf("firma %d pr:%d :    signal(kierow)\n", id_firmy, moje_id ); 
			pthread_cond_signal(&oczekujacy_kierownik);
			printf("firma %d pr:%d :     wait(robotnik)\n", id_firmy, moje_id ); 
			pthread_cond_wait( &oczekujacy_robotnicy, &mutex );
			
		}
		if(pozwolenie == -1){
			ilu_robotnikow_pracuje--;
			printf("firma %d pr:%d :     V(mutex)\n", id_firmy, moje_id ); 
			pthread_mutex_unlock( &mutex );
			return /*Kończę pracę :D*/(NULL);
		 }
		
		// jeden obrót pętli to jeden teren przekopany:
		while(1){
			
			if(koncz_prace){
				ilu_robotnikow_pracuje--;
				printf("firma %d pr:%d :     V(mutex)\n", id_firmy, moje_id ); 
				pthread_mutex_unlock( &mutex );
				return /*Kończę pracę :D*/(NULL);
			}
			
			// Wysyłanie żądanie o teren : moje_id + pozwolenie
			komunikat_odp.jakie_zlecenie = 'T';
			komunikat_odp.nr_terenu = moje_id + pozwolenie;
			x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
			if(x == -1)syserr("firma:Error in msgsnd\n");
			
			// Zwalniam mutex.
			printf("firma %d pr:%d :     V(mutex)\n", id_firmy, moje_id ); 
			pthread_mutex_unlock( &mutex );
			
			// Oczekuję odpowiedzi od delegata.
			x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
					adres_firmy(id_firmy, moje_id), 0);
			if(x == -1)syserr("firma:Error in msgrcv\n");
			pthread_mutex_lock( &mutex );
			printf("firma %d pr:%d :     P(mutex)\n", id_firmy, moje_id ); 
			
			
			if(komunikat.jakie_zlecenie == 'N'){
				// Gdzy nie ma już nic do roboty.
				ilu_robotnikow_pracuje--;
				break /*Idę odpocząć :) */ ;	
			} else if(komunikat.jakie_zlecenie == 'T'){
				// Dostaliśmy działkę do przekopania.
				printf("firma %d pr:%d :     V(mutex)\n", id_firmy, moje_id ); 
				pthread_mutex_unlock( &mutex );
				
				// Pracuję nad wykopaliskiem wynik zapisuje na komunikacie odp.
				PRACUJ(komunikat.symbol_zbioru, &komunikat_odp);
				
				// Skończyłem pracę wysyłam raport
				komunikat_odp.jakie_zlecenie = 'R';
				x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
				if(x == -1)syserr("firma:Error in msgsnd\n");
				
				// Czekam na akceptacje raportu:
				x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
						adres_firmy(id_firmy, moje_id), 0);
				if(x == -1)syserr("firma:Error in msgrcv\n");
				if(komunikat.jakie_zlecenie != 'A')syserr("firma:robotnik źle pracuje\n");
				
				// Proces dodawania zatwierdzonych artefaktów:
				pthread_mutex_lock( &mutex );
				printf("firma %d pr:%d :     P(mutex)\n", id_firmy, moje_id ); 
				for(i = 0; i < komunikat_odp.ilosc_wykopanych_artefaktow; i++)
					kolekcja_firmowa[komunikat_odp.wykopane_artefakty[i]]++;
			}
		}
	}
	return (NULL);
}




int main(int argc, char **argv){
	inituj_komunikacje('F');
	printf("Stworzono firme\n");
	if(argc != 4){
		printf("firma:Podales zla ilosc argumentow.\n");
		return 0;
	}
	
	id_firmy = atoi(argv[1]);
	oplataStala = atoi(argv[2]);
	ograniczenieA = atoi(argv[3]);
	
	// odebranie od banku podstawowych informacji.
	struct kom_1_z_baku_do_firmy komunikant;
	int x = msgrcv(ID_KOLEJKI_BANK_FIRMA, &komunikant, sizeof(komunikant),
				adres_firmy(id_firmy, 0), 0);
	if(x == -1)syserr("firma:Error in msgrcv\n");
	ilosc_pracownikow = komunikant.liczba_pracownikow;
	saldo = komunikant.saldo;
	pozwolenie = -1;
	ilu_robotnikow_pracuje = ilosc_pracownikow;
	
	 pthread_t pth_kierownik, pth_pracownik[ilosc_pracownikow];
	 
	 // Stworzenie kierownika.
	 pthread_create( &pth_kierownik, NULL, &kierownik, NULL);
	ilu_robotnikow_pracuje = ilosc_pracownikow;
	sleep(10);
	// Stworzyć pracowników.
	int i;
	int indeksy[ilosc_pracownikow];
	for(i = 1; i <= ilosc_pracownikow; i++){
		indeksy[i - 1] = i;
		pthread_create( &pth_pracownik[i - 1], NULL, &pracownik, (void *) &indeksy[i - 1]);
	}
	
	// Oddelegj pracownikóœ i kierownika.
	pthread_join(pth_kierownik, NULL);
	for(i = 1; i <= ilosc_pracownikow; i++)
		pthread_join(pth_pracownik[i - 1], NULL);
	
	// Poinformuj że delegat w muzeum jest już niepotrzebny.
	struct kom_do_delegata komunikat_do_delegata;
	komunikat_do_delegata.id_firmy = id_firmy;
	komunikat_do_delegata.jakie_zlecenie = 'K';
	komunikat_do_delegata.nadawca = adres_firmy(id_firmy, 0);
	x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_do_delegata, sizeof(komunikat_do_delegata),0);
	if(x == -1)syserr("firma:Error in msgsnd\n");

	return 0;
}

int czy_pierwsza(int x){
	int i;
	for(i = 2; i*i <= x; i++)
		if(x % i) return 0;
	return 1;
}
void PRACUJ(int symbol_zbioru, struct kom_do_delegata *odp){
	odp->ilosc_wykopanych_artefaktow = 0;
	int p;
	for(p = 2; p <= ograniczenieA; p++)
		if(czy_pierwsza(p))
			while(symbol_zbioru % p == 0){
				odp->wykopane_artefakty[odp->ilosc_wykopanych_artefaktow++] = p;
				symbol_zbioru /= p;
			}
}
