#include "pch.h"

#include "functions.h"
#include "values.h"



void logMessage(string message, string fileName) {// без .txt
    sync::mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm time_info;
    localtime_s(&time_info, &now_time);


    ofstream logOfs("..\\" + fileName + ".txt", ios::app);
    logOfs << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
    logOfs.close();
    sync::mtxForLog.unlock();

    if (fileName == "system")
        cout << "(" + fileName + ") " << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << message << '\n';
}

void logMessage(string message, string fileName, int num) {// без .txt
    sync::mtxForLog.lock();
    time_t now_time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    struct tm time_info;
    localtime_s(&time_info, &now_time);


    ofstream logOfs("..\\" + fileName + ".txt", ios::app);
    logOfs << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] ") << to_string(num) + ")\t" + message << '\n';
    logOfs.close();
    sync::mtxForLog.unlock();

    if (fileName != "messages")
        cout << "(" + fileName + ") " << put_time(&time_info, "[%Y-%m-%d | %H:%M:%S] (") << to_string(num) + ")\t" + message << '\n';
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


int findGroup(string groupName) {
    auto group = find(rb::Groups.begin(), rb::Groups.end(), groupName);

    if (group == rb::Groups.end())
        return -1;
    else
        return group - rb::Groups.begin();
}

void genGroups() {
    const vector<string> gName = { "ДО", "ИИС", "ИКС", "ИСП", "МТО", "ОИБ", "ОПС", "СИС", "ТО", "ЭСС" };
    const vector<string> suf = { "а", "ир", "ис", "п", "р", "т" };
    int offset = 0;

    //получение года
    {
        time_t t = time(nullptr);
        tm now = {};
        localtime_s(&now, &t);

        sync::CurrentYear = (now.tm_year + 1900) % 100 - 1;

        if (now.tm_mon > 6 || (now.tm_mon == 6 && now.tm_mday > 4))
            sync::CurrentYear += 1;

        offset = (sync::CurrentYear - 24) % 4;
    }


    for (auto& name : gName) {
        bool isNormal = name != "ИСП";

        int i = isNormal ? 5 : 0;

        for (; i < 6; i++) {
            for (int j = 0; j < 4; j++) {

                rb::Groups.push_back(std::format("{}-{}{}{}", name, (j + offset) % 4 + 1, sync::CurrentYear - (j + offset) % 4, isNormal ? "" : suf[i]));
            }
        }
    }
}