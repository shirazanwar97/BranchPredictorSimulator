#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <vector>
#include "sim_bp.h"
#include <bitset>
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim gshare 9 3 gcc_trace.txt
    sim bimodal 6 gcc_trace.txt
    ./sim hybrid 8 14 10 5 gcc_trace.txt
    argc = 4
    argv[0] = "sim"
    argv[1] = "bimodal"
    argv[2] = "6"
    ... and so on
*/

vector<int> predictionTableForBiModal, predictionTableForGShare, chooserTable;
long int mispredictCounter = 0, predictionCounter = 0;
unsigned long int GHR = 0;

void initializePredictionTableForGShare(unsigned long int m){
    predictionTableForGShare = vector<int> (1 << m, 2);
    GHR = 0; //might be needed to mask this to N bits??
}

void initializePredictionTableForBiModal(unsigned long int m){
    predictionTableForBiModal = vector<int> (1 << m, 2); //1 << m is equivalent to 2 ki power M.
}

void initializePredictionTableForHybrid(unsigned long int chooserTableSize, unsigned long int biModalPdTableSize, unsigned long int GsharePdTableSize){
    chooserTable = vector<int> (1 << chooserTableSize, 1); //initial value = 1
    predictionTableForBiModal = vector<int> (1 << biModalPdTableSize, 2); //1 << m is equivalent to 2 ki power M.
    predictionTableForGShare = vector<int> (1 << GsharePdTableSize, 2);
    GHR = 0;
}

unsigned long int getIndex(unsigned long int adr, unsigned long int indexSize){
    unsigned long int mask = ((1 << indexSize) - 1);
    unsigned long int index = (adr >> 2) & mask;
    return index;
}

char predictAndSetFlag(unsigned long int index, vector<int> &predictionTable){
    char flag = '\0';
    if(predictionTable[index]==0 || predictionTable[index]==1)
        flag = 'n';
    else if(predictionTable[index]==2 || predictionTable[index]==3)
        flag = 't';
    return flag;
}

void updatePredictorTableAfterCompare(unsigned long int index, char actualFlagTorN, vector<int> &predictionTable){
    if((actualFlagTorN=='t') && (predictionTable[index]!=3)){            
            predictionTable[index]++;
    }
    else if((actualFlagTorN=='n') && (predictionTable[index]!=0)){
        predictionTable[index]--;
    }
}

void updatePredictorTable(unsigned long int index, char predictedFlagAsTOrN, char actualFlagTorN, vector<int> &predictionTable){
    if(predictedFlagAsTOrN == actualFlagTorN){
        updatePredictorTableAfterCompare(index, actualFlagTorN, predictionTable);
    }
    else if(predictedFlagAsTOrN != actualFlagTorN){
        mispredictCounter++;
        updatePredictorTableAfterCompare(index, actualFlagTorN, predictionTable);
    }
}

unsigned long int extractLSB(unsigned long int value, int noOfBitsToExtract) {
    unsigned long int mask = (1 << noOfBitsToExtract) - 1;
    unsigned long int result = value & mask;
    return result;
}

unsigned long int extractMSB(unsigned long int value, unsigned long int m, unsigned long int n) {
    unsigned long int mask = ((1 << (n)) - 1);
    unsigned long int shiftedValue = (value >> (m-n));
    unsigned long int indexMSB = shiftedValue & mask;
    return indexMSB;
}

unsigned long int getIndexForGShare(unsigned long int address, unsigned long int addressSize, unsigned long int ghrSize){
    unsigned long int preIndex = getIndex(address, addressSize);
    unsigned long int addressMSB = extractMSB(preIndex, addressSize, ghrSize);
    unsigned long int indexMSB = (GHR ^ addressMSB);
    unsigned long int indexLSB = extractLSB(preIndex, (addressSize - ghrSize));
    indexMSB = indexMSB << (addressSize - ghrSize);
    unsigned long int index = indexMSB + indexLSB;
    return index;
}

