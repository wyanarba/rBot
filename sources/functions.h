#pragma once
//204

#pragma comment(lib, "wininet.lib")

#include <tgbot/tgbot.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <set>
#include <map>
#include <windows.h>
#include <wininet.h>
#include <opencv2/opencv.hpp>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <filesystem>
#include <mutex>
#include <regex>
#include <numeric>

using namespace std;
using namespace TgBot;
using namespace chrono;
namespace fs = std::filesystem;
using namespace cv;
using namespace poppler;


struct myGroup
{
    bool isExists = 0;//наличие в папке
    bool isExists2 = 0;//реальное наличие в папке
    int32_t messageId = 0;//id сообщения
    int32_t messageIdS = 0;//id сообщения с мини расписанием

    myGroup(const bool& isExists) : isExists(isExists) {}

    myGroup& operator=(bool value) {
        isExists = value;
        return *this;
    }
};

struct myCoord
{
    float x, y;
    string name;

    myCoord(const float& x, const float& y, const string& name) : x(x), y(y), name(name) {}
};

vector<string> Groups = { "ДО-124", "ДО-223", "ДО-322", "ДО-421", "ИИС-124", "ИИС-223", "ИИС-322", "ИИС-421", "ИКС-124", "ИКС-223", "ИКС-322", "ИКС-421", "ИСП-124а", "ИСП-124ир", "ИСП-124ис", "ИСП-124п", "ИСП-124р", "ИСП-124т", "ИСП-223а", "ИСП-223ир", "ИСП-223ис", "ИСП-223п", "ИСП-223р", "ИСП-223т", "ИСП-322а", "ИСП-322ир", "ИСП-322ис", "ИСП-322п", "ИСП-322р", "ИСП-322т", "ИСП-421а", "ИСП-421ир", "ИСП-421ис", "ИСП-421п", "ИСП-421р", "ИСП-421т", "ОИБ-124", "ОИБ-223", "ОИБ-322", "ОИБ-421", "ОПС-124", "ОПС-223", "ОПС-322", "ОПС-421", "СИС-124", "СИС-223", "СИС-322", "СИС-421", "ТО-124", "ТО-223", "ТО-322", "ТО-421", "ЭСС-124", "ЭСС-223", "ЭСС-322", "ЭСС-421", "МТО-124", "МТО-223", "МТО-322", "МТО-421", "NOPE"};
vector<string> Groups1251;


vector<myGroup> GroupsB[4];
vector<bool> AltGroupsB[2], DisabledGroups;//изменённые группы, отключённые группы
set <string> Teachers[4];
set <string> DefTeachers;
vector<int> edgeGroups[2];//края групп для 2 режима рассылки

bool IsNewRaspis[2];
int Mode, ModeS, ModeVs;//режимы
string ModeStr, AltModeStr;//режимы
bool ErrorOnCore = 0;
int DisabledGroupsC = 0;

mutex mtx1, mtxForLog;//для синхронизации проверки режимов
int8_t syncMode = 0;//переменная для синхронизации, 0 - свободный режим, 1 - ожидание остановки потока бота, 2 - подтверждение остановки потока бота, 3 - отправка расписания

//создание логов
void logMessage(string message, string fileName) {// без .txt
    mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm time_info;
    localtime_s(&time_info, &now_time);


    ofstream logOfs("..\\" + fileName + ".txt", ios::app);
    logOfs << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
    logOfs.close();
    mtxForLog.unlock();

    if(fileName == "system")
        cout << "(" + fileName + ") " << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
}

void logMessage(string message, string fileName, int num) {// без .txt
    mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm time_info;
    localtime_s(&time_info, &now_time);


    ofstream logOfs("..\\" + fileName + ".txt", ios::app);
    logOfs << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << to_string(num) + ")\t" + message << '\n';
    logOfs.close();
    mtxForLog.unlock();

    if (fileName != "messages")
        cout << "(" + fileName + ") " << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] (") << to_string(num) + ")\t" + message << '\n';
}

