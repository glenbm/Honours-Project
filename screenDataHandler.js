// EXTRA
var framesCaptured = 0;
var startTime = 0;
var prevScrData = 0;

var firstFrame = true;

var prevScreen;
var delta
var displayFrameRate = document.getElementById("frame-rate");
var canvas = document.getElementById('canvas1');
var ctx = canvas1.getContext('2d');

var num = 0;

function convertToImageData(bitmap) {
    var width = bitmap.infoheader.biWidth;
    var height = bitmap.infoheader.biHeight;
    var imageData = ctx.createImageData(width, height);

    for (var rowIndex = 0; rowIndex < height; ++rowIndex) {
        for (var columnIndex = 0; columnIndex < width; ++columnIndex) {
            var index1 = (columnIndex + width * (height - rowIndex)) * 4;
            var index2 = columnIndex * 3 + bitmap.stride * rowIndex;

            // bitmaps are little endian (reverse bitmap into imageData)
            imageData.data[index1]          = bitmap.pixels[index2];
            imageData.data[index1 + 1]      = bitmap.pixels[index2+1];
            imageData.data[index1 + 2]      = bitmap.pixels[index2+2];
            imageData.data[index1 + 3]      = 255; // alpha value should always be max
        }
    }

    return imageData;
}

function B64_To_ArrayBuffer(base64) {
    var binary_string = window.atob(base64);
    var len = binary_string.length;
    var bytes = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
        bytes[i] = binary_string.charCodeAt(i);
    }
    return bytes;
}

function ArrayBuffer_To_B64(ab) {

    var dView = new Uint8Array(ab);   //Get a byte view        

    var arr = Array.prototype.slice.call(dView); //Create a normal array        

    var arr1 = arr.map(function (item) {
        return String.fromCharCode(item);    //Convert
    });

    return window.btoa(arr1.join(''));   //Form a string

}

function xor(a, b) {
    if (!Buffer.isBuffer(a)) a = new Buffer(a)
    if (!Buffer.isBuffer(b)) b = new Buffer(b)
    var res = []
    if (a.length > b.length) {
        for (var i = 0; i < b.length; i++) {
            res.push(a[i] ^ b[i])
        }
    } else {
        for (var i = 0; i < a.length; i++) {
            res.push(a[i] ^ b[i])
        }
    }
    return new Buffer(res);
}

function DebugBitmap(bitmap) {
    document.getElementById("bfType").innerHTML = bitmap.fileheader.bfType;
    document.getElementById("bfSize").innerHTML = bitmap.fileheader.bfSize;
    document.getElementById("bfReserved1").innerHTML = bitmap.fileheader.bfReserved1;
    document.getElementById("bfReserved2").innerHTML = bitmap.fileheader.bfReserved2;
    document.getElementById("bfOffBits").innerHTML = bitmap.fileheader.bfOffBits;
    document.getElementById("biSize").innerHTML = bitmap.infoheader.biSize;
    document.getElementById("biWidth").innerHTML = bitmap.infoheader.biWidth;
    document.getElementById("biHeight").innerHTML = bitmap.infoheader.biHeight;
    document.getElementById("biPlanes").innerHTML = bitmap.infoheader.biPlanes;
    document.getElementById("biBitCount").innerHTML = bitmap.infoheader.biBitCount;
    document.getElementById("biCompression").innerHTML = bitmap.infoheader.biCompression;
    document.getElementById("biSizeImage").innerHTML = bitmap.infoheader.biSizeImage;
    document.getElementById("biXPelsPerMeter").innerHTML = bitmap.infoheader.biXPelsPerMeter;
    document.getElementById("biYPelsPerMeter").innerHTML = bitmap.infoheader.biYPelsPerMeter;
    document.getElementById("biClrUsed").innerHTML = bitmap.infoheader.biClrUsed;
    document.getElementById("biClrImportant").innerHTML = bitmap.infoheader.biClrImportant;
}

function GetBitmap(data) {
    var datav = new DataView(data);
    var bitmap = {};

    // Get file header
    bitmap.fileheader = {};
    bitmap.fileheader.bfType = datav.getUint16(0, true);
    bitmap.fileheader.bfSize = datav.getUint32(2, true);
    bitmap.fileheader.bfReserved1 = datav.getUint16(6, true);
    bitmap.fileheader.bfReserved2 = datav.getUint16(8, true);
    bitmap.fileheader.bfOffBits = datav.getUint32(10, true);

    // Get info header from the binary data
    bitmap.infoheader = {};
    bitmap.infoheader.biSize = datav.getUint32(14, true);
    bitmap.infoheader.biWidth = datav.getUint32(18, true);
    bitmap.infoheader.biHeight = datav.getUint32(22, true);
    bitmap.infoheader.biPlanes = datav.getUint16(26, true);
    bitmap.infoheader.biBitCount = datav.getUint16(28, true);
    bitmap.infoheader.biCompression = datav.getUint32(30, true);
    bitmap.infoheader.biSizeImage = datav.getUint32(34, true);
    bitmap.infoheader.biXPelsPerMeter = datav.getUint32(38, true);
    bitmap.infoheader.biYPelsPerMeter = datav.getUint32(42, true);
    bitmap.infoheader.biClrUsed = datav.getUint32(46, true);
    bitmap.infoheader.biClrImportant = datav.getUint32(50, true);

    var start = bitmap.fileheader.bfOffBits;
    bitmap.stride = Math.floor((bitmap.infoheader.biBitCount * bitmap.infoheader.biWidth + 31) / 32) * 4;
    bitmap.pixels = new Uint8Array(data, start);

    var timer2 = performance.now();

    DebugBitmap(bitmap);

    return bitmap;
}

