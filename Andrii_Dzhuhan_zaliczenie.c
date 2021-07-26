//  -- Projekt zaliczeniowy --
//  -- Autor: Andrii Dzhuhan --

//  -- szybkość transmisji 2400 bps --
//  -- liczba bitow pomiędzy bajtami 1 --
//  -- brak bitu parzystości --


//  ----  Dyrektywy preprocesora  ----
#define true 1
#define false 0
#define s7on P1_6
#define tled P1_7
#define MUXK P3_5
#include <8051.h>



// ----  Deklaracja zmiennych  ----
//  --  zegarek  --
char sec;
char min;
char hour;
unsigned char lp;					//licznik przerwań
unsigned char fp;					//flaga przerwaniowa
unsigned char index;				//index wyświetlacza
unsigned char WYBW;                 //wybrany wyswietlacz
__xdata unsigned char *CSDS = (__xdata unsigned char*) 0xFF30;
__xdata unsigned char *CSDB = (__xdata unsigned char*) 0xFF38;
char tabc[6];                       //tablica cyfr
__code unsigned char CYFRY[16] =    //cyfry
{
    0b00111111, 0b00000110, 0b01011011, 0b01001111,
    0b01100110, 0b01101101, 0b01111101, 0b00000111,
    0b01111111, 0b01101111
};

//  --  transmisja  --
unsigned char get;			
unsigned char edi;			//tryb edycji
unsigned char rbindx;       //index buforu
unsigned char rcvbuf[14];   //bufor danych

//  --  klawiatura & edycja  --
__xdata unsigned char *ADDR = (__xdata unsigned char*) 0xFF22;
unsigned char blink_time;	//czas migania krópki
char section;				//modyfikacja godzin/minut/sekund
short pos;					//pozycja w historii
unsigned char kmat_cond;	//stan klawiatury matrycowej
unsigned char cond;			//stan klawiatury multipleksowanej
unsigned char bool0;		//czy nacisnięty klawisz na klawiatury matrycowej
unsigned char bool1;		//czy nacisnięty klawisz na klawiatury multipleksowanej
unsigned char blink;		//miganie

//  --  wyswietlacz LCD  --
__xdata unsigned char *lcdwc = (__xdata unsigned char*) 0xFF80;
__xdata unsigned char *lcdwd = (__xdata unsigned char*) 0xFF81;
__xdata unsigned char *lcdrc = (__xdata unsigned char*) 0xFF82;
__xdata unsigned char *cmd_h = (__xdata unsigned char*) 0x4000;
unsigned char cmd_copy[12];	//kopia rozkazu
unsigned char err;			//czy rozkaz jest poprawny
unsigned char lcdc;			//licznik do pola LCD
unsigned char hisc;
unsigned char lcd;



//  ----  Deklarację funkcji  ----
//  --  inicializacja  --
void init();

//  -- zegarek  --
void refresh();
void refresh_clock();

//  --  klawiatura  --
void input();
void kmat();

//  --  transmisja --
void command();

//  --  wyświetlacz LCD  --
void lwwb();
void lcdcwt(char a);
void lcddwt(char a);
void write_cmd(char h);
void write();
void write_st();
void shift();

//  --  obsługa przerwania od T0  --
void t0_int(void) __interrupt(1);



//  ----  Main  ----
void main()
{
    init();
    while(true)
    {
        refresh();
        refresh_clock();
        command();
        kmat();
    }
}



//  ---- Realizacja funkcji  ----
//  -- inicializacja programu  --
void init()
{
    //  --  zegarek  --
    TMOD &= 0b11110001;
    TMOD |= 0b00000001;
    WYBW = 0b00000001;   //wybrany wyswietlacz
    s7on = true;
    TR0 = true;
    ET0 = true;
    EA = true;
    tabc[0] = 0;    	//000000
    tabc[1] = 0;
    tabc[2] = 0;
    tabc[3] = 0;
    tabc[4] = 0;
    tabc[5] = 0;
    TH0 = 232;
    TL0 = 0;
    index = 0;
    sec = 0;
    min = 0;
    hour = 0;
    lp = 150;

    //  --  transmisja  --
    //MO M1 M2 REN TB8 RB8 TI RI
    SCON = 0b01010000; //M0=0, M1=1, M2=0, REN=1, TB8=0, RB8=0, TI=0, RI=0
    //GATE1 CT1 T1M1 T1M0 GATE0 CT0 T0M1 T0M0
    TMOD &= 0b00101111; //GATE1=0, CT1=0, T1M1=?, T1M0=0, GATE0=?, CT0=?, T0M1=?, T0M0=?
    TMOD |= 0b00100000; //GATE1=?, CT1=?, T1M1=1, T1M0=?, GATE0=?, CT0=?, T0M1=?, T0M0=?
    PCON &= 0b01111111;
    TL1 = 0xff;
    TH1 = 0xf4;
    TF1 = false;
    TR1 = true;
    get = false;
    edi = false;
    rbindx = 0;
    
    //  --  klawiatura  --
    blink = false;
    bool0 = false;
    bool1 = false;
    blink_time = 0;
    section = 0;
    cond = 0;
    kmat_cond = 0;
    pos = 0;

    //  --  wyswietlacz LCD  --
    for(lcdc = 0; lcdc < 128; lcdc++)
        *(cmd_h + lcdc) = 0x20;
    lcdcwt(0b00000001);
    lcdcwt(0b00000110);
    lcdcwt(0b00001100);
    lcdcwt(0b00111000);
    err = false;
    lcdc = 0;
    hisc = 0;
    lcd = 0;
}

