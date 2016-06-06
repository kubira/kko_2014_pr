/*******************************************************************************
*  Soubor:   gif2bmp.c                                                         *
*  Autor:    Radim Kubiš, xkubis03                                             *
*  Vytvořen: 8. března 2014                                                    *
*                                                                              *
*  Projekt do předmětu Kódování a komprese (KKO) 2014                          *
*                                                                              *
*                    KONVERZE OBRAZOVÉHO FORMÁTU GIF NA BMP                    *
*                   ----------------------------------------                   *
*                                                                              *
*  Knihovna  obsahující  funkci (funkce) pro  převod  grafického  formátu GIF  *
*  na grafický formát BMP (24bpp). Předpokládá se GIF verze GIF89a a komprese  *
*  pomocí metody LZW.                                                          *
*                                                                              *
*  Převod podporuje:                                                           *
*    - 1-8 bit GIF,                                                            *
*    - prokládání,                                                             *
*    - lokální i globální tabulky barev,                                       *
*    - přepočet pozic a rozměru image bloků                                    *
*    - převod animace (výstupem je pouze poslední frame).                      *
*                                                                              *
*  Výstupem je BMP kódované 24 bity na pixel (24bpp).                          *
*                                                                              *
*******************************************************************************/

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include "gif2bmp.h"

// Informace z hlavičky vstupního souboru
tGIFInfo info = {0, 0, 0, 0, 0, 0, 0, 0};
// Ukazatel pro globální tabulku barev
tRGB *globalColorTable = NULL;
// Ukazatel pro lokální tabulku barev
tRGB *localColorTable = NULL;
// Vstupní soubor (GIF)
FILE *inputGIFFile = NULL;
// Výstupní soubor (BMP)
FILE *outputBMPFile = NULL;
// Ukazatel pro obrazová data v image bloku
char *data = NULL;
// Ukazatel na následující volný bajt při ukládání dat do *data
uint32_t dataIndex = 0;
// Ukazatel na aktuálně zpracovávaný bajt v *data
uint32_t actualIndex = 0;
// Proměnná pro minimální velikost LZW kódu
uint8_t LZWMininumCodeSize = 0;
// Proměnná pro BMP obrazová data
tRGB **dataBMP = NULL;
// Proměnná pro následující index tabulky barev pixelů
uint32_t nextPixelIndex = 0;
// Proměnná pro dorovnání indexu do tabulky nových indexů
uint32_t eoiIndex = 0;
// Proměnná pro velikost GIF souboru
uint64_t gifSize = 0;
// Proměnná pro velikost aktuálně používané tabulky barev
uint16_t actualColorTableSize = 0;
// Ukazatel na aktuálně používanou tabulku barev
tRGB *actualColorTable = NULL;
// Proměnná pro pořadové číslo image bloku
uint8_t imageBlockNumber = 0;
// Proměnná pro příznak průhlednosti
uint8_t blockTrasparentColorFlag = 0;
// Proměnná pro index průhledné barvy
uint8_t transparentColorIndex = 0;
// Horní index image bloku
uint32_t actualTop = 0;
// Levý index image bloku
uint32_t actualLeft = 0;
// Šířka image bloku
uint32_t actualWidth = 0;
// Výška image bloku
uint32_t actualHeight = 0;
// Proměnná pro příznak prokládání
uint8_t blockInterlaceFlag = FLAG_FALSE;
// Proměnná pro aktuální proložený řádek
uint32_t actualInterlaceRow = 0;
// Proměnná pro aktuální stav prokládání
uint8_t interlaceState = STATE0;

/*
 * Funkce pro získání souřadnice řádku zpracovávaného bloku
 */
uint32_t getRowIndex() {
    // Proměnná pro návratovou hodnotu
    uint32_t rowIdx = 0;

    // Pokud je nastaven příznak prokládání
    if(blockInterlaceFlag == FLAG_TRUE) {
        // Aktuální řádek se bude řídit prokládáním
        rowIdx = actualInterlaceRow + actualTop;

        // Pokud je aktuální index na konci řádku
        if(nextPixelIndex > 0 && ((nextPixelIndex + 1) % actualWidth) == 0) {
            // Posunutí indexu řádku na další proložený
            switch(interlaceState) {
                // Prokládání 8n
                case STATE0: {
                    // Posun na další řádek o 8
                    actualInterlaceRow += 8;
                    // Konec větve
                    break;
                }
                // Prokládání 8n+4
                case STATE1: {
                    // Posun na další řádek o 8
                    actualInterlaceRow += 8;
                    // Konec větve
                    break;
                }
                // Prokládání 4n+2
                case STATE2: {
                    // Posun na další řádek o 4
                    actualInterlaceRow += 4;
                    // Konec větve
                    break;
                }
                // Prokládání 2n+1
                case STATE3: {
                    // Posun na další řádek o 2
                    actualInterlaceRow += 2;
                    // Konec větve
                    break;
                }
            }

            // Pokud číslo řádku překročilo aktuální výšku bloku
            if(actualInterlaceRow >= actualHeight) {
                // Posun na další stav prokládání
                interlaceState++;

                // Nastavení iniciální hodnoty podle nového stavu prokládání
                switch(interlaceState) {
                    // Pro stav 8n
                    case STATE0: {
                        // Prokládání začíná na 0
                        actualInterlaceRow = 0;
                        // Konec větve
                        break;
                    }
                    // Pro stav 8n+4
                    case STATE1: {
                        // Prokládání začíná na 4
                        actualInterlaceRow = 4;
                        // Konec větve
                        break;
                    }
                    // Pro stav 4n+2
                    case STATE2: {
                        // Prokládání začíná na 2
                        actualInterlaceRow = 2;
                        // Konec větve
                        break;
                    }
                    // Pro stav 2n+1
                    case STATE3: {
                        // Prokládání začíná na 1
                        actualInterlaceRow = 1;
                        // Konec větve
                        break;
                    }
                }
            }
        }
    } else {
        // Pokud není GIF prokládaný

        // Výpočet čísla řádku bez prokládání
        rowIdx = ((nextPixelIndex / actualWidth) + actualTop);
    }

    // Navrácení souřadnice řádku bloku
    return rowIdx;
}

/*
 * Funkce pro získání souřadnice sloupce zpracovávaného bloku
 */
uint32_t getColIndex() {
    // Výpočet a navrácení souřadnice sloupce bloku
    return ((nextPixelIndex % actualWidth) + actualLeft);
}

/*
 * Funkce pro načtení a navrácení hodnoty jednoho bajtu ze vstupního GIF souboru
 */
int getByte() {
    // Načtení jednoho bajtu
    int bajt = fgetc(inputGIFFile);

    // Kontrola, zda nebylo dosaženo konce souboru předčasně
    if(bajt == EOF) {
        // Výpis chybového hlášení
        fprintf(stderr, "ERROR: End of file reached.\n");
        // Ukončení programu s chybou
        exit(RETURN_FAILURE);
    }

    // Inkrementace velikosti GIF
    gifSize++;
    // Navrácení hodnoty bajtu
    return bajt;
}

/*
 * Funkce pro zápis 2 bajtů v little-endian formátu do výstupního souboru
 *
 * bytes - 2 bajty pro zápis do souboru
 */
void write2Bytes(uint16_t bytes) {
    // Získání spodního bajtu pro zápis
    char byteToWrite = (bytes % BYTE_OVERFLOW);
    // Zápis spodního bajtu do souboru
    fprintf(outputBMPFile, "%c", byteToWrite);
    // Získání horního bajtu pro zápis
    byteToWrite = (bytes / BYTE_OVERFLOW);
    // Zápis horního bajtu do souboru
    fprintf(outputBMPFile, "%c", byteToWrite);
}

/*
 * Funkce pro zápis 4 bajtů v little-endian formátu do výstupního souboru
 *
 * bytes - 4 bajty pro zápis do souboru
 */