function GetJPG(data) {
    var datav = new DataView(data);
    var jpeg = {};

    jpeg.fileheader = {};
    jpeg.fileheader.marker          = datav.getUint8( 0, true);
    jpeg.fileheader.length          = datav.getUint8( 2, true);
    jpeg.fileheader.iddentifier     = datav.getUint8( 4, true);
    jpeg.fileheader.version         = datav.getUint8( 9, true);
    jpeg.fileheader.densityUnits    = datav.getUint8(11, true);
    jpeg.fileheader.densityX        = datav.getUint8(12, true);
    jpeg.fileheader.densityY        = datav.getUint8(14, true);
    jpeg.fileheader.thumbnailX      = datav.getUint8(16, true);
    jpeg.fileheader.thumbnailY      = datav.getUint8(17, true);

    console.log(jpeg);

    return jpeg;
}

function HandleDeltaCapture(screenData) {
    delta = B64_To_ArrayBuffer(screenData);

    var result = new Uint8Array(delta.length);

    //Copy Header
    for (var i = 0; i < 53; i++) {
        result[i] = delta[i];
        console.log("result at " + i + ": " + result[i]);
    }
    
    //Copy Bitmap Data
    for (var i = 52; i < delta.length; i++) {
        if (delta[i] === 0) {
            result[i] = prevScreen[i];
        } else {
            result[i] = delta[i] & prevScreen[i];
        }
    }

    result = xor(result, prevScreen);

    document.getElementById("cardholder_bg_image").src = 'data:image/bmp;base64, ' + ArrayBuffer_To_B64(result.buffer);
    document.getElementById("cardholder_bg_delta").src = 'data:image/bmp;base64, ' + screenData;
    prevScreen = result;

    return ArrayBuffer_To_B64(result.buffer);
}

function IsTypeBitmap(screenData) {
    var bitmap = GetBitmap(screenData);
    canvas.width = bitmap.infoheader.biWidth;
    canvas.height = bitmap.infoheader.biHeight;

    ctx.putImageData(convertToImageData(bitmap), 0, 0);

    if (framesCaptured === 0) {
        startTime = performance.now();
    }
}

function IsTypeJPG(screenData) {
    GetJPG(screenData);

    var width = 500;
    var height = 500;
    var imageData = ctx.createImageData(width, height);

    for (var rowIndex = 0; rowIndex < height; ++rowIndex) {
        for (var columnIndex = 0; columnIndex < width; ++columnIndex) {
            //var index1 = (columnIndex + width * (height - rowIndex)) * 4;
            //var index2 = columnIndex * 3 + bitmap.stride * rowIndex;

            // bitmaps are little endian (reverse bitmap into imageData)
            imageData.data[index1] = bitmap.pixels[index2];
            imageData.data[index1 + 1] = bitmap.pixels[index2 + 1];
            imageData.data[index1 + 2] = bitmap.pixels[index2 + 2];
            imageData.data[index1 + 3] = 255; // alpha value should always be max
        }
    }

    ctx.putImageData(convertToImageData(bitmap), 0, 0);
}

function draw(imgData) {
    var b64imgData = btoa(imgData); //Binary to ASCII, where it probably stands for
    var img = new Image();
    img.src = "data:image/jpeg;base64," + b64imgData;
    document.getElementById("cardholder_bg_image").src = img; //or append it to something else, just an example
}

function HandleCaptureData(screenData) {
    
    
    //var myArray = new Uint8Array(screenData); //= your data in a UInt8Array
    //var blob = new Blob([myArray], { 'type': 'image/jpg' });
    //var url = URL.createObjectURL(blob); //possibly `webkitURL` or another vendor prefix for old browsers.

    //IsTypeJPG(screenData);

    document.getElementById("cardholder_bg_image").src = 'data:image/jpeg;base64, ' + ArrayBuffer_To_B64(event.data);
    
    //time();

    //var timer2 = performance.now();

    //console.log("time taken: " + (timer2 - timer));
}


var frameCount = 30;
function time() {
    if (framesCaptured === frameCount) {
        var endTime = performance.now();
        var resultTime = (endTime - startTime) / 1000;

        displayFrameRate.innerText = framesCaptured / resultTime;

        framesCaptured = 0;
    }
    else {
        framesCaptured++;
    }
}