void updateGHR(char flag, unsigned long int N){
    unsigned int mask = (1 << N) - 1;
    if(flag == 't'){
        GHR = (GHR >> 1) | (1 << (N - 1));
    } else
        GHR = (GHR >> 1);
    GHR = GHR & mask;
}

void biModal(unsigned long int address, char actualFlagTorN, unsigned long int m){
    char predictedFlagAsTOrN;
    unsigned long int index = getIndex(address, m);
    predictedFlagAsTOrN = predictAndSetFlag(index, predictionTableForBiModal);
    // cout << "BP: " << index << " " << predictionTableForBiModal[index] << endl;
    updatePredictorTable(index, predictedFlagAsTOrN, actualFlagTorN, predictionTableForBiModal);
    // cout << "BU: " << index << " " << predictionTableForBiModal[index] << endl;
}

void gshare(unsigned long int address, char actualFlagTorN, unsigned long int addressSize, unsigned long int ghrSize){
    char predictedFlagAsTOrN;
    unsigned long int index = getIndexForGShare(address, addressSize, ghrSize);
    predictedFlagAsTOrN = predictAndSetFlag(index, predictionTableForGShare);
    // cout << "BP: " << index << " " << predictionTableForGShare[index] << endl;
    updatePredictorTable(index, predictedFlagAsTOrN, actualFlagTorN, predictionTableForGShare);
    updateGHR(actualFlagTorN, ghrSize);
    // cout << "BU: " << index << " " << predictionTableForGShare[index] << endl;
}

void updateChooserTable(unsigned long int index ,char actualFlag, char biModalPredictedFlag, char gSharePredictedFlag, vector<int> &chooserTable){
    if(biModalPredictedFlag != gSharePredictedFlag){
        if(actualFlag == biModalPredictedFlag && (chooserTable[index]!=0))
            chooserTable[index]--;
        else if(actualFlag == gSharePredictedFlag && (chooserTable[index]!=3))
            chooserTable[index]++;
    }
}

void hybrid(unsigned long int address,char actualFlagTorN, unsigned long int chooserTableSize, unsigned long int gSharePdTableSize, unsigned long int biModalPdTableSize, unsigned long int ghrSize){
    char biModalPredictedFlagAsTOrN, gSharePredictedFlagAsTOrN;
    unsigned long int index = getIndex(address, chooserTableSize);

    //prediction from biModal
    unsigned long int indexForBiModal = getIndex(address, biModalPdTableSize);
    biModalPredictedFlagAsTOrN = predictAndSetFlag(indexForBiModal, predictionTableForBiModal);

    //prediction from gShare
    unsigned long int indexForGShare = getIndexForGShare(address, gSharePdTableSize, ghrSize);
    gSharePredictedFlagAsTOrN = predictAndSetFlag(indexForGShare, predictionTableForGShare);
     
    if(chooserTable[index]==1 || chooserTable[index]==0){
        //biModal prediction table update
        updatePredictorTable(indexForBiModal, biModalPredictedFlagAsTOrN, actualFlagTorN, predictionTableForBiModal);
    }
    else if(chooserTable[index]==3 || chooserTable[index]==2){
        updatePredictorTable(indexForGShare, gSharePredictedFlagAsTOrN, actualFlagTorN, predictionTableForGShare);
        //gShare prediction table update
    }

    // //updating GHR
    updateGHR(actualFlagTorN, ghrSize);

    // //updating chooser table
    updateChooserTable(index, actualFlagTorN, biModalPredictedFlagAsTOrN, gSharePredictedFlagAsTOrN, chooserTable);
}

void printPredictionTableContent(vector<int> predictionTable){
    for (unsigned int i = 0; i < predictionTable.size(); i++) {
        cout << " " << i << "\t" << predictionTable[i] << endl;
    }
}