void write4Bytes(uint32_t bytes) {
    // Pomocná proměnná pro jeden bajt k zápisu
    char byte = 0;
    // Cyklus získání a zápisu 4 bajtů do souboru
    for(uint8_t step = 0; step < 4; step++) {
        // Získání dalšího (vyššího) bajtu pro zápis
        byte = (bytes % BYTE_OVERFLOW);
        // Zápis bajtu do souboru
        fprintf(outputBMPFile, "%c", byte);
        // Získání zbylých bajtů k zápisu
        bytes = bytes / BYTE_OVERFLOW;
    }
}

/*
 * Funkce pro inicializaci jedné položky tabulky indexů barev
 *
 * item - položka k inicializaci
 */
void initTableItem(tItem *item) {
    // Nastavení aktuální alokované paměti v položce
    item->allocated = ITEM_ALLOC_SIZE;
    // Nastavení počtu využitých bajtů v položce
    item->used = 0;
    // Alokace pole indexů do tabulky barev
    item->indexList = (uint8_t*)malloc(item->allocated * sizeof(uint8_t));
    // Kontrola alokace
    if(item->indexList == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: item->indexList malloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }
}

/*
 * Funkce pro zvětšení alokovaného prostoru v položce tabulky indexů barev
 *
 * item - položka pro zvětšení alokovaného prostoru
 */
void resizeTableItem(tItem *item) {
    // Zvýšení počtu alokovaných bajtů v položce
    item->allocated += ITEM_ALLOC_SIZE;
    // Realokace alokovaného prostoru pro pole indexů do tabulky barev
    item->indexList = (uint8_t*)realloc(item->indexList, item->allocated * sizeof(uint8_t));
    // Kontrola realokace
    if(item->indexList == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: item->indexList realloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }
}

/*
 * Funkce pro uvolnění alokované paměti položky tabulky indexů barev
 *
 * item - položka pro uvolnění paměti
 */
void freeTableItem(tItem *item) {
    // Uvolnění paměti po seznamu indexů do tabulky barev v položce
    free(item->indexList);
}

/*
 * Funkce pro přidání jednoho indexu do seznamu indexů tabulky barev v položce
 *
 * item  - položka pro přidání indexu
 * index - přidávaný index
 */
void addByteToTableItem(tItem *item, uint8_t index) {
    // Pokud je seznam indexů v položce plný
    if(item->allocated == item->used) {
        // Zvětšení prostoru pro indexy
        resizeTableItem(item);
    }
    // Uložení nového indexu do pole v položce
    item->indexList[item->used] = index;
    // Zvýšení počtu využitých míst v poli indexu položky
    item->used++;
}

/*
 * Funkce pro inicializaci tabulky indexů barev
 *
 * table - tabulka pro inicializaci
 */
void initTable(tTable *table) {
    // Nastavení aktuální alokované paměti v tabulce
    table->allocated = TABLE_ALLOC_SIZE;
    // Nastavení počtu využitých míst v tabulce
    table->used = 0;
    // Alokace místa pro položky tabulky
    table->itemList = (tItem*)malloc(table->allocated * sizeof(tItem));
    // Kontrola alokace
    if(table->itemList == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: table->itemList malloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }
}

/*
 * Funkce pro zvětšení místa pro položky tabulky indexů barev
 *
 * table - tabulka pro zvětšení místa
 */
void resizeTable(tTable *table) {
    // Zvýšení počtu alokovaných míst v tabulce
    table->allocated += TABLE_ALLOC_SIZE;
    // Realokace prostoru pro položky tabulky
    table->itemList = (tItem*)realloc(table->itemList, table->allocated * sizeof(tItem));
    // Kontrola realokace
    if(table->itemList == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: table->itemList realloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }
}

/*
 * Funkce pro uvolnění alokované paměti celé tabulky indexů barev
 *
 * table - tabulka pro uvolnění
 */
void freeTable(tTable *table) {
    // Cyklus pro uvolňování paměti po položkách tabulky
    for(uint32_t itemIndex = 0; itemIndex < table->used; itemIndex++) {
        // Uvolnění paměti po jedné položce tabulky
        freeTableItem(&(table->itemList[itemIndex]));
    }
    // Uvolnění místa po položkách v tabulce
    free(table->itemList);
}

/*
 * Funkce pro vložení nové položky do tabulky indexů barev
 *
 * table - tabulka, do které se vkládá nová položka
 * item  - nová vkládaná položka
 */
void insertNewItem(tTable *table, tItem *item) {
    // Pokud jsou obsazená všechna místa v tabulce
    if(table->allocated == table->used) {
        // Zvětšení místa pro další položky
        resizeTable(table);
    }
    // Uložení nové položky do tabulky
    table->itemList[table->used] = *item;
    // Zvýšení počtu využitých položek v tabulce
    table->used++;
}

/*
 * Funkce pro získání položky z tabulky indexů barev podle indexu
 *
 * table - tabulka s položkami
 * index - index požadované položky
 */
tItem *getItemByIndex(tTable *tab, uint32_t index) {
    // Úprava indexu do tabulky - indexy následují až po EOI
    index = index - eoiIndex;
    // Navrácení požadované položky
    return &(tab->itemList[index]);
}

/*
 * Funkce pro kontrolu správné/podporované signatury vstupního souboru
 *
 * Návratová hodnota:
 *         0 - bez chyby
 *        -1 - nastala chyba
 */
int checkGIFSignature() {
    // Index signatury vstupního souboru
    uint8_t inputSignatureIndex = 0;
    // Pole pro uložení vstupní signatury souboru
    char fileSignature[GIF_SIGNATURE_LENGTH + 1];

    // Tisk informace o aktuální operaci funkce
    // fprintf(stderr, "INFO: Checking GIF signature\n");

    // Načtení signatury vstupního souboru
    for(inputSignatureIndex = 0; inputSignatureIndex < GIF_SIGNATURE_LENGTH; inputSignatureIndex++) {
        // Načtení jednoho znaku signatury vstupního souboru do pole
        fileSignature[inputSignatureIndex] = getByte();
    }
    // Ukončení pole signatury vstupního souboru
    fileSignature[inputSignatureIndex] = '\0';

    // Porovnání signatury vstupního souboru a podporované signatury
    if(strcmp(GIF_SIGNATURE, fileSignature) == 0) {
        // Signatura vstupního souboru je podporovaná
        // fprintf(stderr, "INFO: GIF signature: OK\n");
        return RETURN_SUCCESS;
    } else {
        // Signatura vstupního souboru není správná/podporovaná
        fprintf(stderr, "ERROR: GIF signature not supported.\n");
        // Tisk podporované/očekávané signatury
        fprintf(stderr, "Expected: %s\n", GIF_SIGNATURE);
        // Tisk špatné/nepodporované signatury vstupního souboru
        fprintf(stderr, "Found:    %s\n", fileSignature);
        // Ukončení funkce s chybou
        return RETURN_FAILURE;
    }
}

/*
 * Funkce pro získání informací z hlavičky vstupního souboru
 */
