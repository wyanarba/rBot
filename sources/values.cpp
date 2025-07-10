#include "pch.h"
#include "values.h"


namespace sync {
    int8_t SyncMode = 0;
    //переменная для синхронизации, 0 - свободный режим, 1 - ожидание остановки потока бота,
    // 2 - подтверждение остановки потока бота, 3 - отправка расписания

    mutex mtx1, mtxForLog;//да)


    bool IsUpdate = 0;
    bool IsChangeYear = 0;

    int AttemptsToCheck = 0;// 1 10
    int AttemptsToCheck2 = 0;// 1 30

    string NewVersion;
    int  CurrentYear = 0;
}

namespace cfg {
    bool ModeSend = 0;//режим отправки 0 - v1, 1 - v2
    void (*update)();//функция для отправки расписания, (указатель) на неё

    vector<int64_t> GroupsForSpam;//буферные группы для рассылки

    int64_t RootTgId = 0;//тг id владельца
    int64_t SecondRootTgId = 6266601544;//мой тг id, для прав чуть по ниже

    string BotKey = "";//ключ бота
    string StartText = "";//приветственное сообщение

    const DWORD SleepTime = 60000;
    bool EnableAd = 1;
    bool EnableAutoUpdate = 1;
}

namespace rb {
    const int countBuilds = 2;// кол-во корпусов
    const int pagesInBui = 10;// макс страниц в корпусе
    const string imgPath = "rImages\\";// путь до папок с расписанием

    bool ErrorOnCore = 0;// верно ли обработано расписание
    int currentCorps = 0;//на какой корпус рассылать расписание


    set <string> AllTeachers;// перечисление всех учителей
    set <string>SpamText;// исключённые слова, как фамилии преподавателей (скорее всего устарело)
    vector<bool> DisabledGroups;// группы без подписчиков

    vector<string> Groups;
    vector<string> Groups1251;
    vector<corps> corpss; // ну типа корпуса
}