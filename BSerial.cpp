#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <vector>
#include <iostream>
#include <ctime>
#include <string>
#include <assert.h>


#include "atlstr.h"

#include <devguid.h>
#include <regstr.h>
#include <setupapi.h>

#pragma	comment(lib,"setupapi.lib")

#define		__DBG_LOG						1
#define		__DBG_ERR						1
#define 	__DBG_INFO						1
#define		__DBG_FUNC						1

#if __DBG_LOG
#define DBG_LOG(...)		(printf("[%04d][%-30s][LOG] --- ", __LINE__, __FUNCTION__ ),printf( __VA_ARGS__))
#else /* __DBG_LOG */
#define DBG_LOG(...)
#endif /* __DBG_LOG */

#if __DBG_ERR
#define DBG_ERR(...)		(printf("[%04d][%-30s][ERR] --- ", __LINE__, __FUNCTION__ ),printf( __VA_ARGS__))
#else /* __DBG_ERR */
#define DBG_ERR(...)
#endif /* __DBG_ERR */

#if __DBG_INFO
#define DBG_INFO(...)		(printf("[%04d][%-30s][INFO] --- ", __LINE__, __FUNCTION__ ),printf( __VA_ARGS__))
#else /* __DBG_INFO */
#define DBG_INFO(...)
#endif /* __DBG_INFO */

#if __DBG_FUNC
#define DBG_FUNC_IN		printf("[%04d][%-30s] BEGIN\n", __LINE__, __FUNCTION__);
#define DBG_FUNC_OUT(ret)	printf("[%04d][%-30s] END ret=%d\n", __LINE__, __FUNCTION__, (ret));
#define DBG_FUNC_OUTP(ret)	printf("[%04d][%-30s] END ret=0x%08x\n", __LINE__, __FUNCTION__, (ret));
#else /* __DBG_FUNC */
#define DBG_FUNC_IN
#define DBG_FUNC_OUT(ret)
#define DBG_FUNC_OUTP(ret)
#endif /* __DBG_FUNC */





#define READ_BUFSZ          1024
static char rbuf[READ_BUFSZ];
HANDLE hLog;
HANDLE hCom;
HANDLE hReadMutex;
HANDLE hWriteMutex;
BOOL RunFlag = TRUE;

void log_timestamp(void)
{
    char buffer[80];
    BOOL ret;
    DWORD len;
    float sec;

    SYSTEMTIME t;
    GetSystemTime(&t); // or GetLocalTime(&t)
    sec = (float)t.wSecond + ((float)(t.wMilliseconds) / 1000);
    sprintf(buffer, "\n[%04d/%02d/%02d %02d:%02d:%02.05f] ",
        t.wYear, t.wMonth, t.wDay,
        t.wHour, t.wMinute, sec);

    ret = WriteFile(hLog, buffer, (DWORD)strlen(buffer), &len, NULL);
    if (!ret) {
        std::cerr << "Write timestamp Failed";
    }
}

static void log_char(char c)
{
    int ret;
    DWORD len;

    ret = WriteFile(hLog, &c, 1, &len, NULL);
    if (!ret) {
        DBG_ERR("Write timestamp Failed\n");
    }
}

const char* log_file_name(int port)
{
    time_t rawtime;
    struct tm* timeinfo;
    char timestr[128];
    static char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timestr, sizeof(timestr), "%Y%m%d_%H%M%S", timeinfo);
    sprintf_s(buffer, sizeof(buffer), "SerialLog_COM%d_%s.txt", port, timestr);
    return buffer;

}

#define LOG_PATH_LEN            1024
const char* ExeDir(void)
{
    static char dir[LOG_PATH_LEN];
    GetModuleFileNameA(NULL, dir, LOG_PATH_LEN);

    char *c = strrchr(dir, '\\');
    *c = '\0';
    return dir;
}

BOOL DirectoryExists(LPCSTR szPath)
{
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL OpenLogFile(int port)
{
    char iniPath[LOG_PATH_LEN];
    char logPath[LOG_PATH_LEN];
    int logPathLen;
    const char *exeDir = ExeDir();
    sprintf(iniPath, "%s\\BSerial.ini", exeDir);

    logPathLen = GetPrivateProfileStringA("LOG", "LOG_DIR", exeDir, logPath, LOG_PATH_LEN, iniPath);
    if (logPathLen <= 0 || !DirectoryExists(logPath)) {
        DBG_ERR("Read INI failed use default directory\n");
        strcpy(logPath, ".");
        logPathLen = (int)strlen(logPath);
    }

    assert(logPathLen > 0);

    if (logPath[logPathLen - 1] == '\\') logPathLen--;

    sprintf(logPath + logPathLen, "\\%s", log_file_name(port));
    hLog = CreateFileA(logPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hLog == INVALID_HANDLE_VALUE)
    {
        DBG_ERR("Open Log File Failed\n");
        return FALSE;
    }

    return TRUE;
}

#define PORT_NUM_MAX        8
#define PORT_NAME_LEN       512
int ListDevice(const GUID& guid, int portNo[PORT_NUM_MAX], TCHAR portName[PORT_NUM_MAX][PORT_NAME_LEN])
{
    SP_DEVINFO_DATA devInfoData;
    ZeroMemory(&devInfoData, sizeof(devInfoData));
    devInfoData.cbSize = sizeof(devInfoData);

    int nDevice = 0;
    HDEVINFO hDeviceInfo = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    while (SetupDiEnumDeviceInfo(hDeviceInfo, nDevice, &devInfoData)) {
        SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)portName[nDevice], PORT_NAME_LEN, NULL);

        HKEY hkey = SetupDiOpenDevRegKey(hDeviceInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hkey == INVALID_HANDLE_VALUE) {
            printf("SetupDiOpenDevRegKey failed\n");
            return false;
        }
        // Qurey for portname
        TCHAR buf[64];
        DWORD len =64;
        RegQueryValueEx(hkey, _T("PortName"), NULL, NULL, (LPBYTE)buf, &len);
        if (len <= 0) {
            DBG_ERR("Query PortNmae failed\n");
            return false;
        }
        buf[len] = 0;
        wprintf(_T("Port Nmae:%s, len:%d\n"), buf, len);
        swscanf_s(buf, _T("COM%d"), &portNo[nDevice]);
#if 0
        wprintf(L"Friendly name:%s\n", portName[nDevice]);
        wprintf(L"Port name:%s, port number:%d\n", buf, portNo[nDevice]);
#endif
        nDevice++;
    }

    SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return true;
}

