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

int ID_KOL_WSTEPNY_DO_FIRM_Z_BANKU,
ID_KOL_WSTEPNY_DO_MUZEUM_Z_BANKU,
ID_KOL_DO_FIRMY_Z_MUZEUM,
ID_KOL_DO_MUZEUM_Z_FIRMY,
ID_KOL_DO_BANKU_Z_MUZEUM,
ID_KOL_DO_MUZEUM_Z_BANKU,
ID_KOL_DO_RAPORTOW;
void syserr(const char *fmt, ...);
void inituj_komunikacje(char dla_kogo){
	ID_KOL_WSTEPNY_DO_FIRM_Z_BANKU =msgget(1, IPC_CREAT|S_IRWXU);
	ID_KOL_WSTEPNY_DO_MUZEUM_Z_BANKU =msgget(2, IPC_CREAT|S_IRWXU);
	ID_KOL_DO_FIRMY_Z_MUZEUM =msgget(3, IPC_CREAT|S_IRWXU);
	ID_KOL_DO_MUZEUM_Z_FIRMY =msgget(4, IPC_CREAT|S_IRWXU);
	ID_KOL_DO_BANKU_Z_MUZEUM =msgget(5, IPC_CREAT|S_IRWXU);
	ID_KOL_DO_MUZEUM_Z_BANKU =msgget(6, IPC_CREAT|S_IRWXU);
	ID_KOL_DO_RAPORTOW =msgget(7, IPC_CREAT|S_IRWXU);
}

void usun_komunikacje(){
	 if(msgctl(ID_KOL_WSTEPNY_DO_FIRM_Z_BANKU, IPC_RMID, 0) == -1)
		syserr("msgctl");
	if(msgctl(ID_KOL_WSTEPNY_DO_MUZEUM_Z_BANKU, IPC_RMID, 0) == -1)
		syserr("msgctl");
	if(msgctl(ID_KOL_DO_FIRMY_Z_MUZEUM, IPC_RMID, 0) == -1)
		syserr("msgctl");
	if(msgctl(ID_KOL_DO_MUZEUM_Z_FIRMY, IPC_RMID, 0) == -1)
		syserr("msgctl");
	if(msgctl(ID_KOL_DO_BANKU_Z_MUZEUM, IPC_RMID, 0) == -1)
		syserr("msgctl");
	if(msgctl(ID_KOL_DO_MUZEUM_Z_BANKU, IPC_RMID, 0) == -1)
		syserr("msgctl");
	if(msgctl(ID_KOL_DO_RAPORTOW, IPC_RMID, 0) == -1)
		syserr("msgctl");
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
	long typ_banku;
	long odbiorca;
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

struct kom_raport{
	long nr_firmy;
	int ile, koniec;
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