void getGIFInfo() {
    // Proměnná pro bitové pole hlavičky
    uint8_t headerBitField;
    // Pomocná proměnná pro mocninu velikosti globální tabulky barev
    uint8_t globalColorTablePower;

    // Získání šířky souboru z hlavičky
    info.imageWidth  = getByte();
    info.imageWidth += (getByte() * BYTE_OVERFLOW);

    // Získání výšky souboru z hlavičky
    info.imageHeight  = getByte();
    info.imageHeight += (getByte() * BYTE_OVERFLOW);

    // Získání bitového pole z hlavičky
    headerBitField = getByte();

    // Získání příznaku pro globální tabulku barev
    if((headerBitField & AND_OF_COLOR_TABLE_FLAG) == AND_OF_COLOR_TABLE_FLAG) {
        info.gctFlag = FLAG_TRUE;
    } else {
        info.gctFlag = FLAG_FALSE;
    }

    // Zjištění počtu bitů na pixel
    info.bpp = (headerBitField & AND_OF_BPP);
    info.bpp = info.bpp >> BPP_SHIFT;
    info.bpp++;

    // Získání příznaku setřídění globální tabulky barev
    if((headerBitField & AND_OF_SORT_FLAG) == AND_OF_SORT_FLAG) {
        info.gctSortFlag = FLAG_TRUE;
    } else {
        info.gctSortFlag = FLAG_FALSE;
    }

    // Získání velikosti globální tabulky barev
    globalColorTablePower = (headerBitField & AND_OF_COLOR_TABLE_SIZE);
    globalColorTablePower++;
    info.gctSize = (uint16_t)pow(2, globalColorTablePower);

    // Získání indexu barvy pozadí
    info.bgColorIndex = getByte();

    // Získání poměru výšky a šířky pixelu
    info.paRatio = getByte();
}

/*
 * Funkce pro vytvoření globální tabulky barev
 */
void makeGCT() {
    // Alokace prostoru pro globální tabulku barev
    globalColorTable = (tRGB*)malloc(sizeof(tRGB) * info.gctSize);
    // Kontrola alokace
    if(globalColorTable == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: globalColorTable malloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }

    // Načtení jednotlivých barev globální tabulky
    for(uint16_t index = 0; index < info.gctSize; index++) {
        // Červená složka
        globalColorTable[index].r = getByte();
        // Zelená složka
        globalColorTable[index].g = getByte();
        // Modrá složka
        globalColorTable[index].b = getByte();
    }
}

/*
 * Funkce pro vytvoření lokální tabulky barev
 */
void makeLCT(uint16_t lctSize) {
    // Alokace prostoru pro lokální tabulku barev
    localColorTable = (tRGB*)malloc(sizeof(tRGB) * lctSize);
    // Kontrola alokace
    if(localColorTable == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: localColorTable malloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }

    // Načtení jednotlivých barev lokální tabulky
    for(uint16_t index = 0; index < lctSize; index++) {
        // Červená složka
        localColorTable[index].r = getByte();
        // Zelená složka
        localColorTable[index].g = getByte();
        // Modrá složka
        localColorTable[index].b = getByte();
    }
}

/*
 * Funkce pro alokaci výsledných barev pro zápis do výstupního souboru
 */
void allocBMPData() {
    // Alokace počtu řádků výsledného obrázku
    dataBMP = malloc(info.imageHeight * sizeof(tRGB*));
    // Kontrola alokace
    if(dataBMP == NULL) {
        // Tisk chyby
        fprintf(stderr, "ERROR: dataBMP malloc failed.\n");
        // Konec programu s chybou
        exit(RETURN_FAILURE);
    }
    // Cyklus alokace sloupců (celých řádků) pro výsledné hodnoty barev
    for(uint32_t colIndex = 0; colIndex < info.imageHeight; colIndex++) {
        // Alokace jednoho celého řádku indexů
        dataBMP[colIndex] = (tRGB*)malloc(info.imageWidth * sizeof(tRGB));
        // Kontrola alokace
        if(dataBMP[colIndex] == NULL) {
            // Tisk chyby
            fprintf(stderr, "ERROR: dataBMP[colIndex] malloc failed.\n");
            // Konec programu s chybou
            exit(RETURN_FAILURE);
        }
    }
}

/*
 * Funkce pro výpis výsledných barev na standardní výstup (informační)
 */
void printPixels() {
    // Cyklus procházení řádků tabulky
    for(uint32_t row = 0; row < info.imageHeight; row++) {
        // Cyklus procházení sloupců v řádku tabulky
        for(uint32_t col = 0; col < info.imageWidth; col++) {
            // Výpis hodnot barev na pozici řádek:sloupec

            // Červená složka
            printf("%02x", dataBMP[row][col].r);
            // Zelená složka
            printf("%02x", dataBMP[row][col].g);
            // Modrá složka
            printf("%02x ", dataBMP[row][col].b);
        }
        // Odřádkování každého řádku tabulky
        printf("\n");
    }

    // Výpis celkového počtu indexů v tabulce
    printf("\n%d\n", nextPixelIndex);
}

/*
 * Funkce pro uvolnění paměti po tabulce výsledných barev
 */
void freeBMPData() {
    // Cyklus procházení řádků tabulky
    for(uint32_t row = 0; row < info.imageHeight; row++) {
        // Uvolnění jednoho celého řádku tabulky
        free(dataBMP[row]);
    }
    // Uvolnění místa po řádcích tabulky
    free(dataBMP);
}

/*
 * Funkce pro zápis výsledných dat do BMP výstupního souboru
 */
void writeBMPData(tGIF2BMP *logInfo) {
    // Proměnná pro uložení počtu batjů k zarovnání jednoho řádku výsledných dat
    // na délku násobku 4 bajtů
    uint8_t addition = 0;
    // Proměnná počtu bajtů pro jeden řádek ve výsledném souboru
    uint16_t rowWidth = info.imageWidth * ONE_PIXEL_SIZE;
    // Získání počtu bajtů pro jeden řádek
    // a dorovnání počtu bajtů na násobek 4 bajtů
    while((rowWidth % ROW_MULT_SIZE) > 0) {
        // Inkrementace délky řádku v bajtech
        rowWidth++;
        // Inkrementace počtu bajtů na dorovnání řádku
        addition++;
    }

    // BITMAPFILEHEADER - zápis hlavičky
    // Identifikátor formátu BMP
    char bfType[] = BMP_IDENTIFICATOR;
    // Zápis identifikátoru BMP souboru
    fprintf(outputBMPFile, bfType);
    // Celková velikost souboru s obrazovými údaji
    uint32_t bfSize = BITMAPFILEHEADER_SIZE  // Velikost hlavičky
                    + BITMAPINFOHEADER_SIZE  // Velikost informační hlavičky
                    + (info.imageHeight * info.imageWidth * ONE_PIXEL_SIZE)  // Velikost plochy obrázku
                    + (info.imageHeight * addition);  // Bajty pro doplnění délky řádku na násobek 4
    // Uložení velikosti BMP souboru pro log
    if(logInfo != NULL) logInfo->bmpSize = bfSize;
    // Zápis celkové velikosti souboru
    write4Bytes(bfSize);
    // Tento údaj je rezervovaný pro pozdější použití
    uint16_t bfReserved1 = RESERVED_VALUE;
    // Zápis rezervovaného údaje č. 1
    write2Bytes(bfReserved1);
    // I tento údaj je rezervovaný pro pozdější použití
    uint16_t bfReserved2 = RESERVED_VALUE;
    // Zápis rezervovaného údaje č. 2
    write2Bytes(bfReserved2);
    // Posun struktury BITMAPFILEHEADER od začátku vlastních obrazových dat
    uint32_t bfOffBits = BITMAPFILEHEADER_SIZE  // Velikost hlavičky
                       + BITMAPINFOHEADER_SIZE;  // Velikost informační hlavičky
    // Zápis posunu hlavičky
    write4Bytes(bfOffBits);

    // BITMAPINFOHEADER - zápis informační hlavičky
    // Celková velikost datové struktury BITMAPINFOHEADER
    uint32_t biSize = BITMAPINFOHEADER_SIZE;
    // Zápis velikosti informační hlavičky
    write4Bytes(biSize);
    // Šířka obrazu v pixelech
    uint32_t biWidth = info.imageWidth;
    // Zápis šířky obrazu
    write4Bytes(biWidth);
    // Výška obrazu v pixelech
    uint32_t biHeight = info.imageHeight;
    // Zápis výšky obrazu
    write4Bytes(biHeight);
    // Počet bitových rovin pro výstupní zařízení
    uint16_t biPlanes = BI_PLANES_VALUE;
    // Zápis bitových rovin
    write2Bytes(biPlanes);
    // Celkový počet bitů na pixel
    uint16_t biBitCount = BIT_COUNT;
    // Zápis počtu bitů na pixel
    write2Bytes(biBitCount);
    // Typ komprimační metody obrazových dat
    uint32_t biCompression = COMPRESSION_METHOD;
    // Zápis komprimační metody
    write4Bytes(biCompression);
    // Velikost obrazu v bajtech = nula (dopočítá se z předchozích položek)
    uint32_t biSizeImage = 0x0;
    // Zápis velikosti obrazu v bajtech
    write4Bytes(biSizeImage);
    // Horizontální rozlišení výstupního zařízení v pixelech na metr - neznámé
    uint32_t biXPelsPerMeter = 0x0;
    // Zápis horizontálního rozlišení
    write4Bytes(biXPelsPerMeter);
    // Vertikální rozlišení výstupního zařízení v pixelech na metr - neznámé
    uint32_t biYPelsPerMeter = 0x0;
    // Zápis vertikálního rozlišení
    write4Bytes(biYPelsPerMeter);
    // Celkový počet barev, které jsou použité v dané bitmapě - neurčeno
    uint32_t biClrUsed = 0x0;
    // Zápis celkového počtu použitých barev
    write4Bytes(biClrUsed);
    // Počet barev, které jsou důležité pro vykreslení bitmapy - neurčeno
    uint32_t biClrImportant = 0x0;
    // Zápis počtu důležitých barev
    write4Bytes(biClrImportant);

    // BITS - zápis barev pixelů
    // Cyklus procházení řádků obrázku - zdola nahoru
    for(uint32_t row = info.imageHeight; row > 0; row--) {
        // Cyklus procházení sloupců jednoho řádku obrázku
        for(uint32_t col = 0; col < info.imageWidth; col++) {
            // Zápis jedné barvy do výstupního souboru

            // Modrá složka
            fprintf(outputBMPFile, "%c", dataBMP[row-1][col].b);
            // Zelená složka
            fprintf(outputBMPFile, "%c", dataBMP[row-1][col].g);
            // Zelená složka
            fprintf(outputBMPFile, "%c", dataBMP[row-1][col].r);
        }
        // Cyklus doplnění počtu bajtů řádku na násobek 4
        for(uint8_t plus = 0; plus < addition; plus++) {
            // Zápis nulového bajtu do souboru
            fprintf(outputBMPFile, "%c", 0x0);
        }
    }
}

