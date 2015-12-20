#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int ID_KOLEJKI_DELEGATOW; // kolejka do pisania do delegatów.
int ID_KOLEJKI_BANK_MUZEUM; // kolejka między bankiem a muzeum;
int ID_KOLEJKI_FIRM; // Kolejka do pisania dla robotników i kierowników.
int ID_KOLEJKI_BANK_FIRMA;
void inituj_komunikacje(char dla_kogo){
	ID_KOLEJKI_DELEGATOW = msgget(101, IPC_CREAT|S_IRWXU);
	ID_KOLEJKI_BANK_MUZEUM = msgget(102, IPC_CREAT|S_IRWXU);
	ID_KOLEJKI_FIRM = msgget(103, IPC_CREAT|S_IRWXU);
	ID_KOLEJKI_BANK_FIRMA = msgget(104, IPC_CREAT|S_IRWXU);
}
long adres_firmy(int id_firmy, int id_robotnika){
	return (long) id_firmy * (long)1000000 + (long)id_robotnika;
}
int id_robotnika_z_adresu(long adres, int id_firmy){
	return (int) (adres % 1000000);
	
}


struct kom_1_z_baku_do_firmy{
	long adresat_komunikatu; // typ.
	int saldo, liczba_pracownikow;
};

struct kom_do_banku{
	long id_konta; // typ.
	// I - informaca o saldzie, P - wykonanie przelewu, Z - bank ma zakończyć działanie.
	char jakie_zlecenie; 
	int kwota, id1, id2; // przelewy wykonywane są z id1-->id2 

};

struct kom_z_banku{
	long adresat_komunikatu; // typ.
	int stan_konta, akceptacja_tranzakcji;
};

struct kom_do_delegata{
	long id_firmy;
	// Z- zwolnienie terenu, S- sprzedanie artyfaktów, O- oferta zezwolenia,
	// T- prośba o teren do kopania, R- raport z wykopaliska, K- firma kończy działalnosc.
	char jakie_zlecenie;
	long nadawca;
	int kolekcja, cena, ilosc_pol;
	int ilosc_wykopanych_artefaktow, wykopane_artefakty[32], nr_terenu;
};

struct kom_do_firmy{
	long id_adresata;
	//  A- akceptacja, N- brak akceptacji, P- dostanie pozwolenia,
	// T- przekazanie terenu do kopania, Z- zwalnienie pozwolenia.
	char jakie_zlecenie;
	int symbol_zbioru, pozwolenie;
};

struct kom_1_z_banku_do_muzeum{
	long nr_porcja;
	int ilosc_firm;
	int rozmiar_porcji;
	int dane[100];
};

void syserr(const char *fmt, ...)  
{
  va_list fmt_args;

  fprintf(stderr, "ERROR: ");

  va_start(fmt_args, fmt);
  vfprintf(stderr, fmt, fmt_args);
  va_end (fmt_args);
  fprintf(stderr," (%d; %s)\n", errno, strerror(errno));
  exit(1);
}

