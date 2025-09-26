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
#include <freetype/freetype.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <filesystem>
#include <mutex>
#include <regex>
#include <numeric>
#include <queue>

using namespace std;
using namespace TgBot;
using namespace chrono;
namespace fs = std::filesystem;
using namespace cv;
using namespace poppler;


struct myCoord
{
    float x, y;
    string name;

    myCoord(const float& x, const float& y, const string& name) : x(x), y(y), name(name) {}
};

vector<string> Groups;
vector<string> Groups1251;
int  CurrentYear = 0;

//общие переменные расписания
namespace rb {
    const int countBuilds = 2;// кол-во корпусов
    const int pagesInBui = 10;// макс страниц в корпусе
    const string imgPath = "rImages\\";// путь до папок с расписанием

    bool ErrorOnCore = 0;// верно ли обработано расписание
    int currentCorps = 0;//на какой корпус рассылать расписание
    mutex mtx1, mtxForLog;//для синхронизации проверки режимов
    int8_t syncMode = 0;
    //переменная для синхронизации, 0 - свободный режим, 1 - ожидание остановки потока бота,
    // 2 - подтверждение остановки потока бота, 3 - отправка расписания

    set <string> AllTeachers;// перечисление всех учителей (DefTeachers)
    vector<bool> DisabledGroups;// группы без подписчиков

    bool mLogIsEnabled = 1;
    queue<string> callsLogQueue;
}

struct myGroup
{
    bool isExists = 0;//наличие в папке
    bool changed = 0;// изменилось ли расписание группы (не новый файл)
    int idSpam;// номер буферной группы
    int32_t messageId = 0;//id сообщения
    int32_t messageIdS = 0;//id сообщения с мини расписанием

    string ps, psS;// tg fileId

    myGroup(const bool& isExists) : isExists(isExists) {}

    myGroup& operator=(bool value) {
        isExists = value;
        return *this;
    }
};

// страница расписания
struct pageRasp {
    string folderName;// имя папки страницы
    bool IsNewPage = 0;// новая страница или обновление старой
    bool isEmpty = 1;// пуста ли папка
    int32_t mi = 0;//id сообщения
    string ps;// tg fileId

    vector<myGroup>groups;// группы
    set <string> Teachers;// преподаватели

    pageRasp(string folderName) {
        this->folderName = folderName;
        groups.assign(Groups.size(), 0);
    }

    void clear() {
        Teachers.clear();
        groups.clear();
        groups.assign(Groups.size(), 0);
        isEmpty = 1;
        IsNewPage = 0;
    }
};

//корпус
struct corps
{
    string pdfFileName;// имя файла пдф
    int localOffset;// смещение этого корпуса
    string LastFileD;// файл pdf из локальной папки

    vector<pageRasp> pages;
    int pagesUse = 0;// сколько сейчас используется страниц

    corps(string pdfFileName, int localOffset) {
        this->localOffset = localOffset;// порядковый номер корпуса
        this->pdfFileName = pdfFileName;

        for (int i = 0; i < rb::pagesInBui; i++) {
            pages.push_back(pageRasp(to_string(i + localOffset * rb::pagesInBui)));
        }
    }
};

namespace rb {
    vector<corps> corpss; // ну типа корпусы
}

//DisabledGroups 0 - выключеная

//создание логов
void logMessage(string message, string fileName) {// без .txt
    rb::mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm time_info;
    localtime_s(&time_info, &now_time);


    ofstream logOfs("..\\" + fileName + ".txt", ios::app);
    logOfs << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
    logOfs.close();
    rb::mtxForLog.unlock();

    if(fileName == "system")
        cout << "(" + fileName + ") " << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
}

void logMessage(string message, string fileName, int num) {// без .txt
    rb::mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm time_info;
    localtime_s(&time_info, &now_time);


    ofstream logOfs("..\\" + fileName + ".txt", ios::app);
    logOfs << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << to_string(num) + ")\t" + message << '\n';
    logOfs.close();
    rb::mtxForLog.unlock();

    if (fileName != "messages")
        cout << "(" + fileName + ") " << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] (") << to_string(num) + ")\t" + message << '\n';
}