/*
 * Funkce pro zpracování dat v image bloku
 */
void processImageBlockData() {
    // Proměnná pro velikost bloku s daty
    // a její získání
    uint16_t blockSize = getByte();
    // Tisk velikosti bloku s daty
    // fprintf(stderr, "INFO: Block size: %d\n", blockSize);

    // Pokud blok obsahuje nějaká data
    if(blockSize > 0) {
        // Naalokuji místo pro data v tomto bloku
        data = (char*)malloc(blockSize * sizeof(char));
        // Kontrola alokace
        if(data == NULL) {
            // Tisk chyby
            fprintf(stderr, "ERROR: data malloc failed.\n");
            // Konec programu s chybou
            exit(RETURN_FAILURE);
        }
        // Vynulování ukazatelů pro práci s daty bloku
        dataIndex = 0;
        actualIndex = 0;
        nextPixelIndex = 0;
    }

    // Dokud nenarazím na ukončující bajt,
    // načítám data
    while(blockSize > 0) {
        // Do proměnné *data ukládám všechny bajty v aktuálním bloku dat
        for(uint16_t blockIndex = 0; blockIndex < blockSize; blockIndex++) {
            // Uložení jednoho bajtu do proměnné *data
            data[dataIndex] = getByte();
            // Zvýšení indexu na první volný bajt v proměnné *data
            dataIndex++;
        }

        // Zjisti novou velikost bloku,
        // popř. načti ukončující bajt bloku
        blockSize = getByte();

        // Pokud se nejedná o ukončující bajt bloku
        if(blockSize != BLOCK_TERMINATOR) {
            // Tiskni o tom informaci
            // a vypiš velikost dalšího bloku s daty
            // fprintf(stderr, "INFO: No terminator, block size: %d\n", blockSize);
            // Realokuj místo pro data v novém bloku
            data = (char*)realloc(data, (dataIndex + blockSize));
            // Kontrola realokace
            if(data == NULL) {
                // Tisk chyby
                fprintf(stderr, "ERROR: data realloc failed.\n");
                // Konec programu s chybou
                exit(RETURN_FAILURE);
            }
        } else {
            // Jinak tiskni informaci o konci bloku
            // fprintf(stderr, "INFO: Image block terminator, data size: %d\n", dataIndex);
        }
    }

    // Proměnná pro aktuální index do tabulky barev
    uint32_t actualColorIndex = 0x0;
    // Proměnná pro předchozí index do tabulky barev
    uint32_t previousColorIndex = 0x0;
    // Hodnota clear code (CC)
    uint16_t clearCode = pow(2, LZWMininumCodeSize);
    // Hodnota end of input (EOI)
    uint16_t endOfInput = (clearCode + 1);
    // Hodnota pro vyrovnání indexu tabulky nových indexů barev
    eoiIndex = (endOfInput + 1);
    // Aktuální hodnota velikosti LZW kódu
    uint8_t actualLZWCodeSize = (LZWMininumCodeSize + 1);
    // Aktuální bitová maska pro získání bitu
    uint16_t actualBitMask = 0x1;
    // Proměnná pro získání jednoho bitu
    uint16_t oneBit = 0x0;
    // Aktuální index do tabulky nových kódů
    uint32_t actualTableIndex = 0;
    // Proměnná pro maximální počet barev aktuální velikost LZW kódu
    uint32_t maximalTableIndex = (pow(2, (actualLZWCodeSize - 1)) - 1);
    // Tabulka pro nové indexy tabulky indexů barev (258 - ...)
    tTable colorTable;
    // Ukazatel pro položku aktuálního načteného indexu ze vstupního souboru
    tItem *actual = NULL;
    // Ukazatel pro položku předchozího načteného indexu ze vstupního souboru
    tItem *previous = NULL;
    // Příznak prvního bajtu po clear kódu
    uint8_t isFirst = YES;

    // Inicializace tabulky pro nové indexy barev
    initTable(&colorTable);
    // Nastavím ukazatel právě zpracovávaného bajtu na první bajt dat
    actualIndex = 0;

    // Dokud nejsem na konci bloku dat,
    // zpracovávám další index do tabulky barev
    do {
        // Uložení aktuálního indexu barvy jako předchozího
        previousColorIndex = actualColorIndex;
        // Nastavení aktuálního indexu barvy na 0
        actualColorIndex = 0x0;

        // Cyklus pro získání jedné nové hodnoty indexu do tabulky barev
        for(uint8_t bitIndex = 0; bitIndex < actualLZWCodeSize; bitIndex++) {
            // Pokud je aktuálně získávaný bit 1
            if((data[actualIndex] & actualBitMask) > 0) {
                // Nastavím nový bit na 1
                oneBit = 0x1;
            } else {
                // Jinak nastavím nový bit na 0
                oneBit = 0x0;
            }

            // Posun bitu v bitové masce pro další bit o 1 pozici vlevo
            actualBitMask = actualBitMask << 1;
            // Posun nového bitu o i pozic vlevo
            oneBit = oneBit << bitIndex;

            // Přidání nově získaného bitu do výsledného indexu přichozí barvy
            actualColorIndex = actualColorIndex | oneBit;

            // Pokud jsem na konci zpracovávaného bajtu
            if(actualBitMask == 0x100) {
                // Nastavení masky na první bit (LSB)
                actualBitMask = 0x1;
                // Posun na další bajt načtených dat vstupního souboru
                actualIndex++;
            }
        }

        // Pokud je aktuální získaný index EOI
        if(actualColorIndex == endOfInput) {
            // Konec cyklu zpracovávání vstupu
            break;
        }

        // Posun na další položku v tabulce nových indexů barev
        actualTableIndex++;

        // Pokud je aktuální získaný index clear kódem
        if(actualColorIndex == clearCode) {
            // Resetování aktuální hodnoty LZW kódu
            actualLZWCodeSize = (LZWMininumCodeSize + 1);
            // Resetování maximální velikosti tabulky pro aktuální LZW kód
            maximalTableIndex = (pow(2, LZWMininumCodeSize) - 1);
            // Resetování indexu do tabulky nových indexů
            actualTableIndex = 0;
            // Uvolnění místa po předchozí tabulce indexů
            freeTable(&colorTable);
            // Reinicializace tabulky nových indexů barev
            initTable(&colorTable);
            // Nastavení příznaku nového bloku po clear kódu
            isFirst = YES;
            // Pokračuje se dalším krokem cyklu
            continue;
        } else if(actualTableIndex == maximalTableIndex) {
            // Pokud bylo dosaženo maximálního indexu pro aktuální LZW

            // Zvýšení velikosti LZW kódu pokud to ještě lze (<12)
            if(actualLZWCodeSize < 12) {
                // Inkrementace velikosti LZW
                actualLZWCodeSize++;
            }
            // Zvýšení maximálního indexu pro nový LZW kód
            maximalTableIndex = pow(2, (actualLZWCodeSize-1));
            // Reset indexu pro aktuální LZW
            actualTableIndex = 0;
        }

        // Pokud se jedná o nový blok po clear kódu
        if(isFirst == YES) {
            // Nastavení příznaku na zpracovaný první index nového bloku
            isFirst = NO;

            // Pokud aktuálně zpracovávaný pixel má být neprůhledný
            if(imageBlockNumber == 1 || blockTrasparentColorFlag != FLAG_TRUE || actualColorIndex != transparentColorIndex) {
                // Získání čísla řádku
                uint32_t rowIndex = getRowIndex();
                // Získání čísla sloupce
                uint32_t colIndex = getColIndex();
                // Uložení barvy aktuálního pixelu
                dataBMP[rowIndex][colIndex].r = actualColorTable[actualColorIndex].r;
                dataBMP[rowIndex][colIndex].g = actualColorTable[actualColorIndex].g;
                dataBMP[rowIndex][colIndex].b = actualColorTable[actualColorIndex].b;
            }
            // Posun na další index výstupní tabulky
            nextPixelIndex++;
        } else {
            // Pokud již byl zpracován první index nového bloku

            // Nová položka do tabulky indexů
            tItem newTableItem = {0, 0, NULL};
            // Inicializace nové položky
            initTableItem(&newTableItem);

            // Pokud je aktuální index barvy v rozsahu již existujících indexů
            if(actualColorIndex < (colorTable.used + eoiIndex)) {
                // Data z aktuálního indexu jdou na výstup

                // Pokud je index v rozsahu aktuální tabulky barev
                if(actualColorIndex < actualColorTableSize) {
                    // Pokud aktuálně zpracovávaný pixel má být neprůhledný
                    if(imageBlockNumber == 1 || blockTrasparentColorFlag != FLAG_TRUE || actualColorIndex != transparentColorIndex) {
                        // Získání čísla řádku
                        uint32_t rowIndex = getRowIndex();
                        // Získání čísla sloupce
                        uint32_t colIndex = getColIndex();
                        // Na výstupu bude aktuální index barvy
                        dataBMP[rowIndex][colIndex].r = actualColorTable[actualColorIndex].r;
                        dataBMP[rowIndex][colIndex].g = actualColorTable[actualColorIndex].g;
                        dataBMP[rowIndex][colIndex].b = actualColorTable[actualColorIndex].b;
                    }
                    // Posun na další index výstupní tabulky
                    nextPixelIndex++;
                } else {
                    // Pokud je index v rozsahu nových indexů barev

                    // Uložení položky z tabulky indexů podle aktuálního indexu
                    actual = getItemByIndex(&colorTable, actualColorIndex);
                    // Ukládání všech indexů z aktuální položky do výstupní tabulky
                    for(uint32_t idx = 0; idx < actual->used; idx++) {
                        // Pokud aktuálně zpracovávaný pixel má být neprůhledný
                        if(imageBlockNumber == 1 || blockTrasparentColorFlag != FLAG_TRUE || actual->indexList[idx] != transparentColorIndex) {
                            // Získání čísla řádku
                            uint32_t rowIndex = getRowIndex();
                            // Získání čísla sloupce
                            uint32_t colIndex = getColIndex();
                            // Uložení barvy aktuálního pixelu
                            dataBMP[rowIndex][colIndex].r = actualColorTable[actual->indexList[idx]].r;
                            dataBMP[rowIndex][colIndex].g = actualColorTable[actual->indexList[idx]].g;
                            dataBMP[rowIndex][colIndex].b = actualColorTable[actual->indexList[idx]].b;
                        }
                        // Posun na další index výstupní tabulky
                        nextPixelIndex++;
                    }
                }

                // Přidání další položky do tabulky barev
                //    - přidání dat z předchozího indexu

                // Pokud je index v rozsahu globální tabulky barev
                if(previousColorIndex < actualColorTableSize) {
                    // Uložení indexu do nové položky
                    addByteToTableItem(&newTableItem, previousColorIndex);
                } else {
                    // Pokud je index v rozsahu nových indexů

                    // Uložení položky předchozího indexu
                    previous = getItemByIndex(&colorTable, previousColorIndex);
                    // Ukládání indexů z předchozího indexu do nové položky
                    for(uint32_t idx = 0; idx < previous->used; idx++) {
                        // Přidání jednoho indexu do nové položky
                        addByteToTableItem(&newTableItem, previous->indexList[idx]);
                    }
                }
                //    - přidání prvního bajtu z aktuálního indexu

                // Pokud je index v rozsahu globální tabulky barev
                if(actualColorIndex < actualColorTableSize) {
                    // Uložení indexu do nové položky
                    addByteToTableItem(&newTableItem, actualColorIndex);
                } else {
                    // Pokud je index v rozsahu nových indexů

                    // Uložení prvního indexu aktuální položky
                    addByteToTableItem(&newTableItem, actual->indexList[0]);
                }
                // Přidání nové vytvořené položky do tabulky
                insertNewItem(&colorTable, &newTableItem);
            } else {
                // Pokud aktuální index barvy ještě není v tabulce

                // Přidání další položky do tabulky barev
                //    - přidání dat z předchozího indexu

                // Pokud je index v rozsahu globální tabulky barev
                if(previousColorIndex < actualColorTableSize) {
                    // Uložení indexu do nové položky
                    addByteToTableItem(&newTableItem, previousColorIndex);
                } else {
                    // Pokud je index v rozsahu nových indexů

                    // Uložení položky předchozího indexu
                    previous = getItemByIndex(&colorTable, previousColorIndex);
                    // Ukládání indexů z předchozího indexu do nové položky
                    for(uint32_t idx = 0; idx < previous->used; idx++) {
                        // Přidání jednoho indexu do nové položky
                        addByteToTableItem(&newTableItem, previous->indexList[idx]);
                    }
                }
                //    - přidání prvního bajtu z předchozího indexu

                // Pokud je index v rozsahu globální tabulky barev
                if(previousColorIndex < actualColorTableSize) {
                    // Uložení indexu do nové položky
                    addByteToTableItem(&newTableItem, previousColorIndex);
                } else {
                    // Uložení prvního indexu předchozí položky
                    addByteToTableItem(&newTableItem, previous->indexList[0]);
                }
                // Přidání nové vytvořené položky do tabulky
                insertNewItem(&colorTable, &newTableItem);
                // Data z aktuálního indexu jdou na výstup

                // Uložení položky aktuálního indexu
                actual = getItemByIndex(&colorTable, actualColorIndex);
                // Ukládání indexů z aktuálního indexu do výstupní tabulky
                for(uint32_t idx = 0; idx < actual->used; idx++) {
                    // Pokud aktuálně zpracovávaný pixel má být neprůhledný
                    if(imageBlockNumber == 1 || blockTrasparentColorFlag != FLAG_TRUE || actual->indexList[idx] != transparentColorIndex) {
                        // Získání čísla řádku
                        uint32_t rowIndex = getRowIndex();
                        // Získání čísla sloupce
                        uint32_t colIndex = getColIndex();
                        // Uložení barvy aktuálního pixelu
                        dataBMP[rowIndex][colIndex].r = actualColorTable[actual->indexList[idx]].r;
                        dataBMP[rowIndex][colIndex].g = actualColorTable[actual->indexList[idx]].g;
                        dataBMP[rowIndex][colIndex].b = actualColorTable[actual->indexList[idx]].b;
                    }
                    // Posun na další index výstupní tabulky
                    nextPixelIndex++;
                }
            }
        }
    // Cyklus procházení načtených dat,
    // případné ukončení po dosažení konce dat - prevence zacyklení
    } while(actualIndex < (dataIndex + 2));

    // Pokud blok obsahoval obrazová data
    if(data != NULL) {
        // Uvolnění místa po datech
        free(data);
    }

    // Uvolnění paměti po tabulce nových indexů barev
    freeTable(&colorTable);
}