//скачивание файла с сайта
bool DownloadFileToMemory(const std::string& url, std::string& fileContent) {
    HINTERNET hInternet = InternetOpen("File Downloader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        logMessage("Failed to initialize InternetOpen.", "system", 201);
        return false;
    }

    // Генерация уникального параметра для предотвращения кеширования
    std::stringstream ss;
    ss << url << "?t=" << std::time(nullptr);  // Добавляем текущее время к URL
    std::string fullUrl = ss.str();

    // Открываем URL с уникальным параметром
    HINTERNET hFile = InternetOpenUrl(hInternet, fullUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hFile) {
        logMessage("Failed to open URL.", "system", 202);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::ostringstream contentStream;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        contentStream.write(buffer, bytesRead);
    }

    fileContent = contentStream.str();

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    return true;
}

//запись строк (файлов .pdf) в файлы
bool WriteStringToFile(const std::string& content, const std::string& filePath) {
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        logMessage("Failed to open file for writing.", "system", 203);
        return false;
    }
    outFile.write(content.data(), content.size());
    outFile.close();
    return true;
}

//чтение строк (файлов .pdf) из файлов
bool ReadStringFromFile(const std::string& filePath, std::string& content) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) {
        logMessage("Failed to open file for reading.", "system", 204);
        return false;
    }
    std::ostringstream contentStream;
    contentStream << inFile.rdbuf();
    content = contentStream.str();
    inFile.close();
    return true;
}

//получение количества страниц в документе
int getPDFPageCount(const std::string& filePath) {
    // Загружаем документ
    poppler::document* doc = poppler::document::load_from_file(filePath);

    if (!doc) {
        //errorExcept("Не удалось открыть PDF файл: " + filePath, 0);
        return -1;
    }

    // Получаем количество страниц
    int numPages = doc->pages();

    // Освобождаем память
    delete doc;

    return numPages;
}

//кодировки
string Utf8_to_cp1251(const char* str)
{
    string res;
    int result_u, result_c;


    result_u = MultiByteToWideChar(CP_UTF8,
        0,
        str,
        -1,
        0,
        0);

    if (!result_u)
        return 0;

    wchar_t* ures = new wchar_t[result_u];

    if (!MultiByteToWideChar(CP_UTF8,
        0,
        str,
        -1,
        ures,
        result_u))
    {
        delete[] ures;
        return 0;
    }


    result_c = WideCharToMultiByte(
        1251,
        0,
        ures,
        -1,
        0,
        0,
        0, 0);

    if (!result_c)
    {
        delete[] ures;
        return 0;
    }

    char* cres = new char[result_c];

    if (!WideCharToMultiByte(
        1251,
        0,
        ures,
        -1,
        cres,
        result_c,
        0, 0))
    {
        delete[] cres;
        return 0;
    }
    delete[] ures;
    res.append(cres);
    delete[] cres;
    return res;
}

//кодировки
string cp1251_to_utf8(const char* str) {
    string res;
    int result_u, result_c;
    result_u = MultiByteToWideChar(1251, 0, str, -1, 0, 0);
    if (!result_u) { return 0; }
    wchar_t* ures = new wchar_t[result_u];
    if (!MultiByteToWideChar(1251, 0, str, -1, ures, result_u)) {
        delete[] ures;
        return 0;
    }
    result_c = WideCharToMultiByte(65001, 0, ures, -1, 0, 0, 0, 0);
    if (!result_c) {
        delete[] ures;
        return 0;
    }
    char* cres = new char[result_c];
    if (!WideCharToMultiByte(65001, 0, ures, -1, cres, result_c, 0, 0)) {
        delete[] cres;
        return 0;
    }
    delete[] ures;
    res.append(cres);
    delete[] cres;
    return res;
}

bool isAlreadyRunning() {
    // Уникальное имя мьютекса
    const char* mutexName = "Global\\raspisbot";

    // Создаем мьютекс
    HANDLE hMutex = CreateMutexA(nullptr, FALSE, mutexName);
    if (hMutex == nullptr) {
        std::cerr << "Ошибка при создании мьютекса. Код ошибки: " << GetLastError() << std::endl;
        return true; // Если не удалось создать мьютекс, считаем, что программа уже запущена
    }

    // Проверяем, существует ли уже мьютекс
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex); // Закрываем дескриптор
        return true; // Программа уже запущена
    }

    return false; // Программа запущена впервые
}
