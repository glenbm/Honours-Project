#include "atm.h"
#include "ScreenCaptureProcessor.h";
#include <iostream>
#include <fstream>
#include "OleAuto.h";

#include <chrono>;

#include <Shlwapi.h>;
#pragma comment (lib,"Shlwapi.lib")

// defined due to the GDI+ libraries just using min/max without std and causing
// conflicts with other code
namespace Gdiplus
{
    using std::min;
    using std::max;
}

// GDI+ setup (ignore all warnings in these headers)
#pragma warning(push, 0)
#define GDIPVER     0x0110  // Use more advanced GDI+ features
#include <objidl.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")
#pragma warning(pop)


int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Failure

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;  // Failure

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

int rand()
{
    // Initialize GDI+.
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);


    CLSID encoderClsid;
    INT    result;
    WCHAR  strGuid[39];

    result = GetEncoderClsid(L"image/png", &encoderClsid);

    if (result < 0)
    {
        printf("The PNG encoder is not installed.\n");
    }
    else
    {
        StringFromGUID2(encoderClsid, strGuid, 39);
        printf("An ImageCodecInfo object representing the PNG encoder\n");
        printf("was found at position %d in the array.\n", result);
        wprintf(L"The CLSID of the PNG encoder is %s.\n", strGuid);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}

void ScreenCaptureProcessor::InitScreenCapture()
{
    
    rand();

    hdcScreen = CreateDCW(L"DISPLAY", L"\\\\.\\DISPLAY1", NULL, NULL);

    //targetWidth = 1920; targetHeight = 1080;
    targetWidth = 1152; targetHeight = 648;
    //targetWidth = 768; targetHeight = 432;
    //targetWidth = 384; targetHeight = 216;

    // GET SYSTEM METRICS
    screenWidth = GetDeviceCaps(hdcScreen, HORZRES);
    screenHeight = GetDeviceCaps(hdcScreen, VERTRES);

    imageDataSize = targetWidth * targetHeight * 4;

    // BMP FILE HEADER
    memset(&bmpHeader, 0, sizeof(bmpHeader));
    bmpHeader.bfType = 0x4D42;
    bmpHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // BMP INFO
    memset(&bmpInfo, 0, sizeof(bmpInfo));
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = targetWidth;
    bmpInfo.bmiHeader.biHeight = targetHeight;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 32;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    bmpInfo.bmiHeader.biSizeImage = imageDataSize;

    InitCaptureData(&screenCap);
    //InitCaptureData(&bufferOne);
}

int ScreenCaptureProcessor::InitCaptureData(CaptureData* capture)
{
    capture->memDc = CreateCompatibleDC(hdcScreen);
    capture->bits = NULL;
    capture->hBitmap = CreateDIBSection(
        hdcScreen,
        &bmpInfo,
        DIB_RGB_COLORS,
        (void**)&capture->bits,
        NULL,
        0
    );

    if (!SelectObject(capture->memDc, capture->hBitmap))
    {
        return -1;
    }

    if (!PatBlt(
        capture->memDc,
        0,
        0,
        targetWidth,
        targetHeight,
        BLACKNESS
    ))
    {
        return -1;
    }

    return 1;
}


