/*******************************************************************************
*  Soubor:   main.c                                                            *
*  Autor:    Radim Kubiš, xkubis03                                             *
*  Vytvořen: 8. března 2014                                                    *
*                                                                              *
*  Projekt do předmětu Kódování a komprese (KKO) 2014                          *
*                                                                              *
*                    KONVERZE OBRAZOVÉHO FORMÁTU GIF NA BMP                    *
*                   ----------------------------------------                   *
*                                                                              *
*  Demonstrační  aplikace  pro  knihovnu  gif2bmp. Provádí převod formátu GIF  *
*  na BMP, navíc umožňuje zápis logovacího souboru s informacemi o provedeném  *
*  převodu (původní a nová velikost souboru + login řešitele projektu).        *
*                                                                              *
*******************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <inttypes.h>
#include "gif2bmp.h"

/*
 * Struktura vstupních argumentů
 */
typedef struct {
    // Počet vstupních argumentů programu
    int argc;
    // Pole vstupních argumentů programu
    char **argv;

    // Příznak zadaného přepínače -h
    uint8_t helpFlag;

    // Ukazatel pro název vstupního souboru
    char *inputFileName;
    // Ukazatel pro název výstupního souboru
    char *outputFileName;
    // Ukazatel pro název logovacího souboru
    char *logFileName;

    // Ukazatel pro vstupní soubor
    FILE *inputFile;
    // Ukazatel pro výstupní soubor
    FILE *outputFile;
    // Ukazatel pro logovací soubor
    FILE *logFile;
} tArguments;

/*
 * Funkce pro výpis způsobu použití programu
 *
 * programName - název spouštěného programu
 */
void printHelp(char *programName) {
    // Tisk způsobu použití programu
    fprintf(stdout, "Usage: %s [-i input_file] [-o output_file] [-l log_file] [-h]\n\n", programName);
    fprintf(stdout, "  -h print help\n");
    fprintf(stdout, "  -i input file name (GIF), default: stdin\n");
    fprintf(stdout, "  -o output file name (BMP), default: stdout\n");
    fprintf(stdout, "  -l log file name, default: without log file\n");
}

/*
 * Funkce pro zpracování vstupních argumentů příkazové řádky
 *
 * args - struktura argumentů programu
 */
void parseArguments(tArguments *args) {
    // Index do pole argumentů
    int argIndex = 0;
    // Proměnná pro aktuálně načtený přepínač
    int actualChar = 0;

    // Nastavení chybového kódu zpracování argumentů na bezchybný stav
    opterr = 0;

    // Procházení vstupních argumentů programu
    while((actualChar = getopt(args->argc, args->argv, "i:o:l:h")) != -1) {
        // Rozvětvení podle právě získaného přepínače
        switch(actualChar) {
            // Větev přepínače názvu vstupního souboru
            case 'i': {
                // Uložení názvu vstupního souboru
                args->inputFileName = optarg;
                // Konec větve
                break;
            }
            // Větev přepínače názvu výstupního souboru
            case 'o': {
                // Uložení názvu výstupního souboru
                args->outputFileName = optarg;
                // Konec větve
                break;
            }
            // Větev přepínače názvu logovacího souboru
            case 'l': {
                // Uložení názvu logovacího souboru
                args->logFileName = optarg;
                // Konec větve
                break;
            }
            // Větev přepínače výpisu způsobu použití programu
            case 'h': {
                // Uložení přítomnosti příznaku výpisu nápovědy
                args->helpFlag = 1;
                // Konec větve
                break;
            }
            // Větev neočekávaného vstupního argumentu
            case '?': {
                // Pokud je očekáván argument některého prřepínače
                if(optopt == 'i' || optopt == 'o' || optopt == 'l') {
                    // Výpis chyby
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                    // Výpis nápovědy
                    printHelp(args->argv[0]);
                    // Konec programu s chybou
                    exit(EXIT_FAILURE);
                } else if(isprint(optopt)) {
                    // Pokud je přepínač tisknutelný znak

                    // Výpis chyby
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                    // Výpis nápovědy
                    printHelp(args->argv[0]);
                    // Konec programu s chybou
                    exit(EXIT_FAILURE);
                } else {
                    // Pokud je přepínač jiný

                    // Tisk chyby
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                    // Výpis nápovědy
                    printHelp(args->argv[0]);
                    // Konec programu s chybou
                    exit(EXIT_FAILURE);
                }
            }
            // Větev pro další neočekávané chování zpracování argumentů
            default: {
                // Tisk chyby
                fprintf(stderr, "Unknown program argument error.\n");
                // Výpis nápovědy
                printHelp(args->argv[0]);
                // Ukončení programu s chybou
                exit(EXIT_FAILURE);
            }
        }
    }

    // Výpis dalších nezpracovaných argumentů programu
    for(argIndex = optind; argIndex < args->argc; argIndex++) {
        // Tisk chyby nezpracovaného argumentu
        fprintf(stderr, "Non-option argument '%s'.\n", args->argv[argIndex]);
    }

    // Kontrola zda existovaly další nezpracované argumenty programu
    if(optind < args->argc) {
        // Výpis nápovědy
        printHelp(args->argv[0]);
        // Konec programu s chybou
        exit(EXIT_FAILURE);
    }
}