void logCall(string message) {
    rb::mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm now;
    localtime_s(&now, &now_time);


    if (rb::mLogIsEnabled) {
        ofstream logOfs("..\\mLog.txt", ios::app);
        logOfs << put_time(&now, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
        logOfs.close();
    }

    rb::callsLogQueue.push("[" + to_string(now.tm_year + 1900) + "-" + to_string(now.tm_mon) + "-" + to_string(now.tm_mday) + " | " +
        to_string(now.tm_hour) + ":" + to_string(now.tm_min) + ":" + to_string(now.tm_sec) + "] " + message);

    if (rb::callsLogQueue.size() > 50)
        rb::callsLogQueue.pop();

    rb::mtxForLog.unlock();
}

//скачивание файла с сайта
//скачивание файла с сайта с улучшенной обработкой ошибок
bool DownloadFileToMemory(const std::string& url, std::string& fileContent) {
    HINTERNET hInternet = InternetOpen("File Downloader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        logMessage("Failed to initialize InternetOpen.", "system", 201);
        return false;
    }

    // Генерация уникального параметра для предотвращения кеширования
    std::stringstream ss;
    ss << url << "?t=" << std::time(nullptr);
    std::string fullUrl = ss.str();

    // Открываем URL с флагами для бинарных данных
    HINTERNET hFile = InternetOpenUrl(hInternet, fullUrl.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_EXISTING_CONNECT, 0);

    if (!hFile) {
        DWORD error = GetLastError();
        logMessage("Failed to open URL. Error code: " + std::to_string(error), "system", 202);
        InternetCloseHandle(hInternet);
        return false;
    }

    // Проверяем HTTP статус код
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (HttpQueryInfo(hFile, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &statusCode, &statusCodeSize, NULL)) {
        if (statusCode != 200) {
            logMessage("HTTP error. Status code: " + std::to_string(statusCode), "system", 204);
            InternetCloseHandle(hFile);
            InternetCloseHandle(hInternet);
            return false;
        }
    }

    // Получаем размер файла если доступен
    DWORD contentLength = 0;
    DWORD contentLengthSize = sizeof(contentLength);
    bool hasContentLength = HttpQueryInfo(hFile, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
        &contentLength, &contentLengthSize, NULL);

    if (hasContentLength) {
        //logMessage("Content-Length: " + std::to_string(contentLength), "system");
        fileContent.reserve(contentLength); // резервируем память
    }

    // Читаем файл блоками
    std::ostringstream contentStream;
    char buffer[8192]; // увеличили размер буфера
    DWORD bytesRead;
    DWORD totalBytesRead = 0;

    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead)) {
        if (bytesRead == 0) break; // конец файла

        contentStream.write(buffer, bytesRead);
        totalBytesRead += bytesRead;
    }

    fileContent = contentStream.str();

    //logMessage("Downloaded " + std::to_string(totalBytesRead) + " bytes", "system");

    // Проверяем, что скачали ожидаемое количество данных
    if (hasContentLength && totalBytesRead != contentLength) {
        logMessage("Warning: Downloaded " + std::to_string(totalBytesRead) +
            " bytes, expected " + std::to_string(contentLength), "system");
    }

    // Проверяем, что файл не пустой и начинается с PDF сигнатуры
    if (fileContent.size() < 4 || fileContent.substr(0, 4) != "%PDF") {
        logMessage("Downloaded file is not a valid PDF (size: " +
            std::to_string(fileContent.size()) + ")", "system", 205);
        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);
        return false;
    }

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);
    return true;
}

//запись строк (файлов .pdf) в файлы
//запись бинарных данных в файл с проверками
bool WriteStringToFile(const std::string& content, const std::string& filePath) {
    // Проверяем, что контент не пустой
    if (content.empty()) {
        logMessage("Content is empty, cannot write file.", "system", 206);
        return false;
    }

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        logMessage("Failed to open file for writing: " + filePath, "system", 203);
        return false;
    }

    outFile.write(content.data(), content.size());

    // Проверяем успешность записи
    if (outFile.fail()) {
        logMessage("Failed to write data to file: " + filePath, "system", 207);
        outFile.close();
        return false;
    }

    outFile.close();

    // Проверяем размер записанного файла
    std::ifstream checkFile(filePath, std::ios::binary | std::ios::ate);
    if (checkFile.is_open()) {
        std::streamsize fileSize = checkFile.tellg();
        checkFile.close();

        if (static_cast<size_t>(fileSize) != content.size()) {
            logMessage("File size mismatch. Expected: " + std::to_string(content.size()) +
                ", Actual: " + std::to_string(fileSize), "system", 208);
            return false;
        }

        logMessage("Successfully wrote " + std::to_string(fileSize) + " bytes to " + filePath, "system");
    }

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
//получение количества страниц в документе с дополнительными проверками
int getPDFPageCount(const std::string& filePath) {
    // Проверяем существование файла
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        logMessage("File does not exist: " + filePath, "system");
        return -1;
    }

    // Проверяем размер файла
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.close();

    if (fileSize < 10) { // минимальный размер PDF
        logMessage("File too small to be a valid PDF: " + std::to_string(fileSize) + " bytes", "system");
        return -1;
    }

    logMessage("Attempting to open PDF file: " + filePath + " (size: " + std::to_string(fileSize) + " bytes)", "system");

    // Загружаем документ
    poppler::document* doc = poppler::document::load_from_file(filePath);
    if (!doc) {
        logMessage("Failed to open PDF file with poppler: " + filePath, "system");

        // Дополнительная проверка - читаем начало файла
        std::ifstream pdfFile(filePath, std::ios::binary);
        if (pdfFile.is_open()) {
            char header[10];
            pdfFile.read(header, 5);
            header[5] = '\0';
            logMessage("File header: " + std::string(header), "system");
            pdfFile.close();
        }

        return -1;
    }

    // Получаем количество страниц
    int numPages = doc->pages();
    logMessage("PDF contains " + std::to_string(numPages) + " pages", "system");

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

