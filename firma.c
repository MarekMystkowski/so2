/// DO POPRAWY:
/// 1) NIspójność w kolejkach delegatów !

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

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

struct kom_do_delegata{
	long id_robotnika;
	// T - żadanie terenu, R - wysłanie raportu
	char jakie_zlecenie;
	int nr_terenu;
	int ilosc_wykopanych_artefaktow, wykopane_artefakty[20];
	
}

struct kom_do_robotnika{
	long id_robotnika;
	// T - przyczielony teren, B - brak pozwolenia, A- akceptacja raportu
	char jakie_zlecenie;
	int symbol_zbioru;
	
}

struct kom_do_muzeum{
	long id_firmy;
	// S- sprzedanie artyfaktów, O- oferta
	char jakie_zlecenie;
	int kolekcja, cena, ilosc_pol;
}

struct kom_do_firmy{
	long id_firmy;
	//  A - akceptacja P- dostanie pozwolenia, N-odmowa pozwolenia.
	char jakie_zlecenie;
	int pozwolenie;
}
	
int id_firmy, oplataStala, ograniczenieA, koncz_prace;
int ilosc_pracownikow, saldo;
int kolekcja_firmowa[1000009];
int liczba_pracownikow;
int pozwolenie; // -1 - brak pozwolenia; wpf: mamy pozwolenie na pola [pozwoleinie,..]
int ilu_robotnikow_pracuje, trwa_sprzedarz, jest_cos_do_sprzedania;

void PRACUJ(int symbol_zbioru, struct kom_do_delegata *odp);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t oczekujacy_robotnicy, oczekujacy_kierownik;


void dodaj_do_kolekcji(int n, int *dane);

void kierownik(){
	struct kom_do_firmy komunikant;
	struct kom_do_muzeum komunikant_odp;
	komunikant_odp.id_firmy = id_firmy;
	komunikant_odp.ilosc_pol = ilosc_pracownikow;
	
	int cena = oplataStala * 3 + ilosc_pracownikow*(3 +ograniczenieA/10000);
	if(cena > saldo) cena = saldo;
	
	pthread_mutex_lock( &mutex );
	int i, j;
	while(1){
		pthread_cond_wait( &oczekujacy_ksiegowy, &mutex );
		if(!koncz_prace){
			/// Sprzedanie kolekcji.
			j = 0;
			for(i = 2; i <= ograniczenieA; i++)
				while( kolekcja_firmowa[i] >= i){

					// wyślij tranzakcje do muzeum.
					komunikant_odp.jakie_zlecenie = 'S';
					komunikant_odp.kolekcja = i;
					int x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
					if(x == -1)syserr("firma:Error in msgsnd\n");
					kolekcja_firmowa[i] -= i;
					saldo+=i;
					j++;
				}
			
			// Odbieranie akceptacji tranzakcji.
			pthread_mutex_unlock( &mutex );
			while(j-- > 0){
				int x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
				typ_komunikatu('F', id_firmy, moje_id), 0);
				if(x == -1)syserr("firma:Error in msgrcv\n");
				assert(komunikat.jakie_zlecenie == 'A');
			}
			
			// Pobranie aktualnego salda od banku. pominięte aktualizowałem ciągle !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			
			// Negocjowanie pozwoleń, blokujemy mutex.
			pthread_mutex_lock( &mutex );
			pozwolenie = -1;
			while(pozwolenie == -1){
				if(saldo < cena && oplataStala < saldo)
					cena = saldo;
					// wysyłanie oferty:
					komunikant_odp.jakie_zlecenie = 'O';
					komunikant_odp.cena = cena;
					int x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
					if(x == -1)syserr("firma:Error in msgsnd\n");
					x = msgrcv(ID_KOLEJKI_FIRM, &komunikat, sizeof(komunikat),
						typ_komunikatu('F', id_firmy, moje_id), 0);
					if(komunikant.jakie_zlecenie == 'P'){
						// Udana tranzakcja :D
						saldo -= cena;
						pozwolenie = komunikant.pozwolenie;
					}else {
						//odmowa :'( 
						if (cena == saldo) break /* nic nie wytarguję już*/ ;
						saldo -= oplataStala;
						cena*=2; // sprubuję podwujną cenę:D
						
					}
			}
		}
		pthread_cond_broadcast(&robotnicy);
		if(koncz_prace){
			pthread_mutex_unlock( &mutex );
			return /*koniec pracy*/;
		}
	}
}
	
