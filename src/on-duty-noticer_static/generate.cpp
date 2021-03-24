#include "generate.h"
using namespace cv;
using namespace std;
using namespace generate;

namespace generate
{
    void change_wallpaper(string base, const char *fname)
    {
        base += fname;
        SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0,
            (PVOID)base.c_str(),
            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }


    void ChangeWallpaper::GetStringSize(HDC hDC, const char* str, int* w, int* h)
    {
        SIZE size;
        GetTextExtentPoint32A(hDC, str, strlen(str), &size);
        if (w != 0) *w = size.cx;
        if (h != 0) *h = size.cy;
    }

    ChangeWallpaper::ChangeWallpaper(const string& full_path)
        : fname(full_path), font(cv::FONT_HERSHEY_DUPLEX)
    {
        mat = new cv::Mat(cv::imread(full_path, cv::ImreadModes::IMREAD_COLOR));
    }

    ChangeWallpaper::ChangeWallpaper(const string & runtime_path, const string& file_rel_path)
        : font(cv::FONT_HERSHEY_DUPLEX)
    {
        fname = getBaseName(runtime_path.c_str());
        fname += file_rel_path;
        mat = new cv::Mat(cv::imread(fname, cv::ImreadModes::IMREAD_COLOR));
    }

    ChangeWallpaper::~ChangeWallpaper()
    {
        if (nullptr != mat) delete mat;
    }

    void ChangeWallpaper::save(const string& argv, const string& img)
    {
        string path = getBaseName(argv.c_str()) + img;
        cv::imwrite(path, *mat);
    }

    string ChangeWallpaper::getBaseName(const char* argv)
    {
        string path(argv);
        while ('\\' != path.back())
            path.pop_back();
        return path;
    }