/*
 * Funkce pro zpracování bloku s obrazovými daty
 */
void processImageBlock() {
    // Tisk informace o zpracovávání bloku s obrazovými daty
    // fprintf(stderr, "INFO: Image block\n");
    // Inkrementace pořadí image bloku
    imageBlockNumber++;

    // Proměnná pro pozici levého okraje bloku
    // a její výpočet
    uint16_t blockLeftPosition = getByte();
    blockLeftPosition += (getByte() * BYTE_OVERFLOW);
    // Tisk pozice levého okraje bloku
    // fprintf(stderr, "INFO: Block left position: %d\n", blockLeftPosition);
    // Uložení levé pozice aktuálního bloku
    actualLeft = blockLeftPosition;

    // Proměnná pro pozici horního okraje bloku
    // a její výpočet
    uint16_t blockTopPosition = getByte();
    blockTopPosition += (getByte() * BYTE_OVERFLOW);
    // Tisk pozice horního okraje bloku
    // fprintf(stderr, "INFO: Block top position: %d\n", blockTopPosition);
    // Uložení horní pozice aktuálního bloku
    actualTop = blockTopPosition;

    // Proměnná pro šířku bloku
    // a její výpočet
    uint16_t blockWidth = getByte();
    blockWidth += (getByte() * BYTE_OVERFLOW);
    // Tisk šířky bloku
    // fprintf(stderr, "INFO: Block width: %d\n", blockWidth);
    // Uložení šířky aktuálního bloku
    actualWidth = blockWidth;

    // Proměnná pro výšku bloku
    // a její výpočet
    uint16_t blockHeight = getByte();
    blockHeight += (getByte() * BYTE_OVERFLOW);
    // Tisk výšky bloku
    // fprintf(stderr, "INFO: Block height: %d\n", blockHeight);
    // Uložení výšky aktuálního bloku
    actualHeight = blockHeight;

    // Proměnná pro bitové pole bloku
    // a jeho získání
    uint8_t blockBitField = getByte();

    // Proměnná pro příznak lokální tabulky barev
    uint8_t localColorTableFlag = FLAG_FALSE;
    // Vynulování příznaku prokládání
    blockInterlaceFlag = FLAG_FALSE;
    // Proměnná pro příznak setřídění tabulky barev
    uint8_t localColorTableSortFlag = FLAG_FALSE;
    // Proměnná pro velikost lokální tabulky barev
    uint16_t localColorTableSize = 0;

    // Získání příznaku lokální tabulky barev
    if((blockBitField & AND_OF_COLOR_TABLE_FLAG) == AND_OF_COLOR_TABLE_FLAG) {
        localColorTableFlag = FLAG_TRUE;
    }
    // Tisk příznaku lokální tabulky barev
    // fprintf(stderr, "INFO: Local color table flag: %d\n", localColorTableFlag);

    // Získání příznaku prokládání
    if((blockBitField & AND_OF_INTERLACE_FLAG) == AND_OF_INTERLACE_FLAG) {
        blockInterlaceFlag = FLAG_TRUE;

        // Vynulování aktuálního proloženého řádku
        actualInterlaceRow = 0;
        // Vynulování aktuálního stavu prokládání
        interlaceState = STATE0;
    }
    // Tisk příznaku prokládání
    // fprintf(stderr, "INFO: Block interlace flag: %d\n", blockInterlaceFlag);

    // Získání příznaku setřídění
    if((blockBitField & AND_OF_IMAGE_BLOCK_SORT) == AND_OF_IMAGE_BLOCK_SORT) {
        localColorTableSortFlag = FLAG_TRUE;
    }
    // Tisk příznaku setřídění
    // fprintf(stderr, "INFO: Local color table sort flag: %d\n", localColorTableSortFlag);

    // Pokud je v bloku lokální tabulka barev
    if(localColorTableFlag == FLAG_TRUE) {
        // Proměnná pro mocninu velikosti lokální tabulky barev
        // a její výpočet
        uint8_t localColorTablePower = (blockBitField & AND_OF_COLOR_TABLE_SIZE);
        localColorTablePower++;

        // Výpočet velikosti lokální tabulky barev
        localColorTableSize = (uint16_t)pow(2, localColorTablePower);

        // Tisk velikosti lokální tabulky barev
        // fprintf(stderr, "INFO: Local color table size: %d\n", localColorTableSize);

        // Vytvoření lokální tabulky barev
        makeLCT(localColorTableSize);

        // Tisk nadpisu lokální tabulky barev
        // fprintf(stderr, "INFO: Local color table:\n");

        // Ukazatel na barvu v lokální tabulce
        // tRGB *localColor = NULL;
        // Průchod lokální tabulkou barev
        // for(uint16_t lctIndex = 0; lctIndex < localColorTableSize; lctIndex++) {
            // Uložení aktuálně vypisované barvy
            // localColor = &localColorTable[lctIndex];
            // Tisk aktuálně načtené barvy v lokální tabulce barev
            // fprintf(stderr, "INFO: %3d: %02x%02x%02x\n", lctIndex, localColor->r, localColor->g, localColor->b);
        // }

        // Zpracování bloku bude pracovat s lokální tabulkou barev

        // Nastavení používané tabulky na lokální
        actualColorTable = localColorTable;
        // Nastavení velikosti tabulky na velikost lokální
        actualColorTableSize = localColorTableSize;
    } else {
        // Zpracování bloku bude pracovat s globální tabulkou barev

        // Nastavení používané tabulky na blobální
        actualColorTable = globalColorTable;
        // Nastavení velikosti tabulky na velikost globální
        actualColorTableSize = info.gctSize;
    }

    // Získání minimální velikosti LZW kódu
    LZWMininumCodeSize = getByte();
    // Tisk minimální velikosti LZW kódu
    // fprintf(stderr, "INFO: LZW minimum code size: %d\n", LZWMininumCodeSize);

    // Zpracování dat v image bloku
    processImageBlockData();

    // Pokud byla využita lokální tabulka barev
    if(localColorTableFlag == FLAG_TRUE && localColorTable != NULL) {
        // Uvolnění paměti po lokální tabulce barev
        free(localColorTable);
    }
}