void pracownik(void *id){
	int moje_id = (int) id;
	struct kom_do_delegata komunikat_odp;
	struct kom_do_robotnika komunikat;
	
	pthread_mutex_lock( &mutex );
	
	// Jeden obrut pętli to jedno pozwolenie.
	while(1){
		ilu_robotnikow_pracuje--;
		
		// Jeśli nie jestem ostatnim to wieszam się.
		if(ilu_robotnikow_pracuje != 0)
			pthread_cond_wait( &oczekujacy_robotnicy, &mutex );
		else {
			// Może trzeba wybudzić kierownika by zdobył nowe pozwolenie.
			ilu_robotnikow_pracuje++; // dzięki temu nikt tu nie wejdzie gdy ja tu jestem.
			
			// budzenie kierownika
			pthread_cond_signal(&oczekujacy_kierownik);
			pthread_cond_wait( &oczekujacy_robotnicy, &mutex );
			
		} else ilu_robotnikow_pracuje++;
		if(pozwolenie == -1){
			ilu_robotnikow_pracuje--;
			pthread_mutex_unlock( &mutex );
			return /*Kończę pracę :D*/;
		 }
		
		// jeden obrót pętli to jeden teren przekopany:
		while(1){
			
			if(koncz_prace){
				ilu_robotnikow_pracuje--;
				pthread_mutex_unlock( &mutex );
				return /*Kończę pracę :D*/ ;
			}
			
			// Wysyłanie żądanie o teren : moje_id + pozwolenie
			komunikat_odp.id_robotnika = typ_komunikatu('F', id_firmy, moje_id);
			komunikant.jakie_zlecenie = 'T';
			komunikant.nr_terenu = moje_id + pozwolenie;
			int x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
			if(x == -1)syserr("firma:Error in msgsnd\n");
			
			// Zwalniam mutex.
			pthread_mutex_unlock( &mutex );
			
			// Oczekuję odpowiedzi od delegata.
			int x = msgrcv(ID_KOLEJKI_ROBOTNIKOW, &komunikat, sizeof(komunikat),
				typ_komunikatu('F', id_firmy, moje_id), 0);
			if(x == -1)syserr("firma:Error in msgrcv\n");
			pthread_mutex_lock( &mutex );
			
			
			if(komunikant.jakie_zlecenie == 'B'){
				// Gdzy nie ma już nic do roboty.
				ilu_robotnikow_pracuje--;
				break /*Idę odpocząć :) */ ;	
			} else if(komunikant.jakie_zlecenie == 'T'){
				// Dostaliśmy działkę do przekopania.
				pthread_mutex_unlock( &mutex );
				
				// Pracuję nad wykopaliskiem wynik zapisuje na komunikacie odp.
				PRACUJ(komunikant.symbol_zbioru, &komunikat_odp);
				
				// Skończyłem pracę wysyłam raport:
				struct kom_do_delegata komunikat_odp;
				komunikat_odp.id_robotnika = typ_komunikatu('F', id_firmy, moje_id);
				komunikat_odp.jakie_zlecenie = 'R';
				int x = msgsnd(ID_KOLEJKI_DELEGATOW, &komunikat_odp, sizeof(komunikat_odp),0);
				if(x == -1)syserr("firma:Error in msgsnd\n");
				
				// Czekam na akceptacje raportu:
				struct kom_do_robotnika komunikat;
				int x = msgrcv(ID_KOLEJKI_ROBOTNIKOW, &komunikat, sizeof(komunikat),
				typ_komunikatu('F', id_firmy, moje_id), 0);
				if(x == -1)syserr("firma:Error in msgrcv\n");
				if(komunikat.jakie_zlecenie != 'A')syserr("firma:robotnik źle pracuje\n");
				
				// Proces dodawania zatwierdzonych artefaktów:
				pthread_mutex_lock( &mutex );
				for(int i = 0; i < komunikat_odp.ilosc_wykopanych_artefaktow; i++)
					kolekcja_firmowa[komunikat_odp.wykopane_artefakty[i]]++;
			}
		}
	}
}




int main(int argc, char **argv){
	if(argc != 4){
		printf("firma:Podales zla ilosc argumentow.\n");
		return 0;
	}
	
	id_firmy = atoi(argv[1]);
	oplataStala = atoi(argv[2]);
	ograniczenieA = atoi(argv[3]);
	
	// odebranie od banku podstawowych informacji.
	struct kom_1_z_baku_do_firmy komunikant;
	int x = msgrcv(ID_KOLEJKI_BANKU, &komunikat, sizeof(komunikat),
				typ_komunikatu('F', id_firmy, 0), 0);
	if(x == -1)syserr("firma:Error in msgrcv\n");
	ilosc_pracownikow = komunikant.liczba_pracownikow;
	saldo = komunikant.saldo;
	
	ilu_robotnikow_pracuje = liczba_pracownikow;
	
	/// Stworzyć jednego kierownika.
	
	/// Stworzyć pracowników.
	// po stworzeniu pracowników program sam już działa ,
	// po zakończeniu skończą działąć wszystkei procesy same.
	
	
}