//  --  odswiezanie wyswietlacza siedmiosegmentowego  --
void refresh()
{
    for(index = 0; index < 6; index++)
    {
        s7on = true;
        *CSDS = WYBW;
        *CSDB = CYFRY[tabc[index]];
        if(index == 2 || index == 4 || index == 0)	//dopisujemy kropki
        {
            if((index == (section * 2)) && blink)	//wlączamy miganie, jeżeli
                *CSDB = CYFRY[tabc[index]];			//jesteśmy w trybie edycji
            else
                *CSDB = CYFRY[tabc[index]] | 0b10000000;
        }
        s7on = false;
        input();									//sprawdzamy stan klawiatury
        if(MUXK == false && cond == WYBW)
        {
            bool1 = true;
            cond = 0;
        }
        WYBW = WYBW << 1;
        if(WYBW == 0b01000000)
            WYBW = 0b00000001;
    }
}

//  --  odswiezanie zegarka  --
void refresh_clock()
{
    if(fp && !edi)			//jeżeli następiło przerwanie od T0
    {						//oraz nie jesteśmy w trybie edycji, wtedy
        fp = false;			//kasujemy flagę przerwaniową ta
        tabc[0] = sec%10;	//ladujemy do tablicy cyfr
        tabc[1] = sec/10;	//aktualny czas
        tabc[2] = min%10;
        tabc[3] = min/10;
        tabc[4] = hour%10;
        tabc[5] = hour/10;
        
        sec++;
        if(sec >= 60)
        {
            sec = 0;
            min++;
        }
        if(min >= 60)
        {
            min = 0;
            hour++;
        }
        if(hour >= 24)
            hour = 0;
    }
}