    void ChangeWallpaper::putTextZH(Mat& dst, const char* str, Point org, Scalar color, int fontSize, const char* fn, bool italic, bool underline)
    {
        CV_Assert(dst.data != 0 && (dst.channels() == 1 || dst.channels() == 3));

        int x, y, r, b;
        if (org.x > dst.cols || org.y > dst.rows) return;
        x = org.x < 0 ? -org.x : 0;
        y = org.y < 0 ? -org.y : 0;

        LOGFONTA lf;
        lf.lfHeight = -fontSize;
        lf.lfWidth = 0;
        lf.lfEscapement = 0;
        lf.lfOrientation = 0;
        lf.lfWeight = 5;
        lf.lfItalic = italic;   //б��
        lf.lfUnderline = underline; //�»���
        lf.lfStrikeOut = 0;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = 0;
        lf.lfClipPrecision = 0;
        lf.lfQuality = PROOF_QUALITY;
        lf.lfPitchAndFamily = 0;
        strcpy_s(lf.lfFaceName, fn);

        HFONT hf = CreateFontIndirectA(&lf);
        HDC hDC = CreateCompatibleDC(0);
        HFONT hOldFont = (HFONT)SelectObject(hDC, hf);

        int strBaseW = 0, strBaseH = 0;
        int singleRow = 0;
        char buf[1 << 12];
        strcpy_s(buf, str);
        char* bufT[1 << 12];  // ������ڷָ��ַ�����ʣ����ַ������ܻᳬ����
        //�������
        {
            int nnh = 0;
            int cw, ch;

            const char* ln = strtok_s(buf, "\n", bufT);
            while (ln != 0)
            {
                GetStringSize(hDC, ln, &cw, &ch);
                strBaseW = max(strBaseW, cw);
                strBaseH = max(strBaseH, ch);

                ln = strtok_s(0, "\n", bufT);
                nnh++;
            }
            singleRow = strBaseH;
            strBaseH *= nnh;
        }

        if (org.x + strBaseW < 0 || org.y + strBaseH < 0)
        {
            SelectObject(hDC, hOldFont);
            DeleteObject(hf);
            DeleteObject(hDC);
            return;
        }

        r = org.x + strBaseW > dst.cols ? dst.cols - org.x - 1 : strBaseW - 1;
        b = org.y + strBaseH > dst.rows ? dst.rows - org.y - 1 : strBaseH - 1;
        org.x = org.x < 0 ? 0 : org.x;
        org.y = org.y < 0 ? 0 : org.y;

        BITMAPINFO bmp = { 0 };
        BITMAPINFOHEADER& bih = bmp.bmiHeader;
        int strDrawLineStep = strBaseW * 3 % 4 == 0 ? strBaseW * 3 : (strBaseW * 3 + 4 - ((strBaseW * 3) % 4));

        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = strBaseW;
        bih.biHeight = strBaseH;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        bih.biSizeImage = strBaseH * strDrawLineStep;
        bih.biClrUsed = 0;
        bih.biClrImportant = 0;

        void* pDibData = 0;
        HBITMAP hBmp = CreateDIBSection(hDC, &bmp, DIB_RGB_COLORS, &pDibData, 0, 0);

        CV_Assert(pDibData != 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);

        //color.val[2], color.val[1], color.val[0]
        SetTextColor(hDC, RGB(255, 255, 255));
        SetBkColor(hDC, 0);
        //SetStretchBltMode(hDC, COLORONCOLOR);

        strcpy_s(buf, str);
        const char* ln = strtok_s(buf, "\n", bufT);
        int outTextY = 0;
        while (ln != 0)
        {
            TextOutA(hDC, 0, outTextY, ln, strlen(ln));
            outTextY += singleRow;
            ln = strtok_s(0, "\n", bufT);
        }
        uchar* dstData = (uchar*)dst.data;
        int dstStep = dst.step / sizeof(dstData[0]);
        unsigned char* pImg = (unsigned char*)dst.data + org.x * dst.channels() + org.y * dstStep;
        unsigned char* pStr = (unsigned char*)pDibData + x * 3;
        for (int tty = y; tty <= b; ++tty)
        {
            unsigned char* subImg = pImg + (tty - y) * dstStep;
            unsigned char* subStr = pStr + (strBaseH - tty - 1) * strDrawLineStep;
            for (int ttx = x; ttx <= r; ++ttx)
            {
                for (int n = 0; n < dst.channels(); ++n) {
                    double vtxt = subStr[n] / 255.0;
                    int cvv = vtxt * color.val[n] + (1 - vtxt) * subImg[n];
                    subImg[n] = cvv > 255 ? 255 : (cvv < 0 ? 0 : cvv);
                }

                subStr += 3;
                subImg += dst.channels();
            }
        }

        SelectObject(hDC, hOldBmp);
        SelectObject(hDC, hOldFont);
        DeleteObject(hf);
        DeleteObject(hBmp);
        DeleteDC(hDC);

    }

    // copied from the blog,
    // to print chinese character on opencv mat
    /**
    void GetStringSize(HDC hDC, const char* str, int* w, int* h)
    {
        SIZE size;
        GetTextExtentPoint32A(hDC, str, strlen(str), &size);
        if (w != 0) *w = size.cx;
        if (h != 0) *h = size.cy;
    }
    */
    /** 
    void putTextZH(Mat& dst, const char* str, Point org, Scalar color, int fontSize, const char* fn, bool italic, bool underline)
    {
        CV_Assert(dst.data != 0 && (dst.channels() == 1 || dst.channels() == 3));

        int x, y, r, b;
        if (org.x > dst.cols || org.y > dst.rows) return;
        x = org.x < 0 ? -org.x : 0;
        y = org.y < 0 ? -org.y : 0;

        LOGFONTA lf;
        lf.lfHeight = -fontSize;
        lf.lfWidth = 0;
        lf.lfEscapement = 0;
        lf.lfOrientation = 0;
        lf.lfWeight = 5;
        lf.lfItalic = italic;   //б��
        lf.lfUnderline = underline; //�»���
        lf.lfStrikeOut = 0;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = 0;
        lf.lfClipPrecision = 0;
        lf.lfQuality = PROOF_QUALITY;
        lf.lfPitchAndFamily = 0;
        strcpy_s(lf.lfFaceName, fn);

        HFONT hf = CreateFontIndirectA(&lf);
        HDC hDC = CreateCompatibleDC(0);
        HFONT hOldFont = (HFONT)SelectObject(hDC, hf);

        int strBaseW = 0, strBaseH = 0;
        int singleRow = 0;
        char buf[1 << 12];
        strcpy_s(buf, str);
        char* bufT[1 << 12];  // ������ڷָ��ַ�����ʣ����ַ������ܻᳬ����
        //�������
        {
            int nnh = 0;
            int cw, ch;

            const char* ln = strtok_s(buf, "\n", bufT);
            while (ln != 0)
            {
                GetStringSize(hDC, ln, &cw, &ch);
                strBaseW = max(strBaseW, cw);
                strBaseH = max(strBaseH, ch);

                ln = strtok_s(0, "\n", bufT);
                nnh++;
            }
            singleRow = strBaseH;
            strBaseH *= nnh;
        }

        if (org.x + strBaseW < 0 || org.y + strBaseH < 0)
        {
            SelectObject(hDC, hOldFont);
            DeleteObject(hf);
            DeleteObject(hDC);
            return;
        }

        r = org.x + strBaseW > dst.cols ? dst.cols - org.x - 1 : strBaseW - 1;
        b = org.y + strBaseH > dst.rows ? dst.rows - org.y - 1 : strBaseH - 1;
        org.x = org.x < 0 ? 0 : org.x;
        org.y = org.y < 0 ? 0 : org.y;

        BITMAPINFO bmp = { 0 };
        BITMAPINFOHEADER& bih = bmp.bmiHeader;
        int strDrawLineStep = strBaseW * 3 % 4 == 0 ? strBaseW * 3 : (strBaseW * 3 + 4 - ((strBaseW * 3) % 4));

        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = strBaseW;
        bih.biHeight = strBaseH;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        bih.biSizeImage = strBaseH * strDrawLineStep;
        bih.biClrUsed = 0;
        bih.biClrImportant = 0;

        void* pDibData = 0;
        HBITMAP hBmp = CreateDIBSection(hDC, &bmp, DIB_RGB_COLORS, &pDibData, 0, 0);

        CV_Assert(pDibData != 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);

        //color.val[2], color.val[1], color.val[0]
        SetTextColor(hDC, RGB(255, 255, 255));
        SetBkColor(hDC, 0);
        //SetStretchBltMode(hDC, COLORONCOLOR);

        strcpy_s(buf, str);
        const char* ln = strtok_s(buf, "\n", bufT);
        int outTextY = 0;
        while (ln != 0)
        {
            TextOutA(hDC, 0, outTextY, ln, strlen(ln));
            outTextY += singleRow;
            ln = strtok_s(0, "\n", bufT);
        }
        uchar* dstData = (uchar*)dst.data;
        int dstStep = dst.step / sizeof(dstData[0]);
        unsigned char* pImg = (unsigned char*)dst.data + org.x * dst.channels() + org.y * dstStep;
        unsigned char* pStr = (unsigned char*)pDibData + x * 3;
        for (int tty = y; tty <= b; ++tty)
        {
            unsigned char* subImg = pImg + (tty - y) * dstStep;
            unsigned char* subStr = pStr + (strBaseH - tty - 1) * strDrawLineStep;
            for (int ttx = x; ttx <= r; ++ttx)
            {
                for (int n = 0; n < dst.channels(); ++n) {
                    double vtxt = subStr[n] / 255.0;
                    int cvv = vtxt * color.val[n] + (1 - vtxt) * subImg[n];
                    subImg[n] = cvv > 255 ? 255 : (cvv < 0 ? 0 : cvv);
                }

                subStr += 3;
                subImg += dst.channels();
            }
        }

        SelectObject(hDC, hOldBmp);
        SelectObject(hDC, hOldFont);
        DeleteObject(hf);
        DeleteObject(hBmp);
        DeleteDC(hDC);
    }
    */
}
