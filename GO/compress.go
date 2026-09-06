package main

import (
	"fmt"
	"image"
	_ "image/gif"
	"image/jpeg"
	"image/png"
	"io/fs"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"

	"golang.org/x/image/draw"
)

// Код полностью от gemeni

const (
	maxDimensionSum = 10000
)

func main() {
	rootDir := "./../rImages"

	// Ограничиваем пул числом логических ядер CPU, чтобы не убить RAM
	concurrency := runtime.NumCPU() - 2
	if concurrency < 1 {
		concurrency = 1
	}
	sem := make(chan struct{}, concurrency)
	var wg sync.WaitGroup

	err := filepath.WalkDir(rootDir, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}

		ext := strings.ToLower(filepath.Ext(path))
		if ext != ".jpg" && ext != ".jpeg" && ext != ".png" {
			return nil
		}

		sem <- struct{}{} // Занимаем слот
		wg.Add(1)

		go func(filePath, fileExt string) {
			defer func() {
				<-sem // Освобождаем слот
				wg.Done()
			}()

			if err := processImage(filePath, fileExt); err != nil {
				fmt.Printf("Ошибка обработки %s: %v\n", filePath, err)
			}
		}(path, ext) // Передаем переменные аргументами

		return nil
	})

	if err != nil {
		fmt.Printf("Ошибка сканирования директории: %v\n", err)
	}

	// Ждем завершения всех горутин перед выходом
	wg.Wait()
	fmt.Println("Обработка завершена.")
}

func processImage(path, ext string) error {
	// 1. Читаем и декодируем исходник
	srcImg, err := readAndDecode(path)
	if err != nil {
		return err
	}
	if srcImg == nil {
		return nil // Размеры в пределах нормы, пропускаем
	}

	bounds := srcImg.Bounds()
	w, h := bounds.Dx(), bounds.Dy()
	totalPixels := w + h

	scale := float64(maxDimensionSum) / float64(totalPixels)
	newWidth := int(float64(w) * scale)
	newHeight := int(float64(h) * scale)

	fmt.Printf("Ресайз: %s [%dx%d -> %dx%d]\n", path, w, h, newWidth, newHeight)

	// 2. Масштабируем
	dstImg := image.NewRGBA(image.Rect(0, 0, newWidth, newHeight))
	draw.CatmullRom.Scale(dstImg, dstImg.Bounds(), srcImg, bounds, draw.Over, nil)

	// 3. Перезаписываем файл
	outFile, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("создание файла: %w", err)
	}
	defer outFile.Close()

	switch ext {
	case ".jpg", ".jpeg":
		return jpeg.Encode(outFile, dstImg, &jpeg.Options{Quality: 90})
	case ".png":
		return png.Encode(outFile, dstImg)
	}

	return nil
}

func readAndDecode(path string) (image.Image, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	// Читаем метаданные
	cfg, _, err := image.DecodeConfig(file)
	if err != nil {
		return nil, fmt.Errorf("чтение метаданных: %w", err)
	}

	if cfg.Width+cfg.Height <= maxDimensionSum {
		return nil, nil // Не требует ресайза
	}

	// Сбрасываем курсор в начало файла
	if _, err := file.Seek(0, 0); err != nil {
		return nil, err
	}

	srcImg, _, err := image.Decode(file)
	if err != nil {
		return nil, fmt.Errorf("декодирование: %w", err)
	}

	return srcImg, nil
}