//  --  wykonywanie rozkazow  --
void command()
{
    if(TI)
    {
        TI = false;
        if(get)
        {						
            RI = true;
            if(rbindx >= 8)
            {
                rbindx = 0;
                get = false;
                RI = false;
            }
        }
    }

    if(RI)
    {
        RI = false;
        if(!get && !TI)
        {   
            rcvbuf[rbindx] = SBUF;
            if(lcdc < 12)
            {
                *(cmd_h + (lcdc + hisc)) = rcvbuf[rbindx];
                lcdc++;
            }
        }

        if(rcvbuf[rbindx] == 0xA)	//jeżeli napotkaliśmy symbol nowej linii
        {							//sprawdzamy poprawność rozkazu
            err = false;
            if(rcvbuf[0] == 'S' && rcvbuf[1] == 'E' && rcvbuf[2] == 'T'
                && rcvbuf[3] == ' ' && rcvbuf[6] >= '.' && rcvbuf[9] <= '.'
                && rcvbuf[12] == 0xD && rcvbuf[13] == 0xA)
            {		//SET
                rbindx = -1;
                hour = ((rcvbuf[4] - 48) * 10) + (rcvbuf[5] - 48);
                min = ((rcvbuf[7] - 48) * 10) + (rcvbuf[8] - 48);
                sec = ((rcvbuf[10] - 48) * 10) + (rcvbuf[11] - 48);
                if(sec < 0 || min < 0 || hour < 0
                    || sec > 59 || min > 59 || hour > 23)
                {
                    sec = tabc[0] + (tabc[1] * 10);
                    min = tabc[2] + (tabc[3] * 10);
                    hour = tabc[4] + (tabc[5] * 10);
                    err = true;
                }
                lp = 150;
            }
            else if(rcvbuf[0] == 'G' && rcvbuf[1] == 'E' && rcvbuf[2] == 'T'
                && rcvbuf[3] == 0xD && rcvbuf[4] == 0xA)
            {		//GET
                get = true;
                rbindx = 0;
                write_st();
                write();
           `    rcvbuf[0] = tabc[5] + 48;
                rcvbuf[1] = tabc[4] + 48;
                rcvbuf[2] = '.';
                rcvbuf[3] = tabc[3] + 48;
                rcvbuf[4] = tabc[2] + 48;
                rcvbuf[5] = '.';
                rcvbuf[6] = tabc[1] + 48;
                rcvbuf[7] = tabc[0] + 48;
            }
            else if(rcvbuf[0] == 'E' && rcvbuf[1] == 'D'
                && rcvbuf[2] == 'I' && rcvbuf[3] == 'T'
                && rcvbuf[4] == 0xD && rcvbuf[5] == 0xA)
            {		//EDIT
                edi = true;
                section = 0;
                rbindx = -1;
            }
            else if(rcvbuf[0] == 'C' && rcvbuf[1] == 'O' && rcvbuf[2] == 'N'
                && rcvbuf[3] == 'F' && rcvbuf[4] == 'I' && rcvbuf[5] == 'R'
                && rcvbuf[6] == 'M' && rcvbuf[7] == 0xD && rcvbuf[8] == 0xA)
            {		//CONFIRM
                sec = tabc[0] + (tabc[1] * 10);
                min = tabc[2] + (tabc[3] * 10);
                hour = tabc[4] + (tabc[5] * 10);
                edi = false;
                blink = false;
                rbindx = -1;
            }
            else if(rcvbuf[0] == 'R' && rcvbuf[1] == 'E' && rcvbuf[2] == 'S'
                && rcvbuf[3] == 'E' && rcvbuf[4] == 'T' && rcvbuf[5] == 0xD
                && rcvbuf[6] == 0xA)
            {		//RESET
                shift();
                init();
                return;
            }
            else
            {		//ERROR
                rbindx = -1;
                err = true;
            }

            if(!get)
            {
                write_st();
                write();
            }
        }

        if(get && !TI && !RI)
            SBUF = rcvbuf[rbindx];
        rbindx++;
    }
    if(rbindx == 14)
        rbindx = 0;
}

//  --  edycja --
void input()
{
    if(MUXK == true)
    {
        if(cond == 0)
            bool1 = true;
        cond = WYBW;
    }
    if(bool1 == false)
        return;

    switch(cond)
    {	//akceptujemy zmianę
        case 0b00000001:
            if(edi)
            {
                sec = tabc[0] + (tabc[1] * 10);
                min = tabc[2] + (tabc[3] * 10);
                hour = tabc[4] + (tabc[5] * 10);
                blink = false;
                edi = false;
                lp = 150;
            }
            break;
		//opuszczamy tryb edycji bez akceptacji zmian
        case 0b00000010:
            edi = false;
            blink = false;
            break;
		//strzałka w prawo
        case 0b00000100:
            if(edi == false)
            {
                edi = true;
                section = 1;
            }
            section--;
            if(section < 0)
                section = 2;
            blink_time = 0;
            break;
		//zwiększamy wartość wybranej sekcji
        case 0b00001000:
            if(edi)
            {
                if(section == 0)
                {
                    tabc[0]++;
                    if(tabc[0] == 10)
                    {
                        tabc[0] = 0;
                        tabc[1]++;
                    }
                    if(tabc[1] == 6)
                        tabc[1] = 0;
                }
                else if(section == 1)
                {
                    tabc[2]++;
                    if(tabc[2] == 10)
                    {
                        tabc[2] = 0;
                        tabc[3]++;
                    }
                    if(tabc[3] == 6)
                        tabc[3] = 0;
                }
                else if(section == 2)
                {
                    tabc[4]++;
                    if(tabc[4] == 10)
                    {
                        tabc[4] = 0;
                        tabc[5]++;
                    }
                    if(tabc[4] == 4 && tabc[5] == 2)
                    {
                        tabc[4] = 0;
                        tabc[5] = 0;
                    }
                }
            }
            break;
		//zmniejszamy wartość wybranej sekcji
        case 0b00010000:
            if(edi)
            {
                if(section == 0)
                {
                    tabc[0]--;
                    if(tabc[0] < 0)
                    {
                        tabc[0] = 9;
                        tabc[1]--;
                    }
                    if(tabc[1] < 0)
                        tabc[1] = 5;
                }
                else if(section == 1)
                {
                    tabc[2]--;
                    if(tabc[2] < 0)
                    {
                        tabc[2] = 9;
                        tabc[3]--;
                    }
                    if(tabc[3] < 0)
                        tabc[3] = 5;
                }
                else if(section == 2)
                {
                    tabc[4]--;
                    if(tabc[4] < 0)
                    {
                        tabc[4] = 9;
                        tabc[5]--;
                    }
                    if(tabc[5] < 0)
                    {
                        tabc[4] = 3;
                        tabc[5] = 2;
                    }
                }
            }
            break;
		//strzałka w lewo
        case 0b00100000:
            if(edi == false)
            {
                edi = true;
                section = 2;
            }
            section++;
            if(section > 2)
                section = 0;
            blink_time = 0;
            break;
    }

    if(cond != 0)
        bool1 = false;
}

//  --  przewijanie rozkasow  --
void kmat()
{
    kmat_cond = *ADDR;
    if(bool0)
    {
        if(kmat_cond == 0b11101111)
        {
            pos -=16;
            if(pos >= 0)
                write_cmd(pos);
        }
        else if(kmat_cond == 0b11011111)
        {
            pos += 16;
            if(pos <= 80)
                write_cmd(pos);
        }
    }
    if(kmat_cond != 0b11111111)
    {
        bool0 = false;
        if(pos < 0)
            pos = 0;
        if(pos > 80)
            pos = 80;
    }
    else
        bool0 = true;
}

//  --  oczekiwanie na gotowność wyświetlacza LCD  --
void lwwb()
{
    while(true)
    {
        lcd = *lcdrc;
        lcd &= 0b10000000;
        if(lcd == 0b00000000)
            break;
    }
}

//  --  wpisywanie odebranych rozkazów do LCD --
void lcdcwt(char a)
{
    lwwb();
    *lcdwc = a;
}

//  -- wpisywanie danych do LCD  --
void lcddwt(char a)
{
    lwwb();
    *lcdwd = a;
}

//  --  dopisywanie statusu rozkazu  --
void write_st()
{
    while(lcdc <= 13)
    {
        if(lcdc < 13)
            *(cmd_h + (lcdc + hisc)) = ' ';
        else
        {
            if(err)
            {
                *(cmd_h + (lcdc + hisc)) = 'E';
                *(cmd_h + ((lcdc + 1) + hisc)) = 'R';
                *(cmd_h + ((lcdc + 2) + hisc)) = 'R';
            }
            else
            {
                *(cmd_h + (lcdc + hisc)) = ' ';
                *(cmd_h + ((lcdc + 1) + hisc)) = 'O';
                *(cmd_h + ((lcdc + 2) + hisc)) = 'K';
            }
        }
        lcdc++;
    }
    lcdc = 0;
}

//  --  wypisywanie rozkazow na LCD & odswiezanie historii  --
void write()
{
    if(hisc == 0)
    {
        write_cmd(hisc);
        hisc += 16;
    }
    else if(hisc == 16)
    {
        hisc -= 16;
        write_cmd(hisc);
        hisc += 32;
    }
    else if(hisc > 16)
    {
        hisc -= 16;
        write_cmd(hisc);
        hisc += 32;
        if(hisc == 128)
        {
            for(hisc = 0; hisc <= 112; hisc++)
                *(cmd_h + (lcdc + hisc)) = *(cmd_h + (lcdc + (hisc + 16)));
            hisc = 112;
            pos = 80;
        }
    }
}

//  --  wypisywanie rozkazow  --
void write_cmd(char h)
{	//pierwsza linija
    for(lcdc = 0; lcdc < 16; lcdc++)
    {
        if((*(cmd_h + (lcdc + h)) != 0xD) && (*(cmd_h + (lcdc + h)) != 0xA))
            lcddwt(*(cmd_h + (lcdc + h)));
        else
            lcddwt(' ');
    }
    shift();	//przesuwanie do następnej liniji
	//druga linija
    for(lcdc = 0; lcdc < 16; lcdc++)
    {
        if((*(cmd_h + (lcdc + (h + 16))) != 0xD) && (*(cmd_h + (lcdc + (h + 16))) != 0xA))
            lcddwt(*(cmd_h + (lcdc + (h + 16))));
        else
            lcddwt(' ');
    }
    shift();	//przesuwanie na początek
    lcdc = 0;
}

//  --  przesuwanie pola LCD  --
void shift()
{
    for(lcdc = 0; lcdc < 24; lcdc++)
        lcddwt(' ');
        lcdc = 0;
}



//  ----  Obsługa przerwania od T0  ----
void t0_int(void) __interrupt(1)
{
    TH0 = 232;
    if(lp >= 150)		//jeżeli licznik przerwań większy
    {					//lub równy 150,
        fp = true;		//ustawiamy flagę przerwaniową
        lp = 0;			//zerujemy licznik przerwań
    }
    if(!edi)			//jeżeli nie jesteśmy w trybie edycji
        lp++;			//incrementujemy licznik
    if(edi)
    {					//miganie kropki
        blink_time++;	
        if(blink_time == 80)
        {
            blink_time = 0;
            blink = !blink;
        }
    }
}