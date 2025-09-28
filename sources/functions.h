#pragma once
using namespace std;


// Создание логов
void logMessage(string message, string fileName);

void logMessage(string message, string fileName, int num);


// Кодировки
string Utf8_to_cp1251(const char* str);

string cp1251_to_utf8(const char* str);


// Работа с группами
int findGroup(string groupName);

void genGroups();