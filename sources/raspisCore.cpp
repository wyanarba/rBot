#include <freetype/freetype.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <opencv2/opencv.hpp>

#include "pch.h"

#include "raspisCore.h"
#include "functions.h"
#include "values.h"



using namespace cv;
using namespace poppler;
using json = nlohmann::json;


struct myCoord
{
    int x, y;
    string text;
};

struct MyTextBox
{
    int x, y, x1, y1, midX, midY;
    int charWidth;
    string text, text1251;
};

struct Cell
{
    map<char, int> chars;

    bool changedEarlier = 0, changedNow = 0;

    int x, y, x1, y1;

    int indexInX, indexInY;
};

struct Table {

    string date;

    // Строки, колонны
    vector<vector<Cell>> cells;
};


static string MainUrl = "https://rasp.vksit.ru/";
static const char UpdateCommand[17] = "start update.bat";
static string FileDownloaded;//файлы .pdf с расписанием
static int CutsOffX = 0, CutsOffY = 0, LeftEdge = 0;

void to_json(json& j, const Cell& cell) {
    j = json{ {"chars", cell.chars}, {"changedEarlier", cell.changedEarlier} };
}

void from_json(const json& j, Cell& cell) {
    j.at("chars").get_to(cell.chars);
    j.at("changedEarlier").get_to(cell.changedEarlier);
}

void to_json(json& j, const Table& table) {
    j = json{
        {"date", table.date},
        {"cells", table.cells}
    };
}

void from_json(const json& j, Table& table) {
    j.at("date").get_to(table.date);
    j.at("cells").get_to(table.cells);
}

// Скачивание файла с сайта с улучшенной обработкой ошибок
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
    /*if (fileContent.size() < 4 || fileContent.substr(0, 4) != "%PDF") {
        logMessage("Downloaded file is not a valid PDF (size: " +
            std::to_string(fileContent.size()) + ")", "system", 205);
        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);
        return false;
    }*/

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);
    return true;
}

// Запись бинарных данных в файл с проверками
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

        //logMessage("Successfully wrote " + std::to_string(fileSize) + " bytes to " + filePath, "system");
    }

    return true;
}

// Чтение строк (файлов .pdf) из файлов
static bool ReadStringFromFile(const std::string& filePath, std::string& content) {
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

// Получение количества страниц в документе с дополнительными проверками
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

    //logMessage("Attempting to open PDF file: " + filePath + " (size: " + std::to_string(fileSize) + " bytes)", "system");

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

// Рисование текста
static void drawTextFT(cv::Mat& img, const std::string& aa, const std::string& fontPath, int fontSize, int x_center, int y_center) {
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

static int findMajorityElement(const vector<int>& numbers) {
    map<int, int> frequency;
    for (int i = 0; i < numbers.size() - 1; i++) {
        frequency[numbers[i + 1] - numbers[i]]++;
    }

    int majorityElement = numbers[0];
    int maxFrequency = 0;
    for (const auto& entry : frequency) {
        if (entry.second > maxFrequency) {
            maxFrequency = entry.second;
            majorityElement = entry.first;
        }
    }
    return majorityElement;
}

static std::vector<int> filterVector(const std::vector<int>& input, double thresholdFactor = 1) {

    // Подсчёт частоты каждого числа
    std::map<int, int> frequency;
    for (int num : input) {
        frequency[num]++;
    }

    // Вычисление среднего количества повторений
    double averageFrequency = std::accumulate(frequency.begin(), frequency.end(), 0.0,
        [](double sum, const std::pair<int, int>& pair) {
            return sum + pair.second;
        }) / frequency.size();

    // Рассчитаем порог для фильтрации
    double threshold = averageFrequency * thresholdFactor;

    // Формируем новый вектор: удаляем редкие значения и оставляем уникальные
    std::vector<int> result;
    for (const auto& [value, count] : frequency) {
        if (count >= threshold) {
            result.push_back(value);
        }
    }

    return result;
}



static void editRaspis(string filePath) {
    Mat image = imread(filePath);;
    int& imageHeight = image.rows, imageWidth = image.cols;
    int startX = imageWidth + 1, startY = imageHeight + 1, endX = 0, endY = 0;
    Vec3b pixel;
    int margin, marginTop;//отступ по краям
    int wLine = 50;//ширина левой линии

    //поиск startX
    for (int i = 0; i < imageHeight; i++) {// x - j, y - i
        for (int j = 0; j < imageWidth; j++) {

            if (startX > j) {
                pixel = image.at<Vec3b>(i, j); // получение цвета пикселя

                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    startX = j;
                    wLine = 50;
                    break;
                }
            }
            else
                break;
        }

        if (startX < imageWidth) {
            pixel = image.at<Vec3b>(i, startX);

            if (pixel[0] < 50) {
                for (int k = 0; k < wLine; k++) {

                    pixel = image.at<Vec3b>(i, startX + k); // получение цвета пикселя

                    if (pixel[0] > 50) {
                        wLine = k;
                    }
                }
            }
        }
    }

    //поиск startY
    for (int i = 0; i < imageWidth; i++) {// x - i, y - j
        for (int j = 0; j < imageHeight; j++) {
            if (startY > j) {
                pixel = image.at<Vec3b>(j, i); // получение цвета пикселя
                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    startY = j;
                    break;
                }
            }
            else
                break;
        }
    }

    //поиск endX
    for (int y = imageHeight - 1; y > 0; y--) {// x - j, y - i
        for (int x = imageWidth - 1; x > 0; x--) {
            if (x > endX) {
                pixel = image.at<Vec3b>(y, x); // получение цвета пикселя
                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    endX = x;
                    break;
                }
            }
            else
                break;
        }
    }

    //поиск endY
    for (int i = imageWidth - 1; i > 0; i--) {// x - i, y - j
        for (int j = imageHeight - 1; j > 0; j--) {
            if (endY < j) {
                pixel = image.at<Vec3b>(j, i); // получение цвета пикселя
                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    endY = j;
                    break;
                }
            }
            else
                break;
        }
    }

    if (wLine < 3 || wLine > 40)
        wLine = 7;

    margin = wLine * 1.7;
    marginTop = margin * 2;


    cv::Rect roi(startX - margin, startY - marginTop, endX - startX + 2 * margin, endY - startY + margin + marginTop); // x, y, width, height
    cv::imwrite(filePath, image(roi)); // Сохранение изображения

    CutsOffX = startX - margin;
    CutsOffY = startY - marginTop;
    LeftEdge = margin;
}

