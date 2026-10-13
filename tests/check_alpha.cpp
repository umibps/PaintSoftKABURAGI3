#include <QImage>
#include <iostream>

int main() {
    QImage img("E:/Works/Programming/PaintSoftKABURAGI3_0_1/scratch_mat_10_F00_002_01_Tops_01_CLOTH.png");
    std::cout << "size: " << img.width() << "x" << img.height() << " format: " << img.format() << std::endl;
    // (1024, 1500) はスカートの下半分
    QRgb p = img.pixel(1024, 1500);
    std::cout << "skirt pixel alpha: " << qAlpha(p) << " R=" << qRed(p) << " G=" << qGreen(p) << " B=" << qBlue(p) << std::endl;
    // (1024, 500) はエプロン・胸
    QRgb p2 = img.pixel(1024, 500);
    std::cout << "chest pixel alpha: " << qAlpha(p2) << " R=" << qRed(p2) << " G=" << qGreen(p2) << " B=" << qBlue(p2) << std::endl;
    // (500, 500) は透明領域?
    QRgb p3 = img.pixel(500, 500);
    std::cout << "bg pixel alpha: " << qAlpha(p3) << " R=" << qRed(p3) << " G=" << qGreen(p3) << " B=" << qBlue(p3) << std::endl;
    return 0;
}