BOOL BSerialInit(void)
{
    int portNo[PORT_NUM_MAX];
    TCHAR portName[PORT_NUM_MAX][PORT_NAME_LEN];
    char portNoStr[PORT_NAME_LEN];
    DCB dcbSerialParams = { 0 };  // Initializing DCB structure
    COMMTIMEOUTS timeouts = { 0 };  //Initializing timeouts structure
    char select;
    int number = ListDevice(GUID_DEVINTERFACE_COMPORT, portNo, portName);
    BOOL ret;

    std::cout << "Select a COM Ports:\n";
    for (int i = 0; i < number; i++) {
        wprintf(_T("%d: %s\n"), i, portName[i]);
    }

    select = getchar();
    select -= '0';
    if (select > number || select < 0) {
        DBG_ERR("Please input correct number from %d to %d\n", 0, number - 1);
        return FALSE;
    }


    sprintf(portNoStr, "\\\\.\\COM%d", portNo[select]);
    wprintf_s(_T("Open Com Port: %s\n"), portName[select]);

    hCom = CreateFileA(portNoStr, //friendly name
        GENERIC_READ | GENERIC_WRITE,      // Read/Write Access
        0,                                 // No Sharing, ports cant be shared
        NULL,                              // No Security
        OPEN_EXISTING,                     // Open existing port only
        FILE_ATTRIBUTE_NORMAL,                                 // Non Overlapped I/O
        NULL);                             // Null for Comm Devices
    if (hCom == INVALID_HANDLE_VALUE)
    {
        DBG_ERR("Port:%s can't be opened\n", portNoStr);
        return FALSE;
    }

    //Setting the Parameters for the SerialPort
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    ret = GetCommState(hCom, &dcbSerialParams); //retreives  the current settings
    if (ret == FALSE)
    {
        printf_s("Error to Get the Com state\n\n");
        CloseHandle(hLog);
        CloseHandle(hCom);
        return FALSE;
    }
    dcbSerialParams.BaudRate = CBR_115200;      //BaudRate = 9600
    dcbSerialParams.ByteSize = 8;             //ByteSize = 8
    dcbSerialParams.StopBits = ONESTOPBIT;    //StopBits = 1
    dcbSerialParams.Parity = NOPARITY;      //Parity = None
    ret = SetCommState(hCom, &dcbSerialParams);
    if (ret == FALSE)
    {
        printf_s("Error to Setting DCB Structure\n\n");
        CloseHandle(hLog);
        CloseHandle(hCom);
        return FALSE;
    }
    //Setting Timeouts
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (SetCommTimeouts(hCom, &timeouts) == FALSE)
    {
        printf_s("Error to Setting Time outs");
        CloseHandle(hLog);
        CloseHandle(hCom);
        return FALSE;
    }

    // retry open log 
    for (int i = 0; i < 5; i++) {
        if (OpenLogFile(portNo[select])) break;
        Sleep(1000);
    }

    hReadMutex = CreateMutexW(NULL, FALSE, NULL);      // Set
    hWriteMutex = CreateMutexW(NULL, FALSE, NULL);      // Set


    return TRUE;

}



void ReadProc(void* pMyID)
{
    char* MyID = (char*)pMyID;
    DWORD len;
    int newline_flag = 0;
    BOOL ret;
    DWORD dwEventMask;

    //Setting Receive Mask
    SetCommMask(hCom, EV_RXCHAR);

    while (RunFlag) {
        if (!WaitCommEvent(hCom, &dwEventMask, NULL)) {
            DBG_LOG("WaitCommEvent Failed! canceled?\n");
            continue;
        }

        ret = ReadFile(hCom, &rbuf, READ_BUFSZ - 1, &len, NULL);
        if (len > 0) {
            rbuf[len] = 0;
            std::cout << rbuf;

            for (DWORD i = 0; i < len; i++) {
                if (rbuf[i] == '\r' || rbuf[i] == '\n') {
                    if (!newline_flag) {
                        log_timestamp();
                        newline_flag = 1;
                    }
                }
                else {
                    log_char(rbuf[i]);
                    newline_flag = 0;
                }
            }
        }
    }
}


void WriteProc(void* pMyID)
{
    char* MyID = (char*)pMyID;
    char c;
    DWORD len;
    BOOL ret;

    while (RunFlag) {
        c = _getch();
//        WaitForSingleObject(hWriteMutex, INFINITE);
        ret = WriteFile(hCom, &c, 1, &len, NULL);
        if (ret == FALSE)
        {
            printf_s("Fail to Written\n");
        }

    }

}








int main(void)
{
    int     ThreadNr = 0;                    // Number of threads started
    DWORD dwEventMask;
    int ret;
    if (BSerialInit() == FALSE) {
        printf_s("Serial Port initialization failed, exit!\n");
        return -1;
    }

    _beginthread(ReadProc, 0, &ThreadNr);
    _beginthread(WriteProc, 0, &ThreadNr);

    while (RunFlag) {
    }
    system("pause");
    return 0;
}