/*
 * Funkce pro zpracování bloku rozšíření graphic control
 */
void processGraphicControlBlock() {
    // Tisk informace o zpracovávání graphic control bloku
    // fprintf(stderr, "INFO: Graphic control\n");

    // Proměnná pro velikost bloku
    // a její získání
    uint8_t blockSize = getByte();
    // Tisk velikosti bloku
    // fprintf(stderr, "INFO: Block size: %d\n", blockSize);

    // Proměnná pro bitové pole bloku
    // a jeho získání
    uint8_t blockBitField = getByte();

    // Proměnná pro disposal method bloku
    // a její získání
    uint8_t blockDisposalMethod = (blockBitField & AND_OF_DISPOSAL_METHOD);
    blockDisposalMethod = blockDisposalMethod >> DISPOSAL_METHOD_SHIFT;
    // Tisk disposal method
    // fprintf(stderr, "INFO: Block disposal method: %d\n", blockDisposalMethod);

    // Proměnná pro příznak user input
    uint8_t blockUserInputFlag = FLAG_FALSE;
    // Získání user input flagu
    if((blockBitField & AND_OF_USER_INPUT_FLAG) == AND_OF_USER_INPUT_FLAG) {
        blockUserInputFlag = FLAG_TRUE;
    }
    // Tisk příznaku user input
    // fprintf(stderr, "INFO: Block user input flag: %d\n", blockUserInputFlag);

    // Získání příznaku průhlednosti
    blockTrasparentColorFlag = (blockBitField & AND_OF_TRANSPARENT_FLAG);
    // Tisk příznaku průhlednosti
    // fprintf(stderr, "INFO: Block transparent flag: %d\n", blockTrasparentColorFlag);

    // Proměnná pro dobu zpoždění
    // a její získání
    uint16_t blockDelayTime = getByte();
    blockDelayTime += (getByte() * BYTE_OVERFLOW);
    // Výpis doby zpoždění
    // fprintf(stderr, "INFO: Block delay time: %d (1/100s)\n", blockDelayTime);

    // Získání indexu průhledné barvy
    transparentColorIndex = getByte();

    // Pokud je nastaven příznak průhlednosti
    if(blockTrasparentColorFlag == FLAG_TRUE) {
        // Tisk indexu průhledné barvy
        // fprintf(stderr, "INFO: Transparent color index: %d\n", transparentColorIndex);
    }

    // Získání ukončujícího bajtu bloku
    blockSize = getByte();

    // Pokud se nejedná o ukončující bajt
    if(blockSize != BLOCK_TERMINATOR) {
        // Tisk chyby
        // fprintf(stderr, "ERROR: No terminator\n");
    } else {
        // Jinak tiskni informaci o konci bloku
        // fprintf(stderr, "INFO: Graphic control block terminator\n");
    }
}