static void getLocalRaspis(pageRasp& mPage, string pdf_path, int pageNum) {

    // Вроде как это всё более лаконично сделано, чем раньше, но отображение изменений меня очень сильно разочаровало...
    string imageName = rb::imgPath + mPage.folderName + ".png", folderToSave = rb::imgPath + mPage.folderName + "\\";
    Mat image, imageCoper, imageCoper2;//картинки
    vector <int> xDots[4], yDots[4], extremDotsI;// 0 - верхние левые, 1 - нижние правые, 2 - не сортированные точки, 3 - временные
    image = imread(imageName); // загрузка изображения


    //поиск точек пересечения линий таблицы
    {
        int radCheck = 0, radMerge = 5;//радиусы для поиска перекрёстных точек
        int xOff = 0, y0 = 0, y1 = 0;//смещения и кол-во точек(для ключевого y)

        //поиск шапки таблицы (она обычно не меняется)
        for (int y = 0; y < image.rows; y++) {
            if (image.at<Vec3b>(y, LeftEdge)[0] < 100) {//чёрный пиксель

                //проверка линии по x
                bool b1 = 1;

                while (b1 && xOff < LeftEdge * 3) {
                    xOff++;
                    b1 = image.at<Vec3b>(y, LeftEdge + xOff)[0] < 100;
                }

                if (xOff == LeftEdge * 3) {
                    if (yDots[2].size() > 0 && y - yDots[2][yDots[2].size() - 1] > radMerge)
                        y1++;

                    if (y1 == 2) {
                        y1 = y;
                        y = image.rows;
                    }

                    yDots[2].push_back(y);
                }

                xOff = 0;
            }
        }

        y0 = yDots[2][0];
        radCheck = y1 - y0;
        yDots[2].clear();


        //сам поиск
        for (int x = LeftEdge; x < image.cols; x++) {
            bool isNormalLine = 1;

            for (int y = y0; y < y1; y++) {//проверка на нормальную линию
                isNormalLine = image.at<Vec3b>(y, x)[0] < 100;
                if (!isNormalLine) {
                    y = y1;
                }
            }

            if (!isNormalLine)
                continue;

            for (int y = y1; y < image.rows; y++) {

                if (image.at<Vec3b>(y, x)[0] < 100) {//чёрный пиксель

                    bool b1 = 1, b2 = 1;

                    while ((b1 || b2) && xOff < radCheck) {//проверка по x
                        xOff++;

                        if (b1) {
                            b1 = image.at<Vec3b>(y, x + xOff)[0] < 100;
                        }

                        if (b2) {
                            b2 = image.at<Vec3b>(y, x - xOff)[0] < 100;
                        }
                    }

                    if (xOff == radCheck) {
                        xDots[2].push_back(x);
                        yDots[2].push_back(y);
                        //imageCoper.at<Vec3b>(y, x) = cv::Vec3b(0, 255, 0);
                    }

                    xOff = 0;
                }
            }
        }

        //фильтрация результата
        sort(xDots[2].begin(), xDots[2].end());
        sort(yDots[2].begin(), yDots[2].end());


        for (int i = 0; i < xDots[2].size() - 1; i++) {
            xDots[3].push_back(xDots[2][i]);

            if (xDots[0].size() == 0)
                xDots[0].push_back(xDots[2][i]);

            else if (xDots[2][i] - xDots[2][i - 1] > radMerge) {//первая
                xDots[0].push_back(xDots[2][i]);
            }

            if (xDots[2][i + 1] - xDots[2][i] > radMerge) {//последняя
                xDots[3] = filterVector(xDots[3], 0.7);//70% отличие от среднего

                if (xDots[3][0] != xDots[0][xDots[0].size() - 1]) {
                    xDots[0].pop_back();
                    xDots[0].push_back(xDots[3][0]);
                }

                xDots[1].push_back(xDots[3][xDots[3].size() - 1]);
                xDots[3].clear();
            }

        }

        for (int i = 0; i < yDots[2].size() - 1; i++) {
            yDots[3].push_back(yDots[2][i]);

            if (yDots[0].size() == 0)
                yDots[0].push_back(yDots[2][i]);

            else if (yDots[2][i] - yDots[2][i - 1] > radMerge) {
                yDots[0].push_back(yDots[2][i]);
            }

            if (yDots[2][i + 1] - yDots[2][i] > radMerge) {
                yDots[3] = filterVector(yDots[3], 0.7);//70% отличие от среднего

                if (yDots[3][0] != yDots[0][yDots[0].size() - 1]) {
                    yDots[0].pop_back();
                    yDots[0].push_back(yDots[3][0]);
                }

                yDots[1].push_back(yDots[3][yDots[3].size() - 1]);
                yDots[3].clear();
            }

        }

        xDots[1].push_back(xDots[2][xDots[2].size() - 1]);
        yDots[1].push_back(yDots[2][yDots[2].size() - 1]);


        // Тестовая отрисока
        /*for (int i = 0; i < xDots[0].size(); i++) {
            for (int x = xDots[0][i]; x <= xDots[1][i]; x++) {
                for (int j = 0; j < yDots[0].size(); j++) {
                    for (int y = yDots[0][j]; y <= yDots[1][j]; y++) {
                        imageCoper.at<Vec3b>(y, x) = cv::Vec3b(0, 0, 255);
                    }
                }
            }
        }*/

    }

    //костыльчик
    {}

    //поиск угловых точек пересечений
    {
        int majority = findMajorityElement(yDots[0]);
        double threshold = majority * 0.8;  // Порог, на 20% меньше большинство

        for (int i = 0; i < yDots[0].size() - 1; i++) {
            if (yDots[0][i + 1] - yDots[0][i] < threshold) {
                extremDotsI.push_back(i);
            }
        }

        extremDotsI.push_back(yDots[0].size() - 1);
    }

    Table table;
    const int partCount = 3;
    cv::Scalar colors[partCount] = { {0, 0, 255}, {0, 255, 0}, {255, 0, 255} };

    // Создание ячеек
    {
        for (int i = 0; i < xDots[0].size() - 1; i++) {

            for (int j = 0; j < yDots[0].size() - 1; j++) {

                Cell cell;


                // Добавление строк
                if (i == 0) {
                    table.cells.push_back({});
                }

                int part_size = (xDots[1][i + 1] - xDots[0][i]) / partCount;

                for (int k = 0; k < partCount; k++) {

                    cell = {
                        .x = xDots[0][i] + part_size * k,
                        .y = yDots[0][j],
                        .x1 = xDots[0][i] + part_size * (k + 1),
                        .y1 = yDots[1][j + 1],

                        //.indexInX = i,
                        //.indexInY = j
                    };



                    table.cells[j].push_back(cell);
                }
            }
        }
    }

    vector <myCoord> t2;// Инициалы преподавателей
    vector <MyTextBox> textBoxes;
    int date = -1; // Текст бокс с датой

    // Разделение текста на ячейки и начальный поиск
    {
        smatch match;// Переменная для результатов поиска

        regex dateRegex(R"(\b\d{2}\.\d{2}\.\d{4}\b)");// Поиск даты
        regex initialsRegex(Utf8_to_cp1251(R"([А-ЯЁ]\.[А-ЯЁ]\.)"));// Поиск инициалов

        vector<poppler::text_box> poplerTextBoxes;// Слова из документа

        const float coeff = 5.5563;// Коэффициент преобразования координат из pdf в img


        // Открытие документа
        {
            unique_ptr<document> doc(document::load_from_file(pdf_path));// Загружаем PDF-документ
            poppler::page* page = NULL;
            page = doc->create_page(pageNum);

            if (!doc) {
                logMessage("Ошибка загрузки PDF.", "system");
                return;
            }
            if (!page) {
                logMessage("Ошибка открытия страницы ", "system");
                return;
            }

            poplerTextBoxes = page->text_list();

            delete page;
        }

        // Обработка этого текста
        MyTextBox tb;
        for (int i = 0; i < poplerTextBoxes.size(); i++) {

            // Извлечение текста
            {
                const auto& box = poplerTextBoxes[i];
                byte_array byte_arr = box.text().to_utf8();
                string text(byte_arr.data(), byte_arr.size()), text1251 = Utf8_to_cp1251(text.c_str());



                tb = {

                    .x = static_cast<int>(round(box.bbox().x() * coeff) - CutsOffX),
                    .y = static_cast<int>(round(box.bbox().y() * coeff) - CutsOffY),
                    .x1 = static_cast<int>(round(box.bbox().right() * coeff) - CutsOffX),
                    .y1 = static_cast<int>(round(box.bbox().bottom() * coeff) - CutsOffY),

                    .text = text,
                    .text1251 = text1251
                };

                tb.charWidth = (tb.x1 - tb.x) / tb.text1251.size();
                tb.midX = (tb.x1 - tb.x) / 2;
                tb.midY = (tb.y1 - tb.y) / 2;

                textBoxes.push_back(tb);
            }

            // Поиск инициалов и даты
            {
                // Дата
                if (date == -1 && std::regex_search(tb.text1251, match, dateRegex)) {

                    date = i;

                    table.date = tb.text1251;
                }

                // Инициалы
                if (std::regex_search(tb.text1251, match, initialsRegex)) {

                    int pos = match.position();
                    myCoord coord;

                    coord = {
                        tb.x + tb.charWidth * pos,
                        tb.y,
                        tb.text.substr(pos, 6) };

                    t2.push_back(coord);
                }
            }

            // Поиск и внесение в клетку
            {
                int row = -1, col = -1;
                int y, x;

                // Текст бокс вне таблицы
                if (table.cells[0][0].x > tb.x ||
                    table.cells[0][0].y > tb.y ||
                    table.cells.back().back().y1 < tb.y)
                    continue;

                // Поиск строки
                for (int j = 0; j < table.cells.size() && row == -1; j++) {

                    if (table.cells[j][0].y < tb.y && table.cells[j][0].y1 > tb.y)
                        row = j;
                }
                if (row == -1) {
                    logMessage("Не найдена строка!", "system");
                    continue;
                }

                // Поиск столбца
                for (int j = 0; j < table.cells[row].size() && col == -1; j++) {

                    if (table.cells[row][j].x - 2 < tb.x && table.cells[row][j].x1 + 2 > tb.x)
                        col = j;
                }
                if (col == -1) {
                    logMessage("Не найден столбец!", "system");
                    continue;
                }



                for (int j = 0; j < tb.text1251.size(); j++) {

                    if (table.cells[row][col].x1 < tb.charWidth * j + tb.charWidth / 2 + tb.x) {
                        col++;

                        if (table.cells[row].size() <= col) {
                            //logMessage("Выход за пределы столбцов!", "system");
                            col--;
                        }
                    }

                    table.cells[row][col].chars[tb.text1251[j]]++;
                }

            }
        }

        if (date == -1) {
            throw(std::runtime_error("Дата не найдена!"));
        }
    }

    //// Костыльчик
    {}

    vector<cv::Point2i> changedDots; // точки посреди изменённых сейчас ячеек

    // Поиск различий в файле
    {

        // Чтение старой таблицы
        {
            Table oldTable;

            std::ifstream input(rb::imgPath + mPage.folderName + "\\data.json");

            if (input.is_open()) {

                json j2;
                input >> j2;
                oldTable = j2.get<Table>();

                if (oldTable.date == table.date) {
                    mPage.IsNewPage = 0;

                    int rowCount = min(oldTable.cells.size(), table.cells.size());

                    for (int i = 0; i < rowCount; i++) {
                        int colCount = min(oldTable.cells[i].size(), table.cells[i].size());

                        for (int j = 0; j < colCount; j++) {

                            // Сама проверка
                            if (oldTable.cells[i][j].chars != table.cells[i][j].chars) {

                                table.cells[i][j].changedEarlier = 1;
                                table.cells[i][j].changedNow = 1;
                                changedDots.push_back({
                                    table.cells[i][j].x1 - table.cells[i][j].x / 2,
                                    table.cells[i][j].y1 - table.cells[i][j].y / 2
                                    });
                            }
                            else if (oldTable.cells[i][j].changedEarlier) {
                                table.cells[i][j].changedEarlier = 1;
                            }
                        }
                    }

                }
                else {
                    mPage.IsNewPage = 1;
                }
            }
            else
                mPage.IsNewPage = 1;

            json j = table;
            std::ofstream(rb::imgPath + mPage.folderName + "\\data.json") << j.dump(4);
        }

        // Поиск и отрисовка temporarily
        /*
        Mat overlay;
        image.copyTo(overlay);
        for (int i = 0; i < table.cells.size(); i++) {
            for (int j = 0; j < table.cells[i].size(); j++) {

                Cell& cell = table.cells[i][j];

                if (cell.changedEarlier || cell.changedNow) {

                    /*Rect roi1(
                        xDots[1][cell.indexInX],
                        yDots[1][cell.indexInY],

                        xDots[0][cell.indexInX + 1] - xDots[1][cell.indexInX],
                        yDots[0][cell.indexInY + 1] - yDots[1][cell.indexInY]
                    );

                    Rect roi1(
                        cell.x,
                        cell.y,

                        cell.x1 - cell.x,
                        cell.y1 - cell.y
                    );

                    cv::Scalar color;

                    if (cell.changedNow) {

                        color = { 0, 0, 255 };

                        changedDots.push_back({
                            cell.x + (cell.x1 - cell.x) / 2,
                            cell.y + (cell.y1 - cell.y) / 2
                            });

                    }
                    else
                        color = { 0, 255, 255 };

                    // Закрашиваем область
                    {
                        int thickness = 30;
                        double angle = 45.0;
                        double alpha = 0.5; // прозрачность линий

                        // создаём слой для линий
                        cv::Mat hatch = image.clone();

                        double rad = angle * CV_PI / 180.0;
                        double tanA = tan(rad);
                        for (int x = -image.rows; x < image.cols + image.rows; x += thickness * 3) {
                            cv::Point p1(x, 0);
                            cv::Point p2(cvRound(x + image.rows / tanA), image.rows);
                            cv::line(hatch, p1, p2, color, thickness, cv::LINE_AA);
                        }

                        // Маска области
                        cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
                        cv::rectangle(mask, roi1, 255, cv::FILLED);

                        // Берём исходный фон для overlay
                        image.copyTo(overlay);

                        // Рисуем линии только в нужной области
                        hatch.copyTo(overlay, mask);

                        // Накладываем overlay с прозрачностью
                        cv::addWeighted(overlay, alpha, image, 1.0 - alpha, 0, image);
                    }
                }

            }
        }
        */
        //cv::addWeighted(overlay, 0.50, image, 1 - 0.50, 0, image);
        image.copyTo(imageCoper);
        image.copyTo(imageCoper2);
    }

    int padding = 0;//отступы в маленькой версии картинки
    int padding1 = 0;//отступы между датой в маленькой версии картинки


    // Разделение
    {
        smatch match;// Переменная для результатов поиска

        regex groupRegex(Utf8_to_cp1251(R"([А-ЯЁ]{2,3}-\d{3}[а-яё]*)"));// Поиск групп
        regex teacherRegex(Utf8_to_cp1251(R"([А-ЯЁ][а-яё]{2,})"));// Поиск преподавателей

        map <string, Mat>teachers;

        Mat overlay2, overlay3;
        imageCoper2.copyTo(overlay2);
        imageCoper.copyTo(overlay3);

        Mat timeImage, corpsNumImage;

        // Подготовка к разделению
        {
            //динамические отступы
            if (xDots[1][0] - xDots[0][0] < 3 || xDots[1][0] - xDots[0][0] > 40) {
                padding = 7;
                padding1 = 7;
            }
            else {
                padding = (xDots[1][0] - xDots[0][0]) * 0.8;
                padding1 = (xDots[1][0] - xDots[0][0]) * 2;
            }


            // Вырезка даты
            Rect dateR(
                textBoxes[date].x - padding,
                textBoxes[date].y - padding,
                textBoxes[date + 2].x1 - textBoxes[date].x + 2 * padding,
                textBoxes[date + 2].y1 - textBoxes[date].y + 2 * padding
            );
            timeImage = image(dateR);


            // Отрисовка номера корпуса
            corpsNumImage = cv::Mat(timeImage.rows, timeImage.cols, CV_8UC3, cv::Scalar(255, 255, 255));

            drawTextFT(
                corpsNumImage,
                Utf8_to_cp1251((to_string(sync::CurrentCorp + 1) + " корпус").c_str()),
                "times.ttf",
                timeImage.rows * 0.8,
                timeImage.cols / 2,
                timeImage.rows / 2
            );
        }


        for (MyTextBox& tb : textBoxes) {

            // Группы
            if (regex_search(tb.text1251, match, groupRegex)) {

                int dotE = -1;// итератор крайней точки (точка у группы)
                int dotX = -1;// итератор x точки
                int dotY = -1;// итератор y точки

                // Выбор подходящей крайней точки (точка у группы)
                for (int i = 0; i < extremDotsI.size(); i++) {

                    if (tb.y + tb.midY < yDots[0][extremDotsI[i]]) {
                        dotE = extremDotsI[i];
                        i = extremDotsI.size();
                    }
                }

                // Выбор подходящей x точки
                for (int i = 0; i < xDots[0].size(); i++) {
                    if (tb.x <= xDots[0][i]) {
                        dotX = i;
                        i = xDots[0].size();
                    }
                }

                // Выбор подходящей y точки
                for (int i = 0; i < yDots[0].size(); i++) {
                    if (tb.y + tb.midY <= yDots[0][i]) {
                        dotY = i;
                        i = yDots[0].size();
                    }
                }

                // Проверка на ошибки
                if (dotE == -1 || dotX == -1 || dotY == -1) {
                    logMessage(std::format("ошибка 1 {}, {}, {}, {}", dotE, dotX, dotY, tb.text), "system");
                    continue;
                }

                // Выделение и обрезка
                {
                    // Зелёное выделение
                    Rect greenSelection(
                        xDots[1][dotX - 1] + 1,
                        yDots[1][dotY - 1] + 1,
                        xDots[0][dotX] - xDots[1][dotX - 1] - 1,
                        yDots[0][dotE] - yDots[1][dotY - 1] - 1
                    );

                    // Звонки
                    Rect schedule(
                        xDots[1][dotX - 1],
                        yDots[0][dotY - 1],
                        xDots[1][dotX] - xDots[1][dotX - 1],
                        yDots[1][dotE] - yDots[0][dotY - 1]
                    );

                    // Пары
                    Rect lessons(
                        xDots[0][0],
                        yDots[0][dotY - 1],
                        xDots[1][3] - xDots[0][0],
                        yDots[1][dotE] - yDots[0][dotY - 1]
                    );

                    // Вырезанные группа и звонки
                    Mat croppedLessons = image(lessons), croppedSchedule = image(schedule);

                    Size newSize(max(
                        croppedLessons.cols + croppedSchedule.cols, timeImage.cols) + 2 * padding,
                        croppedLessons.rows + timeImage.rows * 2 + 2 * (padding + padding1
                            ));

                    // Мини версия
                    Mat result(newSize, image.type(), Scalar(255, 255, 255));

                    // Копирование всего на мини версию
                    croppedLessons.copyTo(result(Rect(
                        padding,
                        timeImage.rows * 2 + padding1 * 2 + padding,
                        croppedLessons.cols,
                        croppedLessons.rows
                    )));

                    croppedSchedule.copyTo(result(Rect(
                        croppedLessons.cols + padding,
                        timeImage.rows * 2 + padding1 * 2 + padding,
                        croppedSchedule.cols,
                        croppedSchedule.rows
                    )));

                    corpsNumImage.copyTo(result(Rect(
                        (result.cols - timeImage.cols) / 2 + padding,
                        0,
                        timeImage.cols,
                        timeImage.rows
                    )));

                    timeImage.copyTo(result(Rect(
                        (result.cols - timeImage.cols) / 2 + padding,
                        corpsNumImage.rows,
                        timeImage.cols,
                        timeImage.rows
                    )));

                    // Картинка с выделенной группой
                    Mat tempImage;
                    image.copyTo(tempImage);

                    Mat overlay;
                    tempImage.copyTo(overlay);

                    cv::rectangle(overlay, greenSelection, { 0, 255, 0 }, cv::FILLED);
                    cv::addWeighted(overlay, 0.25, tempImage, 1 - 0.25, 0, tempImage);
                    cv::rectangle(overlay3, greenSelection, { 0, 255, 0 }, cv::FILLED);

                    if (mPage.IsNewPage == 1) {
                        for (int i = 0; i < changedDots.size(); i++) {
                            if (greenSelection.contains(changedDots[i])) {
                                int groupId = findGroup(tb.text);
                                if (groupId != -1) {
                                    mPage.groups[groupId].changed = 1;
                                }
                                else {
                                    logMessage("Неожиданная группа " + tb.text, "system");
                                }

                                break;
                            }
                        }
                    }

                    if (!cv::imwrite(folderToSave + tb.text1251 + "S.png", result))
                        logMessage("Не удалось записать файл " + tb.text, "system");

                    if (cv::imwrite(folderToSave + tb.text1251 + ".png", tempImage)) {
                        int groupId = findGroup(tb.text);
                        if (groupId != -1) {
                            mPage.groups[groupId] = 1;
                        }
                        else {
                            logMessage("Неожиданная группа " + tb.text, "system");
                        }
                    }
                    else
                        logMessage("Не удалось записать файл " + tb.text, "system");
                }
            }


            // Преподаватели
            if (regex_search(tb.text1251, match, teacherRegex)) {

                int dotX = -1;// итератор x точки
                int dotY = -1;// итератор y точки
                int numT2 = -1; // Итератор инициалов

                // Проверка на лишний текст
                if (match.suffix().length() > 0 || match.prefix().length() > 0) {

                    tb.text1251 = tb.text1251.substr(match.prefix().length(), match.length());
                    tb.text = cp1251_to_utf8(tb.text1251.c_str());

                    tb.x += tb.charWidth * match.prefix().length();
                    tb.x1 -= tb.charWidth * match.suffix().length();
                    tb.midX = (tb.x1 - tb.x) / 2;
                }

                // Поиск инициалов
                {
                    float min_distantion = 500000;

                    for (int i = 0; i < t2.size(); i++) {
                        auto& a = t2[i];
                        float distanation = a.x - tb.x1;

                        if (abs(a.y - tb.y) < 4 && min_distantion > distanation && distanation > 0) {
                            numT2 = i;
                            min_distantion = distanation;
                        }
                    }

                    if (numT2 == -1 || min_distantion > tb.charWidth * 2)
                        continue;
                }

                tb.text += t2[numT2].text;

                if (teachers.find(tb.text) == teachers.end()) {
                    image.copyTo(teachers[tb.text]);
                }

                // Выбор подходящей x точки
                for (int i = 0; i < xDots[0].size(); i++) {
                    if (tb.x <= xDots[0][i]) {
                        dotX = i;
                        i = xDots[0].size();
                    }
                }

                // Выбор подходящей y точки
                for (int i = 0; i < yDots[0].size(); i++) {
                    if (tb.y + tb.midY <= yDots[0][i]) {
                        dotY = i;
                        i = yDots[0].size();
                    }
                }

                // Проверка на ошибки
                if (dotX == -1 || dotY == -1) {
                    logMessage(std::format("ошибка 1 {}, {}, {}", dotX, dotY, tb.text), "system");
                    continue;
                }


                Rect roi1(
                    xDots[1][0] + 1,
                    yDots[1][dotY - 1] + 1,
                    xDots[0][3] - xDots[1][0] - 1,
                    yDots[0][dotY] - yDots[1][dotY - 1] - 1
                );

                Rect roi2(
                    xDots[1][dotX - 1] + 1,
                    yDots[1][dotY - 1] + 1,
                    xDots[0][dotX] - xDots[1][dotX - 1] - 1,
                    yDots[0][dotY] - yDots[1][dotY - 1] - 1
                );

                Rect roi3(
                    tb.x - padding,
                    tb.y - padding,
                    tb.x1 - tb.x + 2 * padding,
                    tb.y1 - tb.y + 2 * padding
                );  // x, y, ширина, высота

                Mat overlay;
                teachers[tb.text].copyTo(overlay);

                cv::rectangle(overlay, roi2, { 0, 255, 0 }, cv::FILLED);
                cv::rectangle(overlay, roi1, { 0, 255, 0 }, cv::FILLED);
                cv::addWeighted(overlay, 0.25, teachers[tb.text], 1 - 0.25, 0, teachers[tb.text]);
                //teachers[text].at<Vec3b>(y1, x1) = cv::Vec3b(0, 0, 254); // Установка цвета пикселя

                cv::rectangle(overlay2, roi3, { 0, 255, 0 }, cv::FILLED);
            }
        }

        // Сохранение картинок с преподавателями
        for (auto& teacher : teachers) {
            if (cv::imwrite(folderToSave + Utf8_to_cp1251(teacher.first.c_str()) + ".png", teacher.second))
                mPage.Teachers.insert(teacher.first);
            else {
                logMessage("Не удалось записать файл " + teacher.first, "system");
            }

            if (rb::AllTeachers.find(teacher.first) == rb::AllTeachers.end())
                rb::AllTeachers.insert(teacher.first);
        }

        // Запись преподавателей в файл
        std::ofstream outputFile(rb::imgPath + "4\\t.txt");
        for (const string& tea : rb::AllTeachers) {
            outputFile << tea << '\n';
        }
        outputFile.close();  // Закрываем файл

        //добавление рекламы
        if (cfg::EnableAd) {
            Mat adImg = imread("..\\imgs\\ad.png");//   ..\\imgs\\ad.png
            if (adImg.data) {
                Mat overlay;
                int y = yDots[0][0] + (yDots[1][1] - yDots[0][0]) / 2 > adImg.rows + 10 ? yDots[0][0] + (yDots[1][1] - yDots[0][0]) / 2 - adImg.rows : 5;

                image.copyTo(overlay);
                adImg.copyTo(overlay(Rect(xDots[0][xDots[0].size() - 1] - adImg.cols, y, adImg.cols, adImg.rows)));
                cv::addWeighted(overlay, 0.75, image, 1 - 0.75, 0, image);

                cv::imwrite(imageName, image);
            }
        }

        cv::addWeighted(overlay3, 0.20, imageCoper, 1 - 0.20, 0, imageCoper);
        cv::addWeighted(overlay2, 0.50, imageCoper2, 1 - 0.50, 0, imageCoper2);
    }


    //сохранение мусора ;)
    cv::imwrite(imageName, image);
    cv::imwrite(folderToSave + "coper.png", imageCoper);
    cv::imwrite(folderToSave + "coper2.png", imageCoper2);
}