int ScreenCaptureProcessor::NEWCAPTUREMETHOD()
{
    auto t1 = std::chrono::high_resolution_clock::now();

    /*
    /
    /   CAPTURE DESKTO AND SCALE CODE
    /
    */

    // FLUSH THE DATA
    GdiFlush();

    // CAPTURE THE DESKTOP SCREEN AND STRETCH DOWN TO DESIRED LENGTH
    SetStretchBltMode(screenCap.memDc, HALFTONE);
    StretchBlt(screenCap.memDc, 0, 0, targetWidth, targetHeight, hdcScreen, 0, 0, screenWidth, screenHeight, SRCCOPY);

    // INIT THE BUFFER TO BE SENT
    std::vector<char> buffer(bmpHeader.bfOffBits + imageDataSize);

    // ASSIGN DATA
    
    if(!memcpy(&buffer[0], (char*)&bmpHeader, sizeof(BITMAPFILEHEADER)))
    {
        int i = 0;
    }
    if(!memcpy(&buffer[sizeof(BITMAPFILEHEADER)], (char*)&bmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER)))
    {
        int i = 0;
    }
    if(!memcpy(&buffer[bmpHeader.bfOffBits], screenCap.bits, imageDataSize))
    {
        int i = 0;
    }

    /*
    /
    /   CONVERT AND COPY TO STREAM CODE
    /
    */

    // INIT
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Init Encoder (JPEG)
    CLSID encoderClsid;
    GetEncoderClsid(L"image/jpeg", &encoderClsid);

    // Init buffer and streams
    std::vector<BYTE> buf;
    IStream *stream = NULL;
    CreateStreamOnHGlobal(NULL, FALSE, (LPSTREAM*)&stream);

    Gdiplus::Bitmap *bmp = new  Gdiplus::Bitmap(screenCap.hBitmap, (HPALETTE)NULL);

    ULONG quality = 30;

    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].Value = &quality;

    /*
    quality = 100;
    encoderParams.Parameter[0].Value = &quality;
    bmp->Save(L"C:\\Users\\e5522198\\Desktop\\TestImage_Quality_100.jpg", &encoderClsid, &encoderParams);

    quality = 80;
    encoderParams.Parameter[0].Value = &quality;
    bmp->Save(L"C:\\Users\\e5522198\\Desktop\\TestImage_Quality_80.jpg", &encoderClsid, &encoderParams);

    quality = 60;
    encoderParams.Parameter[0].Value = &quality;
    bmp->Save(L"C:\\Users\\e5522198\\Desktop\\TestImage_Quality_60.jpg", &encoderClsid, &encoderParams);

    quality = 20;
    encoderParams.Parameter[0].Value = &quality;
    bmp->Save(L"C:\\Users\\e5522198\\Desktop\\TestImage_Quality_20.jpg", &encoderClsid, &encoderParams);
    */

    bmp->Save(stream, &encoderClsid, &encoderParams);
    delete bmp;

    // SEEK
    const LARGE_INTEGER largeIntegerBeginning = { 0 };
    ULARGE_INTEGER largeIntEnd = { 0 };
    stream->Seek(largeIntegerBeginning, STREAM_SEEK_CUR, &largeIntEnd);
    ULONGLONG pngSize = largeIntEnd.QuadPart;

    // DIRECT ACCESS TO PNG's memory
    HGLOBAL pngInMemory;
    const HRESULT hr = GetHGlobalFromStream(stream, &pngInMemory);

    // COPY TO LOCAL BUFFER
    LPVOID lpPngStreamBytes = GlobalLock(pngInMemory);

    char *charBuf = (char*)lpPngStreamBytes;

    //create a vector by copying out the contents of charBuf
    std::vector<char> v(charBuf, charBuf + pngSize);


    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "Screen Capture: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\n";


    OnScreenChanged.notify(this, v);

    //DeleteDC(screenCap.memDc);
    //DeleteDC(hdcScreen);
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 1;

}

Gdiplus::Bitmap* GetScreeny(ULONG uQuality)
{
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    HWND hMyWnd = GetDesktopWindow(); // get my own window
    RECT  r;                             // the area we are going to capture 
    int w, h;                            // the width and height of the area
    HDC dc;                              // the container for the area
    int nBPP;
    HDC hdcCapture;
    LPBYTE lpCapture;
    int nCapture;
    int iRes;
    CLSID imageCLSID;
    Gdiplus::Bitmap *pScreenShot;
    HGLOBAL hMem;
    int result;

    // get the area of my application's window  
    //GetClientRect(hMyWnd, &r);
    GetWindowRect(hMyWnd, &r);
    dc = GetWindowDC(hMyWnd);//   GetDC(hMyWnd) ;
    w = r.right - r.left;
    h = r.bottom - r.top;
    nBPP = GetDeviceCaps(dc, BITSPIXEL);
    hdcCapture = CreateCompatibleDC(dc);


    // create the buffer for the screenshot
    BITMAPINFO bmiCapture = {
        sizeof(BITMAPINFOHEADER), w, -h, 1, nBPP, BI_RGB, 0, 0, 0, 0, 0,
    };

    // create a container and take the screenshot
    HBITMAP hbmCapture = CreateDIBSection(dc, &bmiCapture,
        DIB_PAL_COLORS, (LPVOID *)&lpCapture, NULL, 0);

    // failed to take it
    if (!hbmCapture)
    {
        DeleteDC(hdcCapture);
        DeleteDC(dc);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        printf("failed to take the screenshot. err: %d\n", GetLastError());
        return 0;
    }

    // copy the screenshot buffer
    nCapture = SaveDC(hdcCapture);
    SelectObject(hdcCapture, hbmCapture);
    BitBlt(hdcCapture, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    RestoreDC(hdcCapture, nCapture);
    DeleteDC(hdcCapture);
    DeleteDC(dc);

    // save the buffer to a file    
    pScreenShot = new  Gdiplus::Bitmap(hbmCapture, (HPALETTE)NULL);
    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].Value = &uQuality;
    GetEncoderClsid(L"image/jpeg", &imageCLSID);

    return pScreenShot;
}