/*
 * Funkce pro zpracování bloku rozšíření komentáře
 */
void processCommentBlock() {
    // Tisk informace o zpracovávání bloku komentáře
    // fprintf(stderr, "INFO: Comment\n");

    // Proměnná pro velikost bloku komentáře
    // a její získání
    uint8_t blockSize = getByte();

    // Dokud pokračuje blok s daty
    while(blockSize > 0) {
        // Projdi všechny datové bajty bloku
        for(uint8_t blockIndex = 0; blockIndex < blockSize; blockIndex++) {
            // Načti jeden bajt dat
            getByte();
        }

        // Zjisti novou velikost bloku,
        // popř. načti ukončující bajt bloku
        blockSize = getByte();

        // Pokud se nejedná o ukončující bajt bloku
        if(blockSize != BLOCK_TERMINATOR) {
            // Tiskni o tom informaci
            // a vypiš velikost dalšího bloku s daty
            // fprintf(stderr, "INFO: No terminator, block size: %d\n", blockSize);
        } else {
            // Jinak tiskni informaci o konci bloku
            // fprintf(stderr, "INFO: Comment block terminator\n");
        }
    }
}

/*
 * Funkce pro zpracování bloku rozšíření prostého textu
 */
void processPlainTextBlock() {
    // Tisk informace o zpracovávání bloku prostého textu
    // fprintf(stderr, "INFO: Plain text\n");

    // Proměnná pro velikost bloku
    // a její získání
    uint8_t blockSize = getByte();
    // Tisk velikosti bloku
    // fprintf(stderr, "INFO: Block size: %d\n", blockSize);

    // Proměnná pro pozici levého okraje mřížky textu
    // a její získání
    uint16_t textGridLeftPosition = getByte();
    textGridLeftPosition += (getByte() * BYTE_OVERFLOW);
    // Tisk pozice levého okraje mřížky textu
    // fprintf(stderr, "INFO: Text grid left position: %d\n", textGridLeftPosition);

    // Proměnná pro pozici horního okraje mřížky textu
    // a její získání
    uint16_t textGridTopPosition = getByte();
    textGridTopPosition += (getByte() * BYTE_OVERFLOW);
    // Tisk pozice horního okraje mřížky textu
    // fprintf(stderr, "INFO: Text grid top position: %d\n", textGridTopPosition);

    // Proměnná pro šířku mřížky textu
    // a její získání
    uint16_t textGridWidth = getByte();
    textGridWidth += (getByte() * BYTE_OVERFLOW);
    // Tisk šířky mřížky textu
    // fprintf(stderr "INFO: Text grid width: %d\n", textGridWidth);

    // Proměnná pro výšky mřížky textu
    // a její získání
    uint16_t textGridHeight = getByte();
    textGridHeight += (getByte() * BYTE_OVERFLOW);
    // Tisk výšky mřížky textu
    // fprintf(stderr, "INFO: Text grid height: %d\n", textGridHeight);

    // Proměnná pro šířku buňky textu
    // a její získání
    uint8_t characterCellWidth = getByte();
    // Tisk šířky buňky textu
    // fprintf(stderr, "INFO: Character cell width: %d\n", characterCellWidth);

    // Proměnná pro výšku buňky textu
    // a její získání
    uint8_t characterCellHeight = getByte();
    // Tisk výšky buňky textu
    // fprintf(stderr, "INFO: Character cell height: %d\n", characterCellHeight);

    // Proměnná pro index barvy popředí textu
    // a jeho získání
    uint8_t textForegroundColorIndex = getByte();
    // Tisk indexu barvy popředí textu
    // fprintf(stderr, "INFO: Text foreground color index: %d\n", textForegroundColorIndex);

    // Proměnná pro index barvy pozadí textu
    // a jeho získání
    uint8_t textBackgroundColorIndex = getByte();
    // Tisk indexu barvy pozadí textu
    // fprintf(stderr, "INFO: Text background color index: %d\n", textBackgroundColorIndex);

    // Získání velikosti bloku s daty textu
    blockSize = getByte();
    // Tisk velikosti bloku s daty textu
    // fprintf(stderr, "INFO: Plain text data size: %d\n", blockSize);
    // Tisk nadpisu pro uložený text
    // fprintf(stderr, "INFO: Text: \n");

    // Dokud pokračuje blok s daty
    while(blockSize > 0) {
        // Projdi všechny bajty bloku
        for(uint8_t blockIndex = 0; blockIndex < blockSize; blockIndex++) {
            // Načti a vytiskni znak v bajtu
            // fprintf(stderr, "%c", getByte());
            getByte();
        }

        // Zjisti novou velikost bloku,
        // popř. načti ukončující bajt bloku
        blockSize = getByte();

        // Pokud se nejedná o ukončující bajt bloku
        if(blockSize != BLOCK_TERMINATOR) {
            // Tiskni o tom informaci
            // a vypiš velikost dalšího bloku s daty
            // fprintf(stderr, "INFO: No terminator, block size: %d\n", blockSize);
        } else {
            // Jinak zalom text
            // fprintf(stderr, "\n");
            // a tiskni informaci o konci bloku
            // fprintf(stderr, "INFO: Plain text block terminator\n");
        }
    }
}

/*
 * Funkce pro zpracování bloku rozšíření aplikace
 */
