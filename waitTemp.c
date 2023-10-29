#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <string.h>

// CPU 核心溫度的檔案位置
char processorTempDir[60];

// 獲取終端機長度
int getTerminalWidth() {
    int defaultLen = 80;

    FILE *term = popen("tput cols", "r");
    if (term == NULL) {
        return defaultLen;
    }

    int cols;
    if (fscanf(term, "%d", &cols) != 1) {
        cols = defaultLen;
    }

    pclose(term);

    return cols;
}

// 格式化輸出溫度
void printfProcessorsTemperature(int *coreTemps, int numCores){
    int terminalWidth = getTerminalWidth();
    int changeLinePivot = terminalWidth / 16;

    for (int i = 0; i < numCores; i++){
        if (coreTemps[i] == -1){
            continue;
        }
        printf("Core %2d: %02d \u00b0C\t", i, coreTemps[i]);
        if ((i + 1) % changeLinePivot == 0 && i != 0) printf("\n");
    }
    printf("\n");    
}

// 尋找並設定 CPU 核心溫度的檔案位置
int setProcessorsTempPath(){
    char parentDirname[30] = "/sys/class/hwmon/";
    char childDirname[50];
    char fullFilename[60];

    FILE *file;
    char buffer[256];

    DIR *dir;
    DIR *subDir;
    struct dirent *entry;

    dir = opendir("/sys/class/hwmon/");

    if (dir == NULL) {
        perror("無法打開目錄");
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_LNK) {
            sprintf(childDirname, "%s%s", parentDirname, entry->d_name);

            subDir = opendir(childDirname);
            while ((entry = readdir(subDir)) != NULL) {
                if (entry->d_type == DT_REG && strcmp(entry->d_name, "name") == 0){
                    sprintf(fullFilename, "%s/name", childDirname);

                    file = fopen(fullFilename, "r");

                    if (file == NULL) {
                        perror("無法打開文件");
                    }

                    if(fgets(buffer, sizeof(buffer), file) == NULL){
                        perror("無法打開文件");
                    }
                    fclose(file);

                    if (strcmp(buffer, "coretemp\n") == 0){
                        sprintf(processorTempDir, "%s", childDirname);
                        break;
                    }
                }
            }
            closedir(subDir);
        }

        if (strlen(processorTempDir) > 0){
            break;
        }
    }

    if (strlen(processorTempDir) == 0){
        perror("設定溫度文件路徑失敗");
        return 1;
    }

    closedir(dir);
    return 0;
}

// 設定 Processor 溫度
int setProcessorsTemperature(int *coreTemps, int numCores){
    char tempfilename[60];
    FILE* file;

    int temp = 0;

    for (int i = 0; i < numCores; i++){
        sprintf(tempfilename, "%s/temp%d_input", processorTempDir, i + 2);
        file = fopen(tempfilename, "r");
        if (file == NULL){
            // 沒有這個檔案
            // 設溫度為 -1
            coreTemps[i] = -1;
        }else{
            fscanf(file, "%d", &temp);
            coreTemps[i] = temp / 1000;
        }
    }
    return 1;
}

// 檢查所有 Processor 溫度是否達到標準溫度
int checkAllProcessorTemperatures(int *coreTemps, int numCores, int targetTemp){
    for (int i = 0; i < numCores; i++){
        if (coreTemps[i] > targetTemp) return 0;
    }
    return 1;
}

// 獲取當前時間
char* getCurrentTime(){
    time_t currentTime = time(NULL);
    struct tm *timeInfo;
    char *timeString = (char*) malloc (sizeof(char) * 80);

    time(&currentTime);

    timeInfo = localtime(&currentTime);
    strftime(timeString, 80, "%Y-%m-%d %H:%M:%S", timeInfo);

    return timeString;
}

int main(int argc, char const *argv[])
{
    int checkTempPath = setProcessorsTempPath();
    if (checkTempPath == 1) {
        return 1;
    }
    // 沒有接收到參數
    if (argc < 2){
        fprintf(stderr, "沒有參數\n");
        return 1;
    }

    // 檢查參數是否為有效數字
    for (int i = 0; argv[1][i] != '\0'; i++){
        if (!isdigit(argv[1][i])){
            fprintf(stderr, "參數非數字\n");
            return 1;
        }
    }

    int targetTemp = atoi(argv[1]);

    // 獲取核心數量
    int numCores = sysconf(_SC_NPROCESSORS_ONLN);

    // 初始化核心溫度陣列
    int *coreTemps = (int *) malloc(sizeof(int) * numCores);

    char *currentTime = getCurrentTime();

    int timesCounter = 0;

    // 做迴圈檢查直到溫度符合標準
    do {
        if(!setProcessorsTemperature(coreTemps, numCores)){
            fprintf(stderr, "獲取核心溫度失敗\n");
            return 1; //獲取核心溫度失敗
        }
        // 睡眠 1 秒
        printf("\033c"); // 清空終端機
        sleep(1);
        timesCounter++;
        printf("CPU temperature info dir: %s\n", processorTempDir);
        printf("Time Begin:\t%s\n", currentTime);
        printf("Target temp:\t%02d \u00b0C\n", targetTemp);
        printf("## Time Count: %d\n", timesCounter);
        printfProcessorsTemperature(coreTemps, numCores);
    } while (!checkAllProcessorTemperatures(coreTemps, numCores, targetTemp));

    currentTime = getCurrentTime();
    printf("Time End:\t%s\n", currentTime);
    printfProcessorsTemperature(coreTemps, numCores);

    free(currentTime);
    free(coreTemps);

    return 0;
}
