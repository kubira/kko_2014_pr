/*******************************************************************************
*  Soubor:   gif2bmp.h                                                         *
*  Autor:    Radim Kubiš, xkubis03                                             *
*  Vytvořen: 8. března 2014                                                    *
*                                                                              *
*  Projekt do předmětu Kódování a komprese (KKO) 2014                          *
*                                                                              *
*                    KONVERZE OBRAZOVÉHO FORMÁTU GIF NA BMP                    *
*                   ----------------------------------------                   *
*                                                                              *
*  Hlavičkový  soubor  pro  knihovnu  gif2bmp.c,  který  obsahuje  konstanty,  *
*  definice používaných struktur a prototyp konverzní funkce GIF na BMP.       *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdint.h>

// Login autora
#define LOGIN "xkubis03"
// Návratová hodnota funkce při neúspěchu
#define RETURN_FAILURE -1
// Návratová hodnota funkce při úspěchu
#define RETURN_SUCCESS  0
// Hodnota pro nenastavené flagy souboru
#define FLAG_FALSE 0
// Hodnota pro nastavené flagy souboru
#define FLAG_TRUE 1
// Hodnota pro přechod do dalšího bajtu
#define BYTE_OVERFLOW 256
// Podporovaná signatura knihovny
#define GIF_SIGNATURE "GIF89a"
// Délka podporované signatury knihovny
#define GIF_SIGNATURE_LENGTH 6
// AND hodnota pro získání příznaku tabulky barev
#define AND_OF_COLOR_TABLE_FLAG 128
// AND hodnota pro získání počtu bitů na pixel
#define AND_OF_BPP 112
// Počet bitů pro posun počtu bitů na pixel
#define BPP_SHIFT 4
// AND hodnota pro získání příznaku setřídění tabulky barev
#define AND_OF_SORT_FLAG 8
// AND hodnota pro získání velikosti tabulky barev
#define AND_OF_COLOR_TABLE_SIZE 7
// Úvodní bajt pro blok s obrazovými daty
#define IMAGE_BLOCK_ID 0x2c
// Úvodní bajt pro blok s rozšířením
#define EXTENSION_BLOCK_ID 0x21
// Rozšíření graphic control
#define GRAPHIC_CONTROL_BLOCK_ID 0xf9
// Rozšíření comment
#define COMMENT_BLOCK_ID 0xfe
// Rozšíření plain text
#define PLAIN_TEXT_BLOCK_ID 0x01
// Rozšíření application
#define APPLICATION_BLOCK_ID 0xff
// Ukončovací bajt bloku
#define BLOCK_TERMINATOR 0x00
// AND pro příznak prokládání
#define AND_OF_INTERLACE_FLAG 64
// AND pro příznak seřazení v bloku obrazových dat
#define AND_OF_IMAGE_BLOCK_SORT 32
// AND pro disposal method v graphic control bloku
#define AND_OF_DISPOSAL_METHOD 28
// Počet bitů pro posun disposal method
#define DISPOSAL_METHOD_SHIFT 2
// AND pro user input flag
#define AND_OF_USER_INPUT_FLAG 2
// AND pro transparent color flag
#define AND_OF_TRANSPARENT_FLAG 1
// Délka identifikátoru v bloku aplikace
#define APPLICATION_IDENTIFIER_LENGTH 8
// Délka kódu aplikace
#define APPLICATION_CODE_LENGTH 3
// Ukončovací bajt GIF souboru
#define TRAILER 0x3b
// Velikost, o kterou se zvětšuje pole indexů v položce tabulky barev
#define ITEM_ALLOC_SIZE    8
// Velikost, o kterou se zvětšuje pole položek tabulky barev
#define TABLE_ALLOC_SIZE 256
// Násobek pro dorovnání řádku výstupního souboru
#define ROW_MULT_SIZE 4
// Identifikátor BMP souboru
#define BMP_IDENTIFICATOR "BM"
// Velikost hlavičky BMP souboru
#define BITMAPFILEHEADER_SIZE 14
// Velikost informační BMP hlavičky
#define BITMAPINFOHEADER_SIZE 40
// Velikost položky tabulky barev BMP souboru
#define BMP_COLOR_SIZE 4
// Hodnota rezervovaných položek hlavičky
#define RESERVED_VALUE 0x0
// Hodnota bitový rovin výstupního zařízení
#define BI_PLANES_VALUE 0x1
// Počet bitů na pixel ve výstupním souboru
#define BIT_COUNT 24
// Identifikátor metody komprese
#define COMPRESSION_METHOD 0x0
// Hodnota pro ANO při prvním bajtu nového bloku za clear kódem
#define YES 1
// Hodnota NE pro přechozí ANO
#define NO 0
// Velikost popisu jednoho pixelu
#define ONE_PIXEL_SIZE 3
// Stav pro první blok prokládání 8n
#define STATE0 0
// Stav pro druhý blok prokládání 8n+4
#define STATE1 1
// Stav pro třetí blok prokládání 4n+2
#define STATE2 2
// Stav pro čtvrtý blok prokládání 2n+1
#define STATE3 3

/*
 * Struktura pro uložení jedné barvy v RGB
 *
 * r - červená složka barvy
 * g - zelená složka barvy
 * b - modrá složka barvy
 */
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} tRGB;

/*
 * Struktura pro uložení informací z hlavičky vstupního GIF souboru
 *
 * imageWidth   - logická šířka obrazovky (v pixelech)
 * imageHeight  - logická výška obrazovky (v pixelech)
 * gctFlag      - příznak globální tabulky barev
 *                   0 - nepřítomna
 *                   1 - přítomna
 * bpp - rozlišení barev (bitů na pixel)
 * gctSortFlag  - příznak setřídění globální tabulky barev
 *                   0 - nesetříděno
 *                   1 - setříděno
 * gctSize      - velikost globální tabulky barev (počet barev)
 * bgColorIndex - index barvy pozadí
 * paRatio      - poměr mezi výškou a šířkou pixelu
 */
typedef struct {
    uint16_t imageWidth;
    uint16_t imageHeight;
    uint8_t gctFlag;
    uint8_t bpp;
    uint8_t gctSortFlag;
    uint16_t gctSize;
    uint8_t bgColorIndex;
    uint8_t paRatio;
} tGIFInfo;

/*
 * Struktura pro uložení informací o převodu
 *
 * bmpSize - velikost dekódovaného souboru
 * gifSize - velikost kódovaného souboru
 */
typedef struct {
  int64_t bmpSize;
  int64_t gifSize;
} tGIF2BMP;

/*
 * Struktura pro uložení hodnot pixelů v tabulce barev od indexu 258
 *
 * allocated - počet alokovaných bajtů ve struktuře
 * used      - počet využitých bajtů ve struktuře
 * indexList - pole indexů do tabulky barev (0 - 255)
 */
typedef struct {
    uint32_t allocated;
    uint32_t used;
    uint8_t *indexList;
} tItem;

/*
 * Struktura pro uložení tabulky barev od indexu 258
 *
 * allocated - počet alokovaných míst v tabulce
 * used      - počet využitých míst v tabulce
 * itemList  - pole položek tabulky
 */
typedef struct {
    uint32_t allocated;
    uint32_t used;
    tItem *itemList;
} tTable;

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
int gif2bmp(tGIF2BMP *gif2bmp, FILE *inputFile, FILE *outputFile);