int main (int argc, char* argv[])
{
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    bp_params params;       // look at sim_bp.h header file for the the definition of struct bp_params
    char outcome;           // Variable holds branch outcome
    unsigned long int addr; // Variable holds the address read from input file
    params.K = 0;
    params.M1 = 0;
    params.M2 = 0;
    params.N = 0;

    if (!(argc == 4 || argc == 5 || argc == 7))
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    params.bp_name  = argv[1];
    
    // strtoul() converts char* to unsigned long. It is included in <stdlib.h>
    if(strcmp(params.bp_name, "bimodal") == 0)              // Bimodal
    {
        if(argc != 4)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M2       = strtoul(argv[2], NULL, 10);
        trace_file      = argv[3];
        printf("COMMAND\n%s %s %lu %s\n", argv[0], params.bp_name, params.M2, trace_file);
        initializePredictionTableForBiModal(params.M2);
    }
    else if(strcmp(params.bp_name, "gshare") == 0)          // Gshare
    {
        if(argc != 5)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M1       = strtoul(argv[2], NULL, 10);
        params.N        = strtoul(argv[3], NULL, 10);
        trace_file      = argv[4];
        printf("COMMAND\n%s %s %lu %lu %s\n", argv[0], params.bp_name, params.M1, params.N, trace_file);
        initializePredictionTableForGShare(params.M1);
    }
    else if(strcmp(params.bp_name, "hybrid") == 0)          // Hybrid
    {
        if(argc != 7)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.K        = strtoul(argv[2], NULL, 10);
        params.M1       = strtoul(argv[3], NULL, 10);
        params.N        = strtoul(argv[4], NULL, 10);
        params.M2       = strtoul(argv[5], NULL, 10);
        trace_file      = argv[6];
        printf("COMMAND\n%s %s %lu %lu %lu %lu %s\n", argv[0], params.bp_name, params.K, params.M1, params.N, params.M2, trace_file);
        initializePredictionTableForHybrid(params.K, params.M2, params.M1);
    }
    else
    {
        printf("Error: Wrong branch predictor name:%s\n", params.bp_name);
        exit(EXIT_FAILURE);
    }
    
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }
    
    char str[2];
    while(fscanf(FP, "%lx %s", &addr, str) != EOF)
    {
        outcome = str[0];
        predictionCounter++;
        if(strcmp(params.bp_name, "bimodal") == 0){ //biModal
            biModal(addr, outcome, params.M2);
        }
        if(strcmp(params.bp_name, "gshare") == 0){ //gshare
            gshare(addr, outcome, params.M1, params.N);
        }
        else if(strcmp(params.bp_name, "hybrid") == 0){ //hybrid
            hybrid(addr, outcome, params.K, params.M1, params.M2, params.N);
        }
    }
    
    cout<< "OUTPUT" << endl;
    cout<< "number of predictions: " << predictionCounter << endl;
    cout<< "number of mispredictions: " << mispredictCounter << endl;
    cout<< "misprediction rate: " << fixed << setprecision(2) <<(float) (((float)mispredictCounter/(float)predictionCounter)*100) << "%" << endl;

    if(strcmp(params.bp_name, "bimodal") == 0){
        cout << "FINAL BIMODAL CONTENTS" << endl;
        printPredictionTableContent(predictionTableForBiModal);
    } else if(strcmp(params.bp_name, "gshare") == 0){ //gshare
        cout << "FINAL GSHARE CONTENTS" << endl;
        printPredictionTableContent(predictionTableForGShare);     
    } else if(strcmp(params.bp_name, "hybrid") == 0){
        cout << "FINAL CHOOSER CONTENTS" << endl;
        printPredictionTableContent(chooserTable);
        cout << "FINAL GSHARE CONTENTS" << endl;
        printPredictionTableContent(predictionTableForGShare);
        cout << "FINAL BIMODAL CONTENTS" << endl;
        printPredictionTableContent(predictionTableForBiModal);
    }

    return 0;
}