/*
 * Funkce pro úklid na konci programu
 *
 * args - struktura argumentů programu
 */
void cleanUp(tArguments *args) {
    // Pokud existoval vstupní soubor
    if(args->inputFile != NULL && args->inputFile != stdin) {
        // Uzavření souboru
        fclose(args->inputFile);
    }
    // Pokud existoval výstupní soubor
    if(args->outputFile != NULL && args->outputFile != stdout) {
        // Uzavření souboru
        fclose(args->outputFile);
    }
    // Pokud existoval logovací soubor
    if(args->logFile != NULL) {
        // Uzavření souboru
        fclose(args->logFile);
    }
}

/*
 * Funkce pro kontrolu vstupních argumentů, přepínaču a nastavení jejich
 * defaultních hodnot
 *
 * args - struktura argumentů programu
 */
void checkArguments(tArguments *args) {
    // Pokud byl zadán přepínač výpisu nápovědy
    if(args->helpFlag == 1) {
        // Výpis nápovědy
        printHelp(args->argv[0]);
        // Ukončení funkce/programu bez chyby
        exit(EXIT_SUCCESS);
    }
    // Pokud nebyl zadán název vstupního souboru
    if(args->inputFileName == NULL) {
        // Vstupem bude standardní vstup programu
        args->inputFile = stdin;
    } else {
        // Pokud byl název vstupního souboru zadán

        // Otevření vstupního souboru
        args->inputFile = fopen(args->inputFileName, "r");
        // Pokud se nepodařilo vstupní soubor otevřít
        if(args->inputFile == NULL) {
            // Tisk chyby
            fprintf(stderr, "Cannot open input file '%s' for read\n", args->inputFileName);
            // Konec programu s chybou
            exit(EXIT_FAILURE);
        }
    }
    // Pokud nebyl zadán název výstupního souboru
    if(args->outputFileName == NULL) {
        // Výstupem bude standardní výstup programu
        args->outputFile = stdout;
    } else {
        // Pokud byl název výstupního souboru zadán

        // Otevření výstupního souboru
        args->outputFile = fopen(args->outputFileName, "w");
        // Pokud se nepodařilo výstupní soubor otevřít
        if(args->outputFile == NULL) {
            // Tisk chyby
            fprintf(stderr, "Cannot open output file '%s' for write\n", args->outputFileName);
            // Úklid
            cleanUp(args);
            // Konec programu s chybou
            exit(EXIT_FAILURE);
        }
    }
    // Pokud nebyl zadán název logovacího souboru
    if(args->logFileName == NULL) {
        // Nic se neděje
    } else {
        // Pokud byl název logovacího souboru zadán

        // Otevření logovacího souboru
        args->logFile = fopen(args->logFileName, "w");
        // Pokud se nepodařilo logovací soubor otevřít
        if(args->logFile == NULL) {
            // Tisk chyby
            fprintf(stderr, "Cannot open log file '%s' for write\n", args->outputFileName);
            // Úklid
            cleanUp(args);
            // Konec programu s chybou
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Funkce pro zápis výstupní zprávy
 *
 * args - struktura argumentů programu
 * info - struktura s informacemi o převodu
 */
void writeLog(tArguments args, tGIF2BMP info) {
    // Pokud je požadován logovací soubor
    if(args.logFile != NULL) {
        // Zápis loginu
        fprintf(args.logFile, "login = %s\n", LOGIN);
        // Zápis nové velikosti BMP
        fprintf(args.logFile, "uncodedSize = %"PRId64"\n", info.bmpSize);
        // Zápis původní velikosti GIF
        fprintf(args.logFile, "codedSize = %"PRId64"\n", info.gifSize);
    }
}

/*
 * Funkce main - hlavní funkce testovacího programu
 *
 * argc - počet vstupních parametrů příkazové řádky
 * argv - pole vstupních parametrů příkazové řádky
 *
 * Návratová hodnota:
 *      RETURN_SUCCESS  (0) - program proběhl v pořádku
 *      RETURN_FAILURE (-1) - program proběhl s chybou
 */
int main(int argc, char *argv[]) {
    // Proměnná pro ukládání aktuálního/chybového stavu programu
    int programState = RETURN_SUCCESS;
    // Inicializace struktury argumentů
    tArguments args = {argc, argv, 0, NULL, NULL, NULL, NULL, NULL, NULL};
    // Struktura informací o převodu
    tGIF2BMP infoStruct = {0, 0};

    // Zpracování vstupních argumentů programu
    parseArguments(&args);
    // Kontrola vstupních argumentů programu
    checkArguments(&args);

    // Převod vstupního souboru GIF na výstupní soubor BMP
    programState = gif2bmp(&infoStruct, args.inputFile, args.outputFile);

    // Zápis logu do souboru
    writeLog(args, infoStruct);

    // Úklid na konci programu
    cleanUp(&args);

    // Návratová hodnota programu/převodu
    return programState;
}