//анти двойной запуск
bool isAlreadyRunning() {

    logCall(std::format("isAlreadyRunning()"));

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

int findGroup(string groupName) {
    auto group = find(Groups.begin(), Groups.end(), groupName);

    if (group == Groups.end())
        return -1;
    else
        return group - Groups.begin();
}

void drawTextFT(cv::Mat& img, const std::string& aa, const std::string& fontPath, int fontSize, int x_center, int y_center) {
    // Конвертация из Windows-1251 в wstring
    int size_needed = MultiByteToWideChar(1251, 0, aa.c_str(), -1, nullptr, 0);
    std::wstring text(size_needed, 0);
    MultiByteToWideChar(1251, 0, aa.c_str(), -1, &text[0], size_needed);
    text.pop_back(); // удаляем null-terminator

    // Инициализация FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "FT_Init_FreeType failed\n";
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "FT_New_Face failed\n";
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Предварительный расчёт общей ширины и максимальной высоты текста
    int text_width = 0;
    int text_height = 0;
    int max_top = 0;
    int max_bottom = 0;

    for (wchar_t wc : text) {
        if (FT_Load_Char(face, wc, FT_LOAD_RENDER)) continue;

        text_width += face->glyph->advance.x >> 6;

        int top = face->glyph->bitmap_top;
        int bottom = face->glyph->bitmap.rows - face->glyph->bitmap_top;

        if (top > max_top) max_top = top;
        if (bottom > max_bottom) max_bottom = bottom;
    }

    text_height = max_top + max_bottom;

    // Начальные координаты для центровки
    int pen_x = x_center - text_width / 2;
    int pen_y = y_center + max_top / 2;

    // Отрисовка
    for (wchar_t wc : text) {
        if (FT_Load_Char(face, wc, FT_LOAD_RENDER)) {
            std::wcerr << L"Failed to load char: " << wc << L"\n";
            continue;
        }

        FT_GlyphSlot g = face->glyph;
        int w = g->bitmap.width;
        int h = g->bitmap.rows;
        int top = g->bitmap_top;
        int left = g->bitmap_left;

        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                int x_img = pen_x + left + col;
                int y_img = pen_y - top + row;
                if (x_img >= 0 && y_img >= 0 && x_img < img.cols && y_img < img.rows) {
                    uchar alpha = g->bitmap.buffer[row * g->bitmap.pitch + col];
                    if (alpha > 0) {
                        cv::Vec3b& pixel = img.at<cv::Vec3b>(y_img, x_img);
                        for (int c = 0; c < 3; ++c) {
                            pixel[c] = static_cast<uchar>(
                                pixel[c] * (255 - alpha) / 255
                                );
                        }
                    }
                }
            }
        }

        pen_x += g->advance.x >> 6;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void genGroups() {

    logCall(std::format("genGroups()"));

    const vector<string> gName = { "ДО", "ИИС", "ИКС", "ИСП", "МТО", "ОИБ", "ОПС", "СИС", "ТО", "ЭСС" };
    const vector<string> suf = { "а", "ир", "ис", "п", "р", "т" };
    int offset = 0;

    //получение года
    {
        time_t t = time(nullptr);
        tm now = {};
        localtime_s(&now, &t);

        CurrentYear = (now.tm_year + 1900) % 100 - 1;

        if (now.tm_mon > 6 || (now.tm_mon == 6 && now.tm_mday > 4))
            CurrentYear += 1;

        offset = (CurrentYear - 24) % 4;
    }


    for (auto& name : gName) {
        bool isNormal = name != "ИСП";

        int i = isNormal ? 5 : 0;

        for (; i < 6; i++) {
            for (int j = 0; j < 4; j++) {

                Groups.push_back(std::format("{}-{}{}{}", name, (j + offset) % 4 + 1, CurrentYear - (j + offset) % 4, isNormal ? "" : suf[i]));
            }
        }
    }
}