int ScreenCaptureProcessor::CaptureScreen()
{
    NEWCAPTUREMETHOD();
    return 1;

    auto t1 = std::chrono::high_resolution_clock::now();

    GdiFlush();


    // Get Full Screen and Scale

    // is it faster to do this is res is the same, or does StretchBlt do it automatically?
    //::BitBlt(screenCap.memDc, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    SetStretchBltMode(screenCap.memDc, HALFTONE);
    if (!StretchBlt(
        screenCap.memDc,
        0,
        0,
        targetWidth,
        targetHeight,
        hdcScreen,
        0,
        0,
        screenWidth,
        screenHeight,
        SRCCOPY
    ))
    {
        return -1;
    }

    // get XOR [delta]
    if (!BitBlt(
        bufferOne.memDc,
        0,
        0,
        targetWidth,
        targetHeight,
        screenCap.memDc,
        0,
        0,
        SRCINVERT
    ))
    {
        return -1;
    }

    std::vector<char> buffer(bmpHeader.bfOffBits + imageDataSize);

    // ASSIGN FILEHEADER DATA
    memcpy(
        &buffer[0],
        (char*)&bmpHeader,
        sizeof(BITMAPFILEHEADER)
    );

    //ASSIGN BMP INFO DATA
    memcpy(
        &buffer[sizeof(BITMAPFILEHEADER)],
        (char*)&bmpInfo.bmiHeader,
        sizeof(BITMAPINFOHEADER)
    );
         
    //ASSIGN IMAGE INFO DATA
    bool senddelta = false;
    if (senddelta)
    {
        memcpy(
            &buffer[bmpHeader.bfOffBits],
            bufferOne.bits,
            imageDataSize
        );
    }
    else
    {
        memcpy(
            &buffer[bmpHeader.bfOffBits],
            screenCap.bits,
            imageDataSize
        );
    }




    // INIT
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Init Encoder (JPEG)
    CLSID encoderClsid;
    GetEncoderClsid(L"image/jpeg", &encoderClsid);

    // Init buffer and streams
    std::vector<BYTE> buf;
    IStream *stream = NULL;
    CreateStreamOnHGlobal(NULL, FALSE, (LPSTREAM*)&stream);

    // Create Bitmap to be saved to stream
    //Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromBITMAPINFO(&bmpInfo, &screenCap.bits);
    //bitmap->Save(L"C:\\Users\\e5522198\\Desktop\\TestImage.jpg", &encoderClsid, NULL);
    
    //Gdiplus::Bitmap *bmp = GetScreeny(5);
    Gdiplus::Bitmap *bmp = new  Gdiplus::Bitmap(screenCap.hBitmap, (HPALETTE)NULL);

    ULONG quality = 0;

    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].Value = &quality;

    bmp->Save(L"C:\\Users\\e5522198\\Desktop\\TestImage_Quality.jpg", &encoderClsid, NULL);
    bmp->Save(stream, &encoderClsid, NULL);
    delete bmp;

    // Save Bitmap To Stream & delete
    //const Gdiplus::Status saveStatus = bitmap->Save(stream, &encoderClsid, NULL);
    //delete bitmap;

    // SEEK
    const LARGE_INTEGER largeIntegerBeginning = { 0 };
    ULARGE_INTEGER largeIntEnd = { 0 };
    stream->Seek(largeIntegerBeginning, STREAM_SEEK_CUR, &largeIntEnd);
    ULONGLONG pngSize = largeIntEnd.QuadPart;

    // DIRECT ACCESS TO PNG's memory
    HGLOBAL pngInMemory;
    const HRESULT hr = GetHGlobalFromStream(stream, &pngInMemory);

    // COPY TO LOCAL BUFFER
    LPVOID lpPngStreamBytes = GlobalLock(pngInMemory);
    
    char *charBuf = (char*)lpPngStreamBytes;
    //create a vector by copying out the contents of charBuf
    std::vector<char> v(charBuf, charBuf + 360000);

    OnScreenChanged.notify(this, v);


   // OnScreenChanged.notify(this, buffer);

    Gdiplus::GdiplusShutdown(gdiplusToken);

    // BLIT DELTA TO PREV CAPTURE OF FULL SCREEN
    if (!BitBlt(bufferOne.memDc,
        0,
        0,
        targetWidth,
        targetHeight,
        screenCap.memDc,
        0,
        0,
        SRCCOPY
        ))
    {
        return -1;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "Screen Capture: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\n";

    return 1;
}

void ScreenCaptureProcessor::ConvertToJPEG(std::vector<char> buff) {

    /*s
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;

    FILE* outfile = fopen("/tmp/test.jpeg", "wb");
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    */

    /*

    cinfo.image_width = 284;
    cinfo.image_height = 216;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    //set the quality [0..100]
    jpeg_set_quality(&cinfo, 75, true);
    jpeg_start_compress(&cinfo, true);

    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer = (JSAMPROW)&buffer[cinfo.next_scanline*(3 >> 3)*284];
    jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    */
}