void main2() {

    try
    {
        // Начальная инициализация
        for (corps& corp : rb::corpss) {
            ReadStringFromFile(corp.pdfFileName, corp.LastFileD);
        }

        // Проверки
        while (true) {

            try {

                // Обновление расписания
                for (corps& corp : rb::corpss) {
                    if (!DownloadFileToMemory(MainUrl + corp.pdfFileName, FileDownloaded)) {
                        logMessage("Не удалось скачать файл с расписанием", "system", 120);
                        continue;
                    }

                    if (FileDownloaded != corp.LastFileD) {

                        int pageCount = 0;

                        logMessage("Начало обработки нового расписания, " + to_string(corp.localOffset + 1) + " корпус", "system", 112);

                        
                        // Приостанавливаем бота
                        {

                            sync::CurrentCorp = corp.localOffset;
                            sync::ErrorOnCore = 0;

                            bool wait = 1;

                            {
                                std::lock_guard<std::mutex> lock(sync::mtx1);

                                sync::SyncMode = sync::Mods::botStopping;
                                sync::SyncAction = sync::Actions::nothing;
                            }

                            while (wait) {
                                this_thread::sleep_for(100ms);

                                {
                                    std::lock_guard<std::mutex> lock(sync::mtx1);

                                    wait = sync::SyncMode != sync::Mods::botStopped;
                                }
                            }
                        }


                        // Подготовка
                        {
                            
                            if (!WriteStringToFile(FileDownloaded, corp.pdfFileName)) {
                                logMessage("Не удалось записать файл .pdf", "system");
                                throw std::runtime_error("zalupka");
                            }


                            pageCount = getPDFPageCount(corp.pdfFileName);
                            if (pageCount < 1 || pageCount > 10) {
                                logMessage(std::format("Неверное количество страниц: {}", pageCount), "system");
                                throw std::runtime_error("zalupka2");
                            }


                            // Зачистка папок
                            for (int i = pageCount + rb::pagesInBui * (corp.localOffset); i < rb::pagesInBui * (corp.localOffset + 1); i++) {

                                for (const auto& entry : std::filesystem::directory_iterator(rb::imgPath + to_string(i))) {

                                    if (entry.is_regular_file() && entry.path().extension().string() == ".png") {
                                        std::filesystem::remove(entry.path());
                                    }
                                }
                            }
                        }


                        // Зачистка переменных корпуса
                        {
                            for (auto& page : corp.pages)
                                page.clear();
                        }


                        // Обработка страниц
                        for (int i = 0; i < pageCount && i < rb::pagesInBui; i++) {

                            auto& page = corp.pages[i];
                            page.isEmpty = 0;

                            system(std::format("magick -density 400 {}[{}] -background white -flatten -quality 100 {}.png",
                                corp.pdfFileName, i, rb::imgPath + page.folderName).c_str());

                            // Обработка картинок
                            try
                            {
                                editRaspis(rb::imgPath + page.folderName + ".png");
                                getLocalRaspis(page, corp.pdfFileName, i);
                            }
                            catch (const std::exception& e)
                            {
                                logMessage("Какой ужас! Скинь мне это tg: @wyanarba EROR: гет локал распис | " + (string)e.what(), "system", 113);
                                sync::ErrorOnCore = 1;
                            }
                        }


                        // Всё было успешно
                        corp.LastFileD = FileDownloaded;


                        // Рассылка
                        {

                            bool wait = 1;

                            {
                                std::lock_guard<mutex> lock(sync::mtx1);

                                sync::SyncMode = sync::Mods::botStarting;
                                sync::SyncAction = sync::Actions::sendNewRasp;
                            }

                            
                            while (wait) {

                                this_thread::sleep_for(300ms);

                                {
                                    std::lock_guard<mutex> lock(sync::mtx1);

                                    wait = sync::SyncMode != sync::Mods::free;
                                }
                            }
                        }

                        logMessage("Конец обработки нового расписания", "system", 115);
                    }
                }

                // Поиск обновления
                {
                    if (cfg::EnableAutoUpdate && sync::AttemptsToCheck == 0) {//чек обновы

                        sync::AttemptsToCheck++;

                        if (DownloadFileToMemory("https://wyanarba.github.io/rBot/", sync::NewVersion) && sync::NewVersion.size() < 30) {
                            if (sync::NewVersion != CurrentVersion) {
                                logMessage("Обнова!!! " + CurrentVersion + " -> " + sync::NewVersion, "system");

                                bool wait = 1;

                                {
                                    std::lock_guard<mutex> lock(sync::mtx1);

                                    sync::SyncMode = sync::Mods::botStopping;
                                    sync::SyncAction = sync::Actions::update;
                                }


                                while (wait) {
                                    this_thread::sleep_for(100ms);

                                    {
                                        std::lock_guard<mutex> lock(sync::mtx1);

                                        wait = sync::SyncMode != sync::Mods::botStopped;
                                    }
                                }

                                system(UpdateCommand);
                                exit(0);
                            }
                        }
                        else {
                            logMessage("Не удалось скачать версию", "system", 121);
                        }
                    }

                    else if (sync::AttemptsToCheck < 10)
                        sync::AttemptsToCheck++;
                    else if (sync::AttemptsToCheck == 10)
                        sync::AttemptsToCheck = 0;
                }

                // Смена года
                {
                    if (sync::AttemptsToCheck2 == 0) {

                        sync::AttemptsToCheck2++;

                        time_t t = time(nullptr);
                        tm now = {};
                        localtime_s(&now, &t);

                        now.tm_year = (now.tm_year + 1900) % 100;

                        if (now.tm_year > sync::CurrentYear && (now.tm_mon > 6 || (now.tm_mon == 6 && now.tm_mday > 4))) {

                            logMessage("Смена года!!! " + to_string(sync::CurrentYear) + " -> " + to_string(now.tm_year), "system");

                            bool wait = 1;

                            {
                                std::lock_guard<mutex> lock(sync::mtx1);

                                sync::SyncMode = sync::Mods::botStopping;
                                sync::SyncAction = sync::Actions::changeYear;
                            }


                            while (wait) {
                                this_thread::sleep_for(100ms);

                                {
                                    std::lock_guard<mutex> lock(sync::mtx1);

                                    wait = sync::SyncMode != sync::Mods::botStopped;
                                }
                            }


                            // Сама смена
                            rb::Groups.clear();
                            rb::Groups1251.clear();

                            genGroups();
                            for (int i = 0; i < rb::Groups.size(); i++) {
                                rb::Groups1251.push_back(Utf8_to_cp1251(rb::Groups[i].c_str()));
                            }


                            // Возращение
                            {
                                std::lock_guard<mutex> lock(sync::mtx1);

                                sync::SyncMode = sync::Mods::botStarting;
                                sync::SyncAction = sync::Actions::nothing;
                            }

                        }
                    }

                    else if (sync::AttemptsToCheck2 < 30)
                        sync::AttemptsToCheck2++;
                    else if (sync::AttemptsToCheck == 30)
                        sync::AttemptsToCheck2 = 0;
                }
            }
            catch (const std::exception& e)
            {
                logMessage("css) EROR | " + (string)e.what(), "system", 121);

                for (auto& corp : rb::corpss) {
                    WriteStringToFile(corp.LastFileD, corp.pdfFileName);
                }

                bool wait = 1;

                // Возращение
                {
                    std::lock_guard<mutex> lock(sync::mtx1);

                    sync::SyncMode = sync::Mods::botStarting;
                    sync::SyncAction = sync::Actions::nothing;
                }

                while (wait) {

                    this_thread::sleep_for(300ms);

                    {
                        std::lock_guard<mutex> lock(sync::mtx1);

                        wait = sync::SyncMode != sync::Mods::free;
                    }
                }
            }


            this_thread::sleep_for(chrono::seconds(60));
        }
    }
    catch (const std::exception& e)
    {
        logMessage("c) EROR | " + (string)e.what(), "system", 121);
    }
}