void processApplicationBlock() {
    // Tisk informace o zpracovávání bloku aplikace
    // fprintf(stderr, "INFO: Application\n");

    // Proměnná pro velikost bloku
    // a její získání
    uint8_t blockSize = getByte();
    // Tisk velikosti bloku
    // fprintf(stderr, "INFO: Block size: %d\n", blockSize);

    // Tisk návěští identifikátoru aplikace
    // fprintf(stderr, "INFO: Application identifier: ");
    // Procházení identifikátoru aplikace
    for(uint8_t idIndex = 0; idIndex < APPLICATION_IDENTIFIER_LENGTH; idIndex++) {
        // Tisk jednoho znaku identifikátoru
        // fprintf(stderr, "%c", getByte());
        getByte();
    }
    // Zalomení identifikátoru aplikace
    // fprintf(stderr, "\n");

    // Tisk authentication kódu aplikace
    // fprintf(stderr, "INFO: Application authentication Code: ");
    // Procházení kódu aplikace
    for(uint8_t codeIndex = 0; codeIndex < APPLICATION_CODE_LENGTH; codeIndex++) {
        // fprintf(stderr, "%c", getByte());
        getByte();
    }
    // Zalomení kódu aplikace
    // fprintf(stderr, "\n");

    // Získání velikosti bloku s daty aplikace
    blockSize = getByte();

    // Dokud pokračuje blok s daty
    while(blockSize > 0) {
        // Projdi všechny bajty dat aplikace
        for(uint8_t blockIndex = 0; blockIndex < blockSize; blockIndex++) {
            // Načti jeden bajt dat
            getByte();
        }

        // Zjisti novou velikost bloku,
        // popř. načti ukončující bajt bloku
        blockSize = getByte();

        // Pokud se nejedná o ukončující bajt bloku
        if(blockSize != BLOCK_TERMINATOR) {
            // Tiskni o tom informaci
            // a vypiš velikost dalšího bloku s daty
            // fprintf(stderr, "INFO: No terminator, block size: %d\n", blockSize);
        } else {
            // Jinak tiskni informaci o konci bloku
            // fprintf(stderr, "INFO: Application block terminator\n");
        }
    }
}

/*
 * Funkce pro zpracování části souboru s bloky
 */
void processBlocks() {
    // Proměnná pro označení typu bloku
    uint8_t blockLabel;
    // Proměnná pro zavaděč/oddělovač bloku
    // a jeho získání
    uint8_t blockSeparator = getByte();

    // Dokud nejsem na konci souboru
    while(blockSeparator != TRAILER) {
        // Pokud se jedná o blok s obrazovými daty
        if(blockSeparator == IMAGE_BLOCK_ID) {
            // Zpracuji obrazová data
            processImageBlock(inputGIFFile);
        } else if (blockSeparator == EXTENSION_BLOCK_ID) {
            // Pokud se jedná o blok s rozšířením

            // Tisk návěští bloku rozšíření
            // fprintf(stderr, "INFO: Extension block: ");

            // Získání označení rozšíření
            blockLabel = getByte();

            // Podle označení zpracuj blok rozšíření
            switch(blockLabel) {
                // Blok s graphic control
                case GRAPHIC_CONTROL_BLOCK_ID: {
                    // Zpracuj graphic control blok
                    processGraphicControlBlock(inputGIFFile);
                    break;
                }
                // Blok s komentářem
                case COMMENT_BLOCK_ID: {
                    // Zpracuj blok komentáře
                    processCommentBlock(inputGIFFile);
                    break;
                }
                // Blok prostého textu
                case PLAIN_TEXT_BLOCK_ID: {
                    // Zpracuj blok prostého textu
                    processPlainTextBlock(inputGIFFile);
                    break;
                }
                // Blok aplikace
                case APPLICATION_BLOCK_ID: {
                    // Zpracuj blok aplikace
                    processApplicationBlock(inputGIFFile);
                    break;
                }
                // Neočekávané označení bloku rozšíření
                default: {
                    // Tisk informace o neočekávaném označení bloku rozšiření
                    fprintf(stderr, "ERROR: Unknown extension label: %x.\n", blockLabel);
                    // Ukončení funkce
                    return;
                }
            }
        } else {
            // Pokud se nejedná o blok obrazových dat nebo rozšíření,
            // tiskni o tom informaci
            fprintf(stderr, "ERROR: Unknown block separator/introducer: %x.\n", blockSeparator);
            // a ukonči zpracovávání souboru
            break;
        }

        // Načti další zavaděč/oddělovač bloku
        blockSeparator = getByte();
    }
}

/*
 * Funkce pro převod GIF na BMP
 *
 * gif2bmp    - záznam o převodu
 * inputFile  - vstupní soubor (GIF)
 * outputFile - výstupní soubor (BMP)
 *
 * Návratová hodnota:
 *      0 - převod proběhl v pořádku
 *     -1 - při převodu došlo k chybě,
 *          příp. nepodporuje daný formát GIF
 */
int gif2bmp(tGIF2BMP *gif2bmp, FILE *inputFile, FILE *outputFile) {
    // Testovací tisk pro správné připojení knihovny
    // fprintf(stderr, "INFO: gif2bmp library linked\n");

    // Nastavení globálního vstupního souboru
    inputGIFFile = inputFile;
    // Nastavení globálního výstupního souboru
    outputBMPFile = outputFile;

    // Kontrola signatury vstupního souboru
    if(checkGIFSignature() != RETURN_SUCCESS) {
        return RETURN_FAILURE;
    }

    // Získání informací z hlavičky vstupního souboru
    getGIFInfo();

    // Tisk informací z hlavičky vstupního souboru
    // fprintf(stderr, "INFO: Image width: %d\n", info.imageWidth);
    // fprintf(stderr, "INFO: Image height: %d\n", info.imageHeight);
    // fprintf(stderr, "INFO: Global color table flag: %d\n", info.gctFlag);
    // fprintf(stderr, "INFO: Bits per pixel: %d\n", info.bpp);
    // fprintf(stderr, "INFO: Global color table sort flag: %d\n", info.gctSortFlag);
    // fprintf(stderr, "INFO: Global color table size: %d\n", info.gctSize);
    // fprintf(stderr, "INFO: Background color index: %d\n", info.bgColorIndex);
    // fprintf(stderr, "INFO: Pixel aspect ratio: %d\n", info.paRatio);

    // Pokud je v souboru globální tabulka barev
    if(info.gctFlag == FLAG_TRUE) {
        // Vytvoření globální tabulky barev
        makeGCT();

        // Tisk nadpisu globální tabulky barev
        // fprintf(stderr, "INFO: Global color table:\n");

        // Výpis barev v globální tabulce
        for(uint16_t gctIndex = 0; gctIndex < info.gctSize; gctIndex++) {
            // Tisk jedné barvy v tabulce
            // fprintf(stderr, "INFO: %3d: %02x%02x%02x\n", gctIndex, globalColorTable[gctIndex].r, globalColorTable[gctIndex].g, globalColorTable[gctIndex].b);
        }
    }

    // Alokace tabulky výsledných barev výstupního souboru
    allocBMPData();

    // Zpracování bloků souboru
    processBlocks();

    // Zápis získaných dat do výstupního souboru
    writeBMPData(gif2bmp);

    // Uložení velikosti GIF souboru pro log
    if(gif2bmp != NULL) gif2bmp->gifSize = gifSize;

    // Tisk velikost BMP souboru
    // fprintf(stderr, "INFO: %"PRIu64"\n", gif2bmp->bmpSize);

    // Uvolnění paměti po tabulce výsledných indexů výstupního souboru
    freeBMPData();

    // Pokud existuje globální tabulka barev
    if(globalColorTable != NULL) {
        // Uvolnění prostoru po globální tabulce barev,
        free(globalColorTable);
    }

    // Návratová hodnota funkce
    return RETURN_SUCCESS